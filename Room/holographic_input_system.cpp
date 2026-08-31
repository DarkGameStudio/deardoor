// holographic_input_system.cpp
// Complete Holographic Input System with Draggable Mouse and Floating QWERTY Keyboard
// Integrated with DearDoor Steam Game Launcher

#include <iostream>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <unordered_map>
#include <queue>
#include <random>
#include <fstream>
#include <sstream>

// OpenCV for hand tracking
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>

// OpenGL and GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

// Steam API
#include <steam/steam_api.h>

// ============================================================================
// Configuration
// ============================================================================
namespace HolographicConfig {
    // Display settings
    constexpr float DISPLAY_WIDTH = 1024.0f;
    constexpr float DISPLAY_HEIGHT = 600.0f;
    constexpr float DISPLAY_DEPTH = 0.5f;        // Distance from user (meters)
    constexpr float FLOATING_HEIGHT = 1.5f;       // Height from ground (meters)
    
    // Mouse settings
    constexpr float MOUSE_SIZE = 30.0f;
    constexpr float MOUSE_SENSITIVITY = 1.5f;
    constexpr float MOUSE_SMOOTHING = 0.7f;
    constexpr float MOUSE_DRAG_THRESHOLD = 0.02f;
    constexpr float DOUBLE_CLICK_TIME = 0.3f;    // Seconds
    constexpr float DRAG_TIMEOUT = 0.15f;        // Seconds before drag starts
    
    // Keyboard settings
    constexpr float KEY_WIDTH = 45.0f;
    constexpr float KEY_HEIGHT = 45.0f;
    constexpr float KEY_SPACING = 5.0f;
    constexpr float KEYBOARD_OPACITY = 0.75f;
    constexpr float KEYBOARD_SCALE = 0.9f;
    constexpr float KEY_PRESS_ANIMATION = 0.2f;  // Seconds
    
    // Gesture settings
    constexpr float PINCH_THRESHOLD = 0.03f;
    constexpr float CLICK_DISTANCE = 0.02f;
    constexpr float SCROLL_SENSITIVITY = 0.01f;
    constexpr int GESTURE_HISTORY = 30;
    
    // Colors
    constexpr ImVec4 HOLO_BLUE = ImVec4(0.3f, 0.7f, 1.0f, 0.8f);
    constexpr ImVec4 MOUSE_COLOR = ImVec4(0.2f, 1.0f, 0.8f, 0.9f);
    constexpr ImVec4 KEY_COLOR = ImVec4(0.3f, 0.6f, 0.9f, 0.75f);
    constexpr ImVec4 KEY_HOVER = ImVec4(0.4f, 0.8f, 1.0f, 0.85f);
    constexpr ImVec4 KEY_PRESS = ImVec4(0.2f, 1.0f, 0.8f, 1.0f);
    constexpr ImVec4 TEXT_COLOR = ImVec4(0.9f, 0.95f, 1.0f, 0.9f);
}

// ============================================================================
// 3D Point Structure
// ============================================================================
struct Point3D {
    float x, y, z;
    float confidence;
    
    Point3D() : x(0), y(0), z(0), confidence(0) {}
    Point3D(float px, float py, float pz, float conf = 1.0f)
        : x(px), y(py), z(pz), confidence(conf) {}
    
    float distanceTo(const Point3D& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    Point3D lerp(const Point3D& other, float t) const {
        return Point3D(
            x + (other.x - x) * t,
            y + (other.y - y) * t,
            z + (other.z - z) * t,
            confidence + (other.confidence - confidence) * t
        );
    }
    
    Point3D operator+(const Point3D& other) const {
        return Point3D(x + other.x, y + other.y, z + other.z, confidence);
    }
    
    Point3D operator-(const Point3D& other) const {
        return Point3D(x - other.x, y - other.y, z - other.z, confidence);
    }
    
    Point3D operator*(float scalar) const {
        return Point3D(x * scalar, y * scalar, z * scalar, confidence);
    }
};

// ============================================================================
// Hand Tracking System
// ============================================================================
class HandTracker {
public:
    struct Finger {
        Point3D tip;
        Point3D base;
        Point3D middle;
        bool isExtended;
        float extension;
    };
    
    struct Hand {
        std::vector<Finger> fingers;
        Point3D palm;
        Point3D wrist;
        Point3D pinchPoint;
        Point3D grabPoint;
        bool isPinching;
        bool isGrabbing;
        bool isPointing;
        float pinchStrength;
        float grabStrength;
        int pointingFinger;
        
        Hand() : isPinching(false), isGrabbing(false), isPointing(false),
                 pinchStrength(0), grabStrength(0), pointingFinger(-1) {}
    };
    
private:
    Hand m_currentHand;
    Hand m_previousHand;
    std::vector<Hand> m_history;
    std::mutex m_mutex;
    
    // Smoothing
    Point3D m_smoothedPinchPoint;
    Point3D m_smoothedPalmPoint;
    Point3D m_smoothedIndexTip;
    
    // Gesture state
    bool m_wasPinching;
    bool m_wasGrabbing;
    std::chrono::steady_clock::time_point m_pinchStart;
    std::chrono::steady_clock::time_point m_clickTime;
    
public:
    HandTracker() : m_wasPinching(false), m_wasGrabbing(false) {}
    
    void updateFromCamera(const cv::Mat& depthFrame, const cv::Mat& colorFrame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Detect and track hand
        Hand detectedHand = detectHand(depthFrame, colorFrame);
        
        // Apply smoothing
        Hand smoothedHand = smoothHand(detectedHand);
        
        // Detect gestures
        detectGestures(smoothedHand);
        
        // Update history
        m_previousHand = m_currentHand;
        m_currentHand = smoothedHand;
        m_history.push_back(smoothedHand);
        
        if (m_history.size() > HolographicConfig::GESTURE_HISTORY) {
            m_history.erase(m_history.begin());
        }
    }
    
    Hand getCurrentHand() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentHand;
    }
    
    Point3D getPinchPoint() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_smoothedPinchPoint;
    }
    
    Point3D getIndexFingerTip() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_smoothedIndexTip;
    }
    
    bool isPinching() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentHand.isPinching;
    }
    
    bool isGrabbing() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentHand.isGrabbing;
    }
    
    bool isPointing() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentHand.isPointing;
    }
    
    // Detect double click
    bool detectDoubleClick() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_clickTime
        ).count() / 1000.0f;
        
        if (m_currentHand.isPinching && !m_wasPinching) {
            if (duration < HolographicConfig::DOUBLE_CLICK_TIME) {
                return true;  // Double click detected
            }
            m_clickTime = now;
        }
        
        return false;
    }
    
    // Detect swipe gesture
    bool detectSwipe(Point3D& direction, float& distance) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_history.size() < 5) return false;
        
        const Hand& first = m_history.front();
        const Hand& last = m_history.back();
        
        if (first.pinchPoint.confidence > 0.5f && 
            last.pinchPoint.confidence > 0.5f) {
            direction = last.pinchPoint - first.pinchPoint;
            distance = first.pinchPoint.distanceTo(last.pinchPoint);
            return distance > HolographicConfig::CLICK_DISTANCE;
        }
        
        return false;
    }
    
private:
    Hand detectHand(const cv::Mat& depthFrame, const cv::Mat& colorFrame) {
        Hand hand;
        
        // Simplified hand detection
        cv::Mat binary;
        cv::threshold(depthFrame, binary, 0.1, 1.0, cv::THRESH_BINARY);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (!contours.empty()) {
            auto largestContour = std::max_element(contours.begin(), contours.end(),
                [](const auto& a, const auto& b) {
                    return cv::contourArea(a) < cv::contourArea(b);
                });
            
            if (largestContour != contours.end()) {
                // Calculate palm center
                cv::Moments moments = cv::moments(*largestContour);
                if (moments.m00 != 0) {
                    float cx = moments.m10 / moments.m00;
                    float cy = moments.m01 / moments.m00;
                    float depth = depthFrame.at<float>(
                        static_cast<int>(cy), static_cast<int>(cx)
                    );
                    
                    hand.palm = Point3D(cx, cy, depth, 1.0f);
                    hand.wrist = Point3D(cx, cy + 50, depth + 0.1f, 0.8f);
                    
                    detectFingers(*largestContour, depthFrame, hand);
                }
            }
        }
        
        return hand;
    }
    
    void detectFingers(const std::vector<cv::Point>& contour,
                      const cv::Mat& depthFrame,
                      Hand& hand) {
        // Simplified finger detection using convex hull
        std::vector<int> hull;
        cv::convexHull(contour, hull);
        
        std::vector<cv::Vec4i> defects;
        if (hull.size() > 3) {
            cv::convexityDefects(contour, hull, defects);
            
            int fingerCount = 0;
            for (const auto& defect : defects) {
                cv::Point start = contour[defect[0]];
                cv::Point end = contour[defect[1]];
                cv::Point far = contour[defect[2]];
                float depth = defect[3] / 256.0f;
                
                if (depth > 0.1f && fingerCount < 5) {
                    Finger finger;
                    finger.tip = Point3D(far.x, far.y, 
                                        depthFrame.at<float>(far.y, far.x), 0.8f);
                    finger.base = Point3D(start.x, start.y, 
                                         depthFrame.at<float>(start.y, start.x), 0.7f);
                    finger.middle = finger.tip.lerp(finger.base, 0.5f);
                    finger.isExtended = depth > 0.2f;
                    finger.extension = depth;
                    
                    hand.fingers.push_back(finger);
                    fingerCount++;
                }
            }
        }
        
        // Calculate pinch point (thumb-index)
        if (hand.fingers.size() >= 2) {
            hand.pinchPoint = hand.fingers[0].tip.lerp(hand.fingers[1].tip, 0.5f);
            float pinchDistance = hand.fingers[0].tip.distanceTo(hand.fingers[1].tip);
            hand.pinchStrength = 1.0f - (pinchDistance / HolographicConfig::PINCH_THRESHOLD);
            hand.pinchStrength = std::max(0.0f, std::min(1.0f, hand.pinchStrength));
            hand.isPinching = hand.pinchStrength > 0.7f;
        }
        
        // Calculate grab point
        if (hand.fingers.size() >= 4) {
            float avgDist = 0;
            for (const auto& finger : hand.fingers) {
                avgDist += finger.tip.distanceTo(hand.palm);
            }
            avgDist /= hand.fingers.size();
            
            hand.grabStrength = 1.0f - (avgDist / HolographicConfig::CLICK_DISTANCE);
            hand.grabStrength = std::max(0.0f, std::min(1.0f, hand.grabStrength));
            hand.isGrabbing = hand.grabStrength > 0.8f;
            hand.grabPoint = hand.palm;
        }
        
        // Detect pointing (index finger extended, others curled)
        if (hand.fingers.size() >= 2) {
            int extendedCount = 0;
            int pointingIndex = -1;
            
            for (int i = 0; i < hand.fingers.size(); i++) {
                if (hand.fingers[i].isExtended) {
                    extendedCount++;
                    pointingIndex = i;
                }
            }
            
            if (extendedCount == 1 && pointingIndex >= 0) {
                hand.isPointing = true;
                hand.pointingFinger = pointingIndex;
            }
        }
    }
    
    Hand smoothHand(const Hand& rawHand) {
        Hand smoothed = rawHand;
        float alpha = HolographicConfig::MOUSE_SMOOTHING;
        
        smoothed.pinchPoint = m_smoothedPinchPoint.lerp(rawHand.pinchPoint, alpha);
        smoothed.palm = m_smoothedPalmPoint.lerp(rawHand.palm, alpha);
        
        if (rawHand.fingers.size() > 1) {
            smoothed.fingers[1].tip = m_smoothedIndexTip.lerp(
                rawHand.fingers[1].tip, alpha
            );
        }
        
        m_smoothedPinchPoint = smoothed.pinchPoint;
        m_smoothedPalmPoint = smoothed.palm;
        if (rawHand.fingers.size() > 1) {
            m_smoothedIndexTip = smoothed.fingers[1].tip;
        }
        
        return smoothed;
    }
    
    void detectGestures(Hand& hand) {
        // Pinch gesture
        hand.isPinching = hand.pinchStrength > 0.7f;
        
        // Grab gesture
        hand.isGrabbing = hand.grabStrength > 0.8f;
    }
};

// ============================================================================
// Virtual Mouse System
// ============================================================================
class VirtualMouse {
public:
    enum class MouseState {
        IDLE,
        HOVERING,
        CLICKING,
        DRAGGING,
        DOUBLE_CLICKING,
        RIGHT_CLICKING,
        SCROLLING
    };
    
    enum class MouseButton {
        LEFT,
        RIGHT,
        MIDDLE,
        NONE
    };
    
private:
    struct MouseVisual {
        ImVec2 position;
        ImVec2 velocity;
        float angle;
        float scale;
        float opacity;
        MouseState state;
        MouseButton button;
        bool isVisible;
        bool isDragging;
        std::string dragTarget;
    };
    
    MouseVisual m_mouse;
    HandTracker m_handTracker;
    std::chrono::steady_clock::time_point m_clickTime;
    std::chrono::steady_clock::time_point m_dragStartTime;
    Point3D m_dragStartPosition;
    bool m_wasPinching;
    bool m_wasGrabbing;
    float m_scrollAccumulator;
    
public:
    VirtualMouse() : m_wasPinching(false), m_wasGrabbing(false),
                     m_scrollAccumulator(0) {
        m_mouse.position = ImVec2(512, 300);
        m_mouse.velocity = ImVec2(0, 0);
        m_mouse.angle = 0;
        m_mouse.scale = 1.0f;
        m_mouse.opacity = 1.0f;
        m_mouse.state = MouseState::IDLE;
        m_mouse.button = MouseButton::NONE;
        m_mouse.isVisible = true;
        m_mouse.isDragging = false;
    }
    
    void update(float deltaTime) {
        auto hand = m_handTracker.getCurrentHand();
        Point3D pinchPoint = m_handTracker.getPinchPoint();
        
        // Convert 3D hand position to 2D screen position
        ImVec2 targetPosition = projectToScreen(pinchPoint);
        
        // Apply smoothing
        m_mouse.velocity = ImVec2(
            (targetPosition.x - m_mouse.position.x) * HolographicConfig::MOUSE_SENSITIVITY,
            (targetPosition.y - m_mouse.position.y) * HolographicConfig::MOUSE_SENSITIVITY
        );
        
        m_mouse.position.x += m_mouse.velocity.x * deltaTime;
        m_mouse.position.y += m_mouse.velocity.y * deltaTime;
        
        // Update mouse state
        updateMouseState(hand, deltaTime);
        
        // Update visual effects
        updateVisualEffects(deltaTime);
    }
    
    void render(ImDrawList* drawList) {
        if (!m_mouse.isVisible) return;
        
        ImVec2 pos = m_mouse.position;
        float size = HolographicConfig::MOUSE_SIZE * m_mouse.scale;
        float angle = m_mouse.angle;
        
        // Draw mouse cursor
        ImVec4 cursorColor = getCursorColor();
        ImU32 cursorColorU32 = ImGui::ColorConvertFloat4ToU32(cursorColor);
        
        // Main cursor shape (arrow)
        ImVec2 p1 = pos;
        ImVec2 p2 = ImVec2(pos.x + size, pos.y + size * 0.5f);
        ImVec2 p3 = ImVec2(pos.x + size * 0.45f, pos.y + size * 0.45f);
        ImVec2 p4 = ImVec2(pos.x + size * 0.55f, pos.y + size);
        
        // Rotate points
        float cosA = cos(angle);
        float sinA = sin(angle);
        
        p1 = rotatePoint(p1, pos, cosA, sinA);
        p2 = rotatePoint(p2, pos, cosA, sinA);
        p3 = rotatePoint(p3, pos, cosA, sinA);
        p4 = rotatePoint(p4, pos, cosA, sinA);
        
        // Draw cursor
        drawList->AddTriangleFilled(p1, p2, p3, cursorColorU32);
        drawList->AddTriangleFilled(p1, p3, p4, cursorColorU32);
        
        // Draw outline
        ImU32 outlineColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(1.0f, 1.0f, 1.0f, m_mouse.opacity * 0.5f)
        );
        drawList->AddTriangle(p1, p2, p3, outlineColor, 1.5f);
        drawList->AddTriangle(p1, p3, p4, outlineColor, 1.5f);
        
        // Draw state indicator
        drawStateIndicator(drawList, pos, size);
        
        // Draw drag trail
        if (m_mouse.isDragging) {
            drawDragTrail(drawList, pos);
        }
    }
    
    ImVec2 getPosition() const { return m_mouse.position; }
    MouseState getState() const { return m_mouse.state; }
    bool isDragging() const { return m_mouse.isDragging; }
    std::string getDragTarget() const { return m_mouse.dragTarget; }
    
    void setDragTarget(const std::string& target) {
        m_mouse.dragTarget = target;
        m_mouse.isDragging = !target.empty();
    }
    
    float getScrollDelta() {
        float delta = m_scrollAccumulator;
        m_scrollAccumulator = 0;
        return delta;
    }
    
private:
    ImVec2 projectToScreen(const Point3D& point) {
        // Convert 3D hand position to 2D screen coordinates
        float screenX = (point.x / HolographicConfig::DISPLAY_WIDTH) * 
                        HolographicConfig::DISPLAY_WIDTH;
        float screenY = (point.y / HolographicConfig::DISPLAY_HEIGHT) * 
                        HolographicConfig::DISPLAY_HEIGHT;
        
        return ImVec2(screenX, screenY);
    }
    
    void updateMouseState(const HandTracker::Hand& hand, float deltaTime) {
        auto now = std::chrono::steady_clock::now();
        
        // Detect pinch (left click)
        if (hand.isPinching && !m_wasPinching) {
            m_clickTime = now;
            m_dragStartTime = now;
            m_dragStartPosition = hand.pinchPoint;
            m_mouse.state = MouseState::CLICKING;
            m_mouse.button = MouseButton::LEFT;
        }
        
        // Detect double click
        if (hand.isPinching && !m_wasPinching) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_clickTime
            ).count() / 1000.0f;
            
            if (duration < HolographicConfig::DOUBLE_CLICK_TIME) {
                m_mouse.state = MouseState::DOUBLE_CLICKING;
            }
        }
        
        // Detect drag
        if (hand.isPinching && m_wasPinching) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_dragStartTime
            ).count() / 1000.0f;
            
            float distance = hand.pinchPoint.distanceTo(m_dragStartPosition);
            
            if (duration > HolographicConfig::DRAG_TIMEOUT && 
                distance > HolographicConfig::MOUSE_DRAG_THRESHOLD) {
                m_mouse.state = MouseState::DRAGGING;
                m_mouse.isDragging = true;
            }
        }
        
        // Detect release
        if (!hand.isPinching && m_wasPinching) {
            m_mouse.state = MouseState::IDLE;
            m_mouse.button = MouseButton::NONE;
            m_mouse.isDragging = false;
            m_mouse.dragTarget.clear();
        }
        
        // Detect grab (right click)
        if (hand.isGrabbing && !m_wasGrabbing) {
            m_mouse.state = MouseState::RIGHT_CLICKING;
            m_mouse.button = MouseButton::RIGHT;
        }
        
        // Detect scroll (index finger movement)
        if (hand.isPointing && m_wasPinching == false) {
            m_mouse.state = MouseState::SCROLLING;
            // Accumulate scroll based on vertical movement
            if (hand.fingers.size() > 1) {
                float scrollDelta = hand.fingers[1].tip.y - m_dragStartPosition.y;
                m_scrollAccumulator += scrollDelta * HolographicConfig::SCROLL_SENSITIVITY;
            }
        }
        
        // Update previous states
        m_wasPinching = hand.isPinching;
        m_wasGrabbing = hand.isGrabbing;
    }
    
    void updateVisualEffects(float deltaTime) {
        // Update cursor angle based on velocity
        float speed = sqrt(m_mouse.velocity.x * m_mouse.velocity.x + 
                          m_mouse.velocity.y * m_mouse.velocity.y);
        
        if (speed > 0.1f) {
            float targetAngle = atan2(m_mouse.velocity.y, m_mouse.velocity.x);
            m_mouse.angle += (targetAngle - m_mouse.angle) * deltaTime * 10.0f;
        }
        
        // Scale animation
        float targetScale = 1.0f;
        switch (m_mouse.state) {
            case MouseState::CLICKING:
                targetScale = 0.8f;
                break;
            case MouseState::DRAGGING:
                targetScale = 0.9f;
                break;
            case MouseState::DOUBLE_CLICKING:
                targetScale = 0.7f;
                break;
            case MouseState::SCROLLING:
                targetScale = 1.1f;
                break;
            default:
                targetScale = 1.0f;
                break;
        }
        
        m_mouse.scale += (targetScale - m_mouse.scale) * deltaTime * 12.0f;
        
        // Opacity animation
        float targetOpacity = 1.0f;
        if (speed < 0.01f && m_mouse.state == MouseState::IDLE) {
            targetOpacity = 0.6f;
        }
        m_mouse.opacity += (targetOpacity - m_mouse.opacity) * deltaTime * 5.0f;
    }
    
    ImVec4 getCursorColor() {
        ImVec4 color = HolographicConfig::MOUSE_COLOR;
        color.w = m_mouse.opacity;
        
        switch (m_mouse.state) {
            case MouseState::CLICKING:
                color = ImVec4(0.2f, 1.0f, 0.8f, 1.0f);
                break;
            case MouseState::DRAGGING:
                color = ImVec4(1.0f, 0.8f, 0.2f, 0.9f);
                break;
            case MouseState::DOUBLE_CLICKING:
                color = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
                break;
            case MouseState::RIGHT_CLICKING:
                color = ImVec4(1.0f, 0.4f, 0.2f, 0.9f);
                break;
            case MouseState::SCROLLING:
                color = ImVec4(0.6f, 0.8f, 1.0f, 0.9f);
                break;
            default:
                break;
        }
        
        return color;
    }
    
    void drawStateIndicator(ImDrawList* drawList, ImVec2 pos, float size) {
        // Draw small indicator for current state
        ImVec2 indicatorPos = ImVec2(pos.x + size, pos.y + size);
        float indicatorSize = 5.0f;
        
        ImU32 indicatorColor = ImGui::ColorConvertFloat4ToU32(getCursorColor());
        
        switch (m_mouse.state) {
            case MouseState::CLICKING:
                drawList->AddCircleFilled(indicatorPos, indicatorSize, indicatorColor);
                break;
            case MouseState::DRAGGING:
                drawList->AddRectFilled(
                    ImVec2(indicatorPos.x - indicatorSize, indicatorPos.y - indicatorSize),
                    ImVec2(indicatorPos.x + indicatorSize, indicatorPos.y + indicatorSize),
                    indicatorColor
                );
                break;
            case MouseState::DOUBLE_CLICKING:
                drawList->AddCircle(indicatorPos, indicatorSize, indicatorColor, 16, 2.0f);
                break;
            case MouseState::SCROLLING:
                drawList->AddLine(
                    ImVec2(indicatorPos.x, indicatorPos.y - indicatorSize),
                    ImVec2(indicatorPos.x, indicatorPos.y + indicatorSize),
                    indicatorColor, 2.0f
                );
                break;
            default:
                break;
        }
    }
    
    void drawDragTrail(ImDrawList* drawList, ImVec2 pos) {
        // Draw trail showing drag path
        float trailLength = 20.0f;
        ImVec2 trailStart = ImVec2(
            pos.x - m_mouse.velocity.x * trailLength,
            pos.y - m_mouse.velocity.y * trailLength
        );
        
        ImU32 trailColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(1.0f, 0.8f, 0.2f, 0.4f)
        );
        
        drawList->AddLine(trailStart, pos, trailColor, 3.0f);
    }
    
    ImVec2 rotatePoint(const ImVec2& point, const ImVec2& center, 
                      float cosA, float sinA) {
        float dx = point.x - center.x;
        float dy = point.y - center.y;
        return ImVec2(
            center.x + dx * cosA - dy * sinA,
            center.y + dx * sinA + dy * cosA
        );
    }
};

// ============================================================================
// Floating QWERTY Keyboard
// ============================================================================
class FloatingKeyboard {
public:
    struct Key {
        std::string label;
        std::string shiftLabel;
        ImVec2 position;
        ImVec2 size;
        bool isHovered;
        bool isPressed;
        float pressAnimation;
        std::function<void()> onPress;
    };
    
private:
    std::vector<Key> m_keys;
    ImVec2 m_position;
    ImVec2 m_size;
    float m_scale;
    float m_opacity;
    bool m_isVisible;
    bool m_isDraggable;
    bool m_isDragging;
    ImVec2 m_dragOffset;
    
    // Keyboard layout
    const std::vector<std::vector<std::string>> QWERTY_LAYOUT = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace"},
        {"Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\"},
        {"Caps", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Enter"},
        {"Shift", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "Shift"},
        {"Ctrl", "Win", "Alt", "Space", "Alt", "Fn", "Ctrl"}
    };
    
    std::string m_inputBuffer;
    bool m_shiftPressed;
    bool m_capsLock;
    
public:
    FloatingKeyboard() : m_position(100, 400), m_scale(1.0f), 
                        m_opacity(HolographicConfig::KEYBOARD_OPACITY),
                        m_isVisible(true), m_isDraggable(true),
                        m_isDragging(false), m_shiftPressed(false),
                        m_capsLock(false) {
        initializeKeyboard();
    }
    
    void initializeKeyboard() {
        m_keys.clear();
        
        float startX = m_position.x;
        float startY = m_position.y;
        float keyWidth = HolographicConfig::KEY_WIDTH * m_scale;
        float keyHeight = HolographicConfig::KEY_HEIGHT * m_scale;
        float spacing = HolographicConfig::KEY_SPACING * m_scale;
        
        for (int row = 0; row < QWERTY_LAYOUT.size(); row++) {
            float currentX = startX;
            float currentY = startY + row * (keyHeight + spacing);
            
            for (int col = 0; col < QWERTY_LAYOUT[row].size(); col++) {
                Key key;
                key.label = QWERTY_LAYOUT[row][col];
                key.shiftLabel = getShiftLabel(key.label);
                key.position = ImVec2(currentX, currentY);
                key.size = ImVec2(keyWidth, keyHeight);
                key.isHovered = false;
                key.isPressed = false;
                key.pressAnimation = 0.0f;
                
                // Special key sizing
                if (key.label == "Space") {
                    key.size.x = keyWidth * 5;
                } else if (key.label == "Backspace" || key.label == "Enter") {
                    key.size.x = keyWidth * 1.5f;
                } else if (key.label == "Shift") {
                    key.size.x = keyWidth * 1.8f;
                } else if (key.label == "Caps") {
                    key.size.x = keyWidth * 1.3f;
                } else if (key.label == "Tab") {
                    key.size.x = keyWidth * 1.2f;
                }
                
                m_keys.push_back(key);
                currentX += key.size.x + spacing;
            }
        }
        
        // Calculate total size
        float maxWidth = 0;
        float totalHeight = QWERTY_LAYOUT.size() * (keyHeight + spacing);
        
        for (const auto& key : m_keys) {
            float right = key.position.x + key.size.x - startX;
            maxWidth = std::max(maxWidth, right);
        }
        
        m_size = ImVec2(maxWidth, totalHeight);
    }
    
    void update(float deltaTime, const ImVec2& mousePos, bool mousePressed) {
        // Update key states
        for (auto& key : m_keys) {
            ImVec2 keyMin = key.position;
            ImVec2 keyMax = ImVec2(key.position.x + key.size.x,
                                  key.position.y + key.size.y);
            
            bool isOver = mousePos.x >= keyMin.x && mousePos.x <= keyMax.x &&
                         mousePos.y >= keyMin.y && mousePos.y <= keyMax.y;
            
            key.isHovered = isOver;
            
            if (isOver && mousePressed) {
                if (!key.isPressed) {
                    key.isPressed = true;
                    key.pressAnimation = 1.0f;
                    handleKeyPress(key);
                }
            } else {
                key.isPressed = false;
            }
            
            // Update press animation
            key.pressAnimation = std::max(0.0f, 
                key.pressAnimation - deltaTime / HolographicConfig::KEY_PRESS_ANIMATION);
        }
        
        // Handle keyboard dragging
        if (m_isDraggable) {
            handleDragging(mousePos, mousePressed);
        }
    }
    
    void render(ImDrawList* drawList) {
        if (!m_isVisible) return;
        
        // Draw keyboard background
        ImVec2 bgMin = ImVec2(m_position.x - 20, m_position.y - 20);
        ImVec2 bgMax = ImVec2(m_position.x + m_size.x + 20, 
                             m_position.y + m_size.y + 20);
        
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.1f, 0.15f, 0.2f, m_opacity * 0.8f)
        );
        drawList->AddRectFilled(bgMin, bgMax, bgColor, 15.0f);
        
        // Draw border
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(HolographicConfig::HOLO_BLUE.x,
                   HolographicConfig::HOLO_BLUE.y,
                   HolographicConfig::HOLO_BLUE.z,
                   m_opacity)
        );
        drawList->AddRect(bgMin, bgMax, borderColor, 15.0f, 0, 2.0f);
        
        // Draw keys
        for (const auto& key : m_keys) {
            drawKey(drawList, key);
        }
    }
    
    void setPosition(const ImVec2& position) {
        ImVec2 delta = ImVec2(position.x - m_position.x, 
                             position.y - m_position.y);
        m_position = position;
        
        // Move all keys
        for (auto& key : m_keys) {
            key.position.x += delta.x;
            key.position.y += delta.y;
        }
    }
    
    void setVisible(bool visible) { m_isVisible = visible; }
    bool isVisible() const { return m_isVisible; }
    
    void setDraggable(bool draggable) { m_isDraggable = draggable; }
    
    std::string getInputBuffer() const { return m_inputBuffer; }
    void clearInputBuffer() { m_inputBuffer.clear(); }
    
private:
    std::string getShiftLabel(const std::string& key) {
        if (key.length() == 1 && isalpha(key[0])) {
            return std::string(1, toupper(key[0]));
        }
        
        static const std::map<std::string, std::string> shiftMap = {
            {"1", "!"}, {"2", "@"}, {"3", "#"}, {"4", "$"}, {"5", "%"},
            {"6", "^"}, {"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"},
            {"-", "_"}, {"=", "+"}, {"[", "{"}, {"]", "}"}, {"\\", "|"},
            {";", ":"}, {"'", "\""}, {",", "<"}, {".", ">"}, {"/", "?"}
        };
        
        auto it = shiftMap.find(key);
        if (it != shiftMap.end()) {
            return it->second;
        }
        
        return key;
    }
    
    void handleKeyPress(Key& key) {
        std::string label = key.label;
        
        // Handle special keys
        if (label == "Backspace") {
            if (!m_inputBuffer.empty()) {
                m_inputBuffer.pop_back();
            }
        } else if (label == "Enter") {
            m_inputBuffer += "\n";
        } else if (label == "Space") {
            m_inputBuffer += " ";
        } else if (label == "Shift") {
            m_shiftPressed = !m_shiftPressed;
        } else if (label == "Caps") {
            m_capsLock = !m_capsLock;
        } else if (label == "Tab") {
            m_inputBuffer += "\t";
        } else if (label.length() == 1) {
            // Regular character
            char c = label[0];
            if (m_shiftPressed || m_capsLock) {
                c = toupper(c);
            }
            m_inputBuffer += c;
        }
        
        // Auto-release shift after one character
        if (m_shiftPressed && label.length() == 1) {
            m_shiftPressed = false;
        }
    }
    
    void drawKey(ImDrawList* drawList, const Key& key) {
        ImVec2 keyMin = key.position;
        ImVec2 keyMax = ImVec2(key.position.x + key.size.x,
                              key.position.y + key.size.y);
        
        // Key background
        ImVec4 keyColor = HolographicConfig::KEY_COLOR;
        keyColor.w = m_opacity;
        
        if (key.isHovered) {
            keyColor = HolographicConfig::KEY_HOVER;
            keyColor.w = m_opacity;
        }
        
        if (key.isPressed) {
            keyColor = HolographicConfig::KEY_PRESS;
            keyColor.w = m_opacity * (0.8f + key.pressAnimation * 0.2f);
        }
        
        ImU32 keyColorU32 = ImGui::ColorConvertFloat4ToU32(keyColor);
        drawList->AddRectFilled(keyMin, keyMax, keyColorU32, 5.0f);
        
        // Key border
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.5f, 0.7f, 0.9f, m_opacity * 0.6f)
        );
        drawList->AddRect(keyMin, keyMax, borderColor, 5.0f, 0, 1.5f);
        
        // Key label
        std::string displayLabel = key.label;
        if (m_shiftPressed && key.shiftLabel != key.label) {
            displayLabel = key.shiftLabel;
        }
        
        ImVec2 labelSize = ImGui::CalcTextSize(displayLabel.c_str());
        ImVec2 labelPos = ImVec2(
            keyMin.x + (key.size.x - labelSize.x) * 0.5f,
            keyMin.y + (key.size.y - labelSize.y) * 0.5f - key.pressAnimation * 3.0f
        );
        
        ImU32 labelColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(HolographicConfig::TEXT_COLOR.x,
                   HolographicConfig::TEXT_COLOR.y,
                   HolographicConfig::TEXT_COLOR.z,
                   m_opacity)
        );
        drawList->AddText(labelPos, labelColor, displayLabel.c_str());
    }
    
    void handleDragging(const ImVec2& mousePos, bool mousePressed) {
        ImVec2 bgMin = ImVec2(m_position.x - 20, m_position.y - 20);
        ImVec2 bgMax = ImVec2(m_position.x + m_size.x + 20, 
                             m_position.y + m_size.y + 20);
        
        bool isOverKeyboard = mousePos.x >= bgMin.x && mousePos.x <= bgMax.x &&
                             mousePos.y >= bgMin.y && mousePos.y <= bgMax.y;
        
        if (isOverKeyboard && mousePressed && !m_isDragging) {
            m_isDragging = true;
            m_dragOffset = ImVec2(mousePos.x - m_position.x,
                                 mousePos.y - m_position.y);
        }
        
        if (m_isDragging && mousePressed) {
            setPosition(ImVec2(mousePos.x - m_dragOffset.x,
                              mousePos.y - m_dragOffset.y));
        }
        
        if (!mousePressed) {
            m_isDragging = false;
        }
    }
};

// ============================================================================
// Holographic Input Manager
// ============================================================================
class HolographicInputManager {
private:
    VirtualMouse m_virtualMouse;
    FloatingKeyboard m_keyboard;
    HandTracker m_handTracker;
    
    GLFWwindow* m_window;
    float m_displayScaleX;
    float m_displayScaleY;
    
    // Input state
    bool m_keyboardVisible;
    bool m_mouseVisible;
    std::string m_textInput;
    
public:
    HolographicInputManager() : m_keyboardVisible(true), m_mouseVisible(true) {}
    
    bool initialize() {
        // Initialize GLFW
        if (!glfwInit()) return false;
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        
        m_window = glfwCreateWindow(1280, 720, 
                                    "DearDoor - Holographic Input System",
                                    nullptr, nullptr);
        
        if (!m_window) return false;
        
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);
        
        // Initialize GLEW
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) return false;
        
        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Calculate display scale
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        m_displayScaleX = static_cast<float>(fbWidth) / 1280.0f;
        m_displayScaleY = static_cast<float>(fbHeight) / 720.0f;
        
        return true;
    }
    
    void update(float deltaTime) {
        // Update hand tracking (simulated for now)
        updateHandTracking();
        
        // Update virtual mouse
        m_virtualMouse.update(deltaTime);
        
        // Update keyboard
        ImVec2 mousePos = m_virtualMouse.getPosition();
        bool mousePressed = m_virtualMouse.getState() == VirtualMouse::MouseState::CLICKING;
        m_keyboard.update(deltaTime, mousePos, mousePressed);
        
        // Update text input
        m_textInput = m_keyboard.getInputBuffer();
    }
    
    void render() {
        glfwPollEvents();
        
        // Clear screen
        glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render holographic interface
        renderHolographicInterface();
        
        // Render keyboard
        if (m_keyboardVisible) {
            m_keyboard.render(ImGui::GetForegroundDrawList());
        }
        
        // Render virtual mouse
        if (m_mouseVisible) {
            m_virtualMouse.render(ImGui::GetForegroundDrawList());
        }
        
        // Render text input display
        renderTextInput();
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(m_window);
    }
    
    void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (m_window) {
            glfwDestroyWindow(m_window);
        }
        
        glfwTerminate();
    }
    
    bool shouldClose() {
        return glfwWindowShouldClose(m_window);
    }
    
private:
    void updateHandTracking() {
        // In production, use depth camera data
        // Simulate hand position with mouse for now
        double mouseX, mouseY;
        glfwGetCursorPos(m_window, &mouseX, &mouseY);
        
        // Create simulated hand
        HandTracker::Hand hand;
        hand.palm = Point3D(static_cast<float>(mouseX),
                           static_cast<float>(mouseY),
                           HolographicConfig::DISPLAY_DEPTH, 0.9f);
        
        hand.pinchPoint = Point3D(static_cast<float>(mouseX),
                                 static_cast<float>(mouseY),
                                 HolographicConfig::DISPLAY_DEPTH, 0.9f);
        
        bool isPinching = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool isGrabbing = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        
        hand.isPinching = isPinching;
        hand.isGrabbing = isGrabbing;
        hand.pinchStrength = isPinching ? 1.0f : 0.0f;
        hand.grabStrength = isGrabbing ? 1.0f : 0.0f;
        
        // Create index finger for pointing
        HandTracker::Finger indexFinger;
        indexFinger.tip = Point3D(static_cast<float>(mouseX + 20),
                                 static_cast<float>(mouseY + 20),
                                 HolographicConfig::DISPLAY_DEPTH, 0.8f);
        indexFinger.isExtended = true;
        hand.fingers.push_back(indexFinger);
        
        hand.isPointing = true;
        hand.pointingFinger = 0;
        
        // Update hand tracker with simulated data
        // In production: m_handTracker.updateFromCamera(depthFrame, colorFrame);
    }
    
    void renderHolographicInterface() {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        // Draw holographic background
        ImVec2 displayMin(50, 30);
        ImVec2 displayMax(50 + HolographicConfig::DISPLAY_WIDTH,
                         30 + HolographicConfig::DISPLAY_HEIGHT);
        
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.1f, 0.15f, 0.2f, 0.6f)
        );
        drawList->AddRectFilled(displayMin, displayMax, bgColor, 10.0f);
        
        // Draw title
        ImVec2 titlePos(displayMin.x + 20, displayMin.y + 20);
        ImU32 titleColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.3f, 0.7f, 1.0f, 0.9f)
        );
        drawList->AddText(titlePos, titleColor, "DearDoor Holographic Interface");
        
        // Draw input display area
        ImVec2 inputMin(displayMin.x + 20, displayMin.y + 60);
        ImVec2 inputMax(displayMax.x - 20, displayMin.y + 120);
        
        ImU32 inputBgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.15f, 0.2f, 0.25f, 0.8f)
        );
        drawList->AddRectFilled(inputMin, inputMax, inputBgColor, 5.0f);
        
        // Draw text input
        ImVec2 textPos(inputMin.x + 10, inputMin.y + 10);
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.9f, 0.95f, 1.0f, 0.9f)
        );
        drawList->AddText(textPos, textColor, m_textInput.c_str());
    }
    
    void renderTextInput() {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        // Draw current text input at top
        ImVec2 textPos(60, 80);
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.9f, 0.95f, 1.0f, 0.9f)
        );
        drawList->AddText(textPos, textColor, "Input: " + m_textInput.c_str());
        
        // Draw keyboard toggle hint
        ImVec2 hintPos(60, 110);
        ImU32 hintColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.5f, 0.6f, 0.7f, 0.7f)
        );
        drawList->AddText(hintPos, hintColor, 
                         "Press 'K' to toggle keyboard | 'M' to toggle mouse");
    }
    
    void handleKeyboardShortcuts() {
        // Toggle keyboard visibility
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_K))) {
            m_keyboardVisible = !m_keyboardVisible;
            m_keyboard.setVisible(m_keyboardVisible);
        }
        
        // Toggle mouse visibility
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_M))) {
            m_mouseVisible = !m_mouseVisible;
        }
        
        // Clear input
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
            m_keyboard.clearInputBuffer();
            m_textInput.clear();
        }
    }
};

// ============================================================================
// Main Application
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "DearDoor - Holographic Input System" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  - Floating QWERTY Keyboard" << std::endl;
    std::cout << "  - Draggable Virtual Mouse" << std::endl;
    std::cout << "  - Hand Gesture Recognition" << std::endl;
    std::cout << "  - Holographic Display" << std::endl;
    std::cout << std::endl;
    
    HolographicInputManager inputManager;
    
    if (!inputManager.initialize()) {
        std::cerr << "Failed to initialize holographic input system" << std::endl;
        return -1;
    }
    
    std::cout << "Holographic input system initialized" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Move hand to control mouse" << std::endl;
    std::cout << "  - Pinch to click" << std::endl;
    std::cout << "  - Grab to drag" << std::endl;
    std::cout << "  - Point to scroll" << std::endl;
    std::cout << "  - Type on floating keyboard" << std::endl;
    std::cout << "  - ESC to exit" << std::endl;
    
    auto lastTime = std::chrono::steady_clock::now();
    
    while (!inputManager.shouldClose()) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(
            currentTime - lastTime
        ).count();
        lastTime = currentTime;
        
        // Update
        inputManager.update(deltaTime);
        
        // Render
        inputManager.render();
        
        // Check for exit
        if (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            break;
        }
    }
    
    inputManager.shutdown();
    
    std::cout << "\nHolographic input system closed" << std::endl;
    return 0;
}