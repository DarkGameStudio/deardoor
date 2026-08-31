// floating_touch_display.cpp
// Holographic Floating Touch Screen Interface for DearDoor
// Uses computer vision, depth sensing, and gesture recognition

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

// OpenCV for computer vision and gesture tracking
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/video.hpp>

// OpenGL for holographic rendering
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Dear ImGui for UI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// RealSense or similar depth camera SDK
#ifdef USE_REALSENSE
#include <librealsense2/rs.hpp>
#endif

// MediaPipe for hand tracking (alternative)
#ifdef USE_MEDIAPIPE
#include <mediapipe/framework/calculator_framework.h>
#include <mediapipe/framework/port/status.h>
#endif

// Steam integration
#include <steam/steam_api.h>

// ============================================================================
// Configuration
// ============================================================================
namespace HolographicConfig {
    // Display settings
    constexpr float DISPLAY_WIDTH = 800.0f;      // Virtual display width
    constexpr float DISPLAY_HEIGHT = 500.0f;     // Virtual display height
    constexpr float DISPLAY_DEPTH = 0.5f;        // Distance from user (meters)
    constexpr float FLOATING_HEIGHT = 1.5f;      // Height from ground (meters)
    
    // Touch interaction
    constexpr float TOUCH_SENSITIVITY = 0.02f;   // Meters of touch detection
    constexpr float GESTURE_SMOOTHING = 0.7f;    // Smoothing factor (0-1)
    constexpr float MIN_SWIPE_DISTANCE = 0.1f;   // Minimum swipe distance
    constexpr float TAP_TIMEOUT = 0.3f;          // Seconds for tap detection
    
    // Visual effects
    constexpr float HOLOGRAM_OPACITY = 0.7f;     // Hologram transparency
    constexpr float GLOW_INTENSITY = 0.8f;       // Glow effect strength
    constexpr float SCANLINE_DENSITY = 2.0f;     // Scanline effect density
    constexpr float FLICKER_AMPLITUDE = 0.05f;   // Hologram flicker amount
    
    // Colors (holographic blue theme)
    constexpr ImVec4 HOLO_COLOR = ImVec4(0.3f, 0.7f, 1.0f, 0.7f);
    constexpr ImVec4 TOUCH_COLOR = ImVec4(0.0f, 1.0f, 0.8f, 0.9f);
    constexpr ImVec4 HOVER_COLOR = ImVec4(0.5f, 0.8f, 1.0f, 0.8f);
    constexpr ImVec4 ACTIVE_COLOR = ImVec4(0.2f, 1.0f, 0.9f, 1.0f);
    
    // Gesture recognition
    constexpr int GESTURE_HISTORY_SIZE = 30;
    constexpr float PINCH_THRESHOLD = 0.03f;     // Pinch detection threshold
    constexpr float GRAB_THRESHOLD = 0.05f;      // Grab gesture threshold
}

// ============================================================================
// 3D Point Structure for Touch Tracking
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
};

// ============================================================================
// Hand Tracking System
// ============================================================================
class HandTracker {
public:
    struct Hand {
        std::vector<Point3D> fingers;       // Finger tip positions
        Point3D palm;                       // Palm center
        Point3D pinchPoint;                 // Pinch position (thumb-index)
        bool isPinching;
        bool isGrabbing;
        float pinchStrength;
        
        Hand() : isPinching(false), isGrabbing(false), pinchStrength(0) {}
    };
    
    struct HandHistory {
        std::vector<Hand> frames;
        int maxFrames;
        
        HandHistory(int max = HolographicConfig::GESTURE_HISTORY_SIZE)
            : maxFrames(max) {}
        
        void addFrame(const Hand& hand) {
            frames.push_back(hand);
            if (frames.size() > maxFrames) {
                frames.erase(frames.begin());
            }
        }
        
        void clear() { frames.clear(); }
    };
    
private:
    Hand m_currentHand;
    HandHistory m_history;
    std::mutex m_handMutex;
    
    // Smoothing filters
    Point3D m_smoothedPinchPoint;
    Point3D m_smoothedPalmPoint;
    
    // Gesture detection
    bool m_previousPinchState;
    std::chrono::steady_clock::time_point m_pinchStartTime;
    
public:
    HandTracker() : m_previousPinchState(false) {
        m_history = HandHistory(HolographicConfig::GESTURE_HISTORY_SIZE);
    }
    
    void updateFromDepthFrame(const cv::Mat& depthFrame,
                             const cv::Mat& colorFrame) {
        std::lock_guard<std::mutex> lock(m_handMutex);
        
        // Detect hand in frame (simplified)
        Hand detectedHand = detectHand(depthFrame, colorFrame);
        
        // Apply smoothing
        if (detectedHand.fingers.size() > 0) {
            m_currentHand = smoothHand(detectedHand);
            detectGestures(m_currentHand);
            m_history.addFrame(m_currentHand);
        }
    }
    
    Hand getCurrentHand() {
        std::lock_guard<std::mutex> lock(m_handMutex);
        return m_currentHand;
    }
    
    Point3D getSmoothedPinchPoint() {
        std::lock_guard<std::mutex> lock(m_handMutex);
        return m_smoothedPinchPoint;
    }
    
    bool isPinching() {
        std::lock_guard<std::mutex> lock(m_handMutex);
        return m_currentHand.isPinching;
    }
    
    bool isGrabbing() {
        std::lock_guard<std::mutex> lock(m_handMutex);
        return m_currentHand.isGrabbing;
    }
    
    // Detect swipe gesture
    bool detectSwipe(Point3D& swipeDirection, float& swipeDistance) {
        std::lock_guard<std::mutex> lock(m_handMutex);
        
        if (m_history.frames.size() < 2) return false;
        
        const Hand& first = m_history.frames.front();
        const Hand& last = m_history.frames.back();
        
        if (first.pinchPoint.confidence > 0.5f && 
            last.pinchPoint.confidence > 0.5f) {
            swipeDirection = Point3D(
                last.pinchPoint.x - first.pinchPoint.x,
                last.pinchPoint.y - first.pinchPoint.y,
                last.pinchPoint.z - first.pinchPoint.z
            );
            
            swipeDistance = first.pinchPoint.distanceTo(last.pinchPoint);
            
            return swipeDistance > HolographicConfig::MIN_SWIPE_DISTANCE;
        }
        
        return false;
    }
    
    // Detect tap gesture
    bool detectTap() {
        std::lock_guard<std::mutex> lock(m_handMutex);
        
        if (m_history.frames.size() < 5) return false;
        
        // Check for quick pinch-pinch gesture
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_pinchStartTime
        ).count() / 1000.0f;
        
        if (m_currentHand.isPinching && !m_previousPinchState) {
            m_pinchStartTime = now;
            return false;
        }
        
        if (!m_currentHand.isPinching && m_previousPinchState) {
            if (duration < HolographicConfig::TAP_TIMEOUT) {
                return true;  // Quick tap detected
            }
        }
        
        m_previousPinchState = m_currentHand.isPinching;
        return false;
    }
    
private:
    Hand detectHand(const cv::Mat& depthFrame, const cv::Mat& colorFrame) {
        Hand hand;
        
        // Simplified hand detection
        // In production, use MediaPipe or RealSense SDK
        cv::Mat binary;
        cv::threshold(depthFrame, binary, 0.1, 1.0, cv::THRESH_BINARY);
        
        // Find contours (potential hand)
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (!contours.empty()) {
            // Find largest contour (likely hand)
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
                    
                    // Detect fingertips (simplified)
                    detectFingertips(*largestContour, depthFrame, hand);
                }
            }
        }
        
        return hand;
    }
    
    void detectFingertips(const std::vector<cv::Point>& contour,
                         const cv::Mat& depthFrame,
                         Hand& hand) {
        // Simplified fingertip detection
        // Use convex hull and convexity defects
        std::vector<int> hull;
        cv::convexHull(contour, hull);
        
        std::vector<cv::Vec4i> defects;
        if (hull.size() > 3) {
            cv::convexityDefects(contour, hull, defects);
            
            // Filter defects to find fingertips
            int fingerCount = 0;
            for (const auto& defect : defects) {
                cv::Point start = contour[defect[0]];
                cv::Point end = contour[defect[1]];
                cv::Point far = contour[defect[2]];
                float depth = defect[3] / 256.0f;
                
                if (depth > 0.1f && fingerCount < 5) {
                    float fx = far.x;
                    float fy = far.y;
                    float fz = depthFrame.at<float>(fy, fx);
                    
                    hand.fingers.push_back(Point3D(fx, fy, fz, 0.8f));
                    fingerCount++;
                }
            }
        }
        
        // Calculate pinch point (thumb-index intersection)
        if (hand.fingers.size() >= 2) {
            hand.pinchPoint = hand.fingers[0].lerp(hand.fingers[1], 0.5f);
            hand.pinchStrength = 1.0f - hand.fingers[0].distanceTo(hand.fingers[1]) / 
                                 HolographicConfig::PINCH_THRESHOLD;
            hand.pinchStrength = std::max(0.0f, std::min(1.0f, hand.pinchStrength));
        }
    }
    
    Hand smoothHand(const Hand& rawHand) {
        Hand smoothed = rawHand;
        
        // Apply exponential smoothing
        float alpha = HolographicConfig::GESTURE_SMOOTHING;
        
        smoothed.pinchPoint = m_smoothedPinchPoint.lerp(rawHand.pinchPoint, alpha);
        smoothed.palm = m_smoothedPalmPoint.lerp(rawHand.palm, alpha);
        
        // Update smoothed points
        m_smoothedPinchPoint = smoothed.pinchPoint;
        m_smoothedPalmPoint = smoothed.palm;
        
        return smoothed;
    }
    
    void detectGestures(Hand& hand) {
        // Detect pinch gesture
        hand.isPinching = hand.pinchStrength > 0.7f;
        
        // Detect grab gesture (all fingers close to palm)
        if (hand.fingers.size() >= 4) {
            float avgDistance = 0;
            for (const auto& finger : hand.fingers) {
                avgDistance += finger.distanceTo(hand.palm);
            }
            avgDistance /= hand.fingers.size();
            
            hand.isGrabbing = avgDistance < HolographicConfig::GRAB_THRESHOLD;
        }
    }
};

// ============================================================================
// Holographic Display Renderer
// ============================================================================
class HolographicDisplay {
private:
    struct HolographicElement {
        std::string id;
        ImVec2 position;
        ImVec2 size;
        std::string label;
        std::function<void()> onClick;
        bool isHovered;
        bool isPressed;
        float hoverAlpha;
        float pressAlpha;
    };
    
    std::vector<HolographicElement> m_elements;
    GLFWwindow* m_window;
    HandTracker m_handTracker;
    
    // Holographic effects
    float m_hologramTime;
    float m_flickerPhase;
    ImVec2 m_displayPosition;
    ImVec2 m_displaySize;
    
    // Touch mapping
    std::map<std::string, ImVec2> m_touchPositions;
    
public:
    HolographicDisplay() : m_hologramTime(0), m_flickerPhase(0) {
        m_displayPosition = ImVec2(100, 100);
        m_displaySize = ImVec2(
            HolographicConfig::DISPLAY_WIDTH,
            HolographicConfig::DISPLAY_HEIGHT
        );
    }
    
    bool initialize() {
        // Initialize GLFW
        if (!glfwInit()) return false;
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        
        m_window = glfwCreateWindow(
            1280, 720,
            "DearDoor - Holographic Display",
            nullptr, nullptr
        );
        
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
        
        return true;
    }
    
    void addElement(const std::string& id,
                   const ImVec2& position,
                   const ImVec2& size,
                   const std::string& label,
                   std::function<void()> onClick) {
        HolographicElement element;
        element.id = id;
        element.position = position;
        element.size = size;
        element.label = label;
        element.onClick = onClick;
        element.isHovered = false;
        element.isPressed = false;
        element.hoverAlpha = 0.0f;
        element.pressAlpha = 0.0f;
        m_elements.push_back(element);
    }
    
    void update(float deltaTime) {
        m_hologramTime += deltaTime;
        m_flickerPhase += deltaTime * 2.0f;
        
        // Update hand tracking
        updateHandTracking();
        
        // Update touch interactions
        updateTouchInteractions();
        
        // Update element states
        for (auto& element : m_elements) {
            // Smooth hover transitions
            float targetHover = element.isHovered ? 1.0f : 0.0f;
            element.hoverAlpha += (targetHover - element.hoverAlpha) * deltaTime * 10.0f;
            
            float targetPress = element.isPressed ? 1.0f : 0.0f;
            element.pressAlpha += (targetPress - element.pressAlpha) * deltaTime * 15.0f;
        }
    }
    
    void render() {
        glfwPollEvents();
        
        // Clear with transparent background
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Render holographic display
        renderHolographicDisplay();
        
        // Render hand cursor
        renderHandCursor();
        
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
        // In production, this would use depth camera data
        // For now, simulate hand tracking with mouse
        double mouseX, mouseY;
        glfwGetCursorPos(m_window, &mouseX, &mouseY);
        
        // Convert mouse to 3D point (simulated)
        Point3D simulatedHand(
            static_cast<float>(mouseX),
            static_cast<float>(mouseY),
            HolographicConfig::DISPLAY_DEPTH,
            0.9f
        );
        
        // Simulate pinch with mouse click
        bool isPinching = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        
        // Update hand tracker with simulated data
        // In production: m_handTracker.updateFromDepthFrame(depthFrame, colorFrame);
        HandTracker::Hand hand;
        hand.pinchPoint = simulatedHand;
        hand.palm = simulatedHand;
        hand.isPinching = isPinching;
        hand.pinchStrength = isPinching ? 1.0f : 0.0f;
        
        // Store for interaction
        m_simulatedHand = hand;
    }
    
    void updateTouchInteractions() {
        HandTracker::Hand hand = m_simulatedHand;
        
        // Convert hand position to display coordinates
        ImVec2 handPos = ImVec2(hand.pinchPoint.x, hand.pinchPoint.y);
        
        // Check touch on elements
        for (auto& element : m_elements) {
            ImVec2 elementMin = ImVec2(
                m_displayPosition.x + element.position.x,
                m_displayPosition.y + element.position.y
            );
            ImVec2 elementMax = ImVec2(
                elementMin.x + element.size.x,
                elementMin.y + element.size.y
            );
            
            // Check if hand is over element
            bool isOver = handPos.x >= elementMin.x && 
                         handPos.x <= elementMax.x &&
                         handPos.y >= elementMin.y && 
                         handPos.y <= elementMax.y;
            
            element.isHovered = isOver;
            
            // Handle touch/press
            if (isOver && hand.isPinching) {
                if (!element.isPressed) {
                    element.isPressed = true;
                    if (element.onClick) {
                        element.onClick();
                    }
                }
            } else {
                element.isPressed = false;
            }
        }
    }
    
    void renderHolographicDisplay() {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        // Apply holographic effects
        float flicker = std::sin(m_flickerPhase) * HolographicConfig::FLICKER_AMPLITUDE;
        float opacity = HolographicConfig::HOLOGRAM_OPACITY + flicker;
        
        // Draw display background
        ImVec2 displayMin = m_displayPosition;
        ImVec2 displayMax = ImVec2(
            m_displayPosition.x + m_displaySize.x,
            m_displayPosition.y + m_displaySize.y
        );
        
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.1f, 0.2f, 0.3f, opacity * 0.5f)
        );
        drawList->AddRectFilled(displayMin, displayMax, bgColor, 10.0f);
        
        // Draw border glow
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(HolographicConfig::HOLO_COLOR.x,
                   HolographicConfig::HOLO_COLOR.y,
                   HolographicConfig::HOLO_COLOR.z,
                   opacity)
        );
        drawList->AddRect(displayMin, displayMax, borderColor, 10.0f, 0, 3.0f);
        
        // Draw scanlines
        drawScanlines(drawList, displayMin, displayMax, opacity);
        
        // Draw elements
        for (const auto& element : m_elements) {
            renderHolographicElement(drawList, element, opacity);
        }
        
        // Draw title
        ImVec2 titlePos = ImVec2(
            m_displayPosition.x + m_displaySize.x * 0.5f - 50,
            m_displayPosition.y + 20
        );
        ImU32 titleColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(HolographicConfig::HOLO_COLOR.x,
                   HolographicConfig::HOLO_COLOR.y,
                   HolographicConfig::HOLO_COLOR.z,
                   opacity)
        );
        drawList->AddText(titlePos, titleColor, "DearDoor Portal");
    }
    
    void renderHolographicElement(ImDrawList* drawList,
                                 const HolographicElement& element,
                                 float opacity) {
        ImVec2 elementMin = ImVec2(
            m_displayPosition.x + element.position.x,
            m_displayPosition.y + element.position.y
        );
        ImVec2 elementMax = ImVec2(
            elementMin.x + element.size.x,
            elementMin.y + element.size.y
        );
        
        // Element background with hover effect
        ImVec4 bgColor = HolographicConfig::HOLO_COLOR;
        bgColor.w = opacity * (0.3f + element.hoverAlpha * 0.3f);
        
        if (element.isPressed) {
            bgColor = HolographicConfig::ACTIVE_COLOR;
            bgColor.w = opacity * (0.5f + element.pressAlpha * 0.3f);
        }
        
        ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
        drawList->AddRectFilled(elementMin, elementMax, bgColorU32, 5.0f);
        
        // Element border
        ImVec4 borderColor = HolographicConfig::HOLO_COLOR;
        borderColor.w = opacity * (0.5f + element.hoverAlpha * 0.3f);
        
        if (element.isPressed) {
            borderColor = HolographicConfig::ACTIVE_COLOR;
            borderColor.w = opacity * (0.7f + element.pressAlpha * 0.3f);
        }
        
        ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(borderColor);
        drawList->AddRect(elementMin, elementMax, borderColorU32, 5.0f, 0, 2.0f);
        
        // Element label
        ImVec2 labelPos = ImVec2(
            elementMin.x + element.size.x * 0.5f - 
                ImGui::CalcTextSize(element.label.c_str()).x * 0.5f,
            elementMin.y + element.size.y * 0.5f - 5
        );
        
        ImU32 labelColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(1.0f, 1.0f, 1.0f, opacity * (0.8f + element.hoverAlpha * 0.2f))
        );
        drawList->AddText(labelPos, labelColor, element.label.c_str());
    }
    
    void drawScanlines(ImDrawList* drawList,
                      const ImVec2& displayMin,
                      const ImVec2& displayMax,
                      float opacity) {
        float scanlineSpacing = 10.0f / HolographicConfig::SCANLINE_DENSITY;
        
        ImU32 scanlineColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.5f, 0.8f, 1.0f, opacity * 0.1f)
        );
        
        for (float y = displayMin.y; y < displayMax.y; y += scanlineSpacing) {
            drawList->AddLine(
                ImVec2(displayMin.x, y),
                ImVec2(displayMax.x, y),
                scanlineColor,
                1.0f
            );
        }
    }
    
    void renderHandCursor() {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        
        HandTracker::Hand hand = m_simulatedHand;
        ImVec2 cursorPos = ImVec2(hand.pinchPoint.x, hand.pinchPoint.y);
        
        // Draw cursor
        float cursorRadius = 10.0f + hand.pinchStrength * 5.0f;
        
        ImVec4 cursorColor = HolographicConfig::TOUCH_COLOR;
        cursorColor.w = 0.7f;
        
        if (hand.isPinching) {
            cursorColor = HolographicConfig::ACTIVE_COLOR;
            cursorColor.w = 1.0f;
        }
        
        ImU32 cursorColorU32 = ImGui::ColorConvertFloat4ToU32(cursorColor);
        
        // Outer ring
        drawList->AddCircle(cursorPos, cursorRadius, cursorColorU32, 32, 2.0f);
        
        // Inner dot
        drawList->AddCircleFilled(cursorPos, 3.0f, cursorColorU32);
        
        // Draw pinch indicator
        if (hand.isPinching) {
            drawList->AddCircle(
                cursorPos,
                cursorRadius * 1.5f,
                cursorColorU32,
                32,
                1.0f
            );
        }
    }
    
    HandTracker::Hand m_simulatedHand;
};

// ============================================================================
// Gesture Command Handler for DearDoor
// ============================================================================
class GestureCommandHandler {
private:
    HolographicDisplay m_display;
    HandTracker m_handTracker;
    
    // Gesture patterns
    enum class GestureType {
        NONE,
        TAP,
        SWIPE_LEFT,
        SWIPE_RIGHT,
        SWIPE_UP,
        SWIPE_DOWN,
        PINCH,
        GRAB,
        CIRCLE,
        WAVE
    };
    
    std::map<GestureType, std::function<void()>> m_gestureCommands;
    
public:
    GestureCommandHandler() {
        initializeCommands();
    }
    
    void processGestures() {
        // Detect swipe gestures
        Point3D swipeDirection;
        float swipeDistance;
        
        if (m_handTracker.detectSwipe(swipeDirection, swipeDistance)) {
            // Determine swipe direction
            if (std::abs(swipeDirection.x) > std::abs(swipeDirection.y)) {
                if (swipeDirection.x > 0) {
                    executeGesture(GestureType::SWIPE_RIGHT);
                } else {
                    executeGesture(GestureType::SWIPE_LEFT);
                }
            } else {
                if (swipeDirection.y > 0) {
                    executeGesture(GestureType::SWIPE_DOWN);
                } else {
                    executeGesture(GestureType::SWIPE_UP);
                }
            }
        }
        
        // Detect tap
        if (m_handTracker.detectTap()) {
            executeGesture(GestureType::TAP);
        }
        
        // Detect pinch
        if (m_handTracker.isPinching()) {
            executeGesture(GestureType::PINCH);
        }
        
        // Detect grab
        if (m_handTracker.isGrabbing()) {
            executeGesture(GestureType::GRAB);
        }
    }
    
    void bindGesture(GestureType gesture, std::function<void()> command) {
        m_gestureCommands[gesture] = command;
    }
    
private:
    void initializeCommands() {
        // Default DearDoor commands
        bindGesture(GestureType::SWIPE_LEFT, []() {
            std::cout << "Navigate to previous hallway" << std::endl;
        });
        
        bindGesture(GestureType::SWIPE_RIGHT, []() {
            std::cout << "Navigate to next hallway" << std::endl;
        });
        
        bindGesture(GestureType::SWIPE_UP, []() {
            std::cout << "Scroll up" << std::endl;
        });
        
        bindGesture(GestureType::SWIPE_DOWN, []() {
            std::cout << "Scroll down" << std::endl;
        });
        
        bindGesture(GestureType::TAP, []() {
            std::cout << "Open selected door" << std::endl;
        });
        
        bindGesture(GestureType::PINCH, []() {
            std::cout << "Zoom in" << std::endl;
        });
        
        bindGesture(GestureType::GRAB, []() {
            std::cout << "Grab and move door" << std::endl;
        });
    }
    
    void executeGesture(GestureType gesture) {
        auto it = m_gestureCommands.find(gesture);
        if (it != m_gestureCommands.end()) {
            it->second();
        }
    }
};

// ============================================================================
// Main Application
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "DearDoor - Holographic Touch Display" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Initialize holographic display
    HolographicDisplay display;
    
    if (!display.initialize()) {
        std::cerr << "Failed to initialize holographic display" << std::endl;
        return -1;
    }
    
    // Add holographic UI elements
    display.addElement(
        "launch_game",
        ImVec2(50, 100),
        ImVec2(200, 60),
        "Launch Game",
        []() {
            std::cout << "Launching selected game..." << std::endl;
            // Launch game through Steam
        }
    );
    
    display.addElement(
        "browse_library",
        ImVec2(50, 180),
        ImVec2(200, 60),
        "Browse Library",
        []() {
            std::cout << "Opening game library..." << std::endl;
        }
    );
    
    display.addElement(
        "settings",
        ImVec2(50, 260),
        ImVec2(200, 60),
        "Settings",
        []() {
            std::cout << "Opening settings..." << std::endl;
        }
    );
    
    display.addElement(
        "exit",
        ImVec2(50, 340),
        ImVec2(200, 60),
        "Exit",
        []() {
            std::cout << "Exiting DearDoor..." << std::endl;
        }
    );
    
    // Main loop
    auto lastTime = std::chrono::steady_clock::now();
    
    while (!display.shouldClose()) {
        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(
            currentTime - lastTime
        ).count();
        lastTime = currentTime;
        
        // Update display
        display.update(deltaTime);
        
        // Render holographic interface
        display.render();
    }
    
    display.shutdown();
    
    std::cout << "Holographic display closed" << std::endl;
    return 0;
}