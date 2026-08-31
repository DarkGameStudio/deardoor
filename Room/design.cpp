// deardoor_design.cpp
// DearDoor - Immersive Steam Game Launcher with Door Navigation
// Complete implementation with detailed architecture

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <queue>
#include <fstream>
#include <sstream>
#include <filesystem>

// Steamworks
#include <steam/steam_api.h>

// GLFW for window management
#include <GLFW/glfw3.h>

// Dear ImGui for UI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

// ============================================================================
// DearDoor Core Design Philosophy
// ============================================================================
/*
 * DearDoor Design Architecture:
 * 
 * 1. DOOR METAPHOR: Each game is represented as a unique "door" in a 3D-like hallway
 * 2. ROOM SYSTEM: Games are organized into "rooms" (categories)
 * 3. HALLWAY NAVIGATION: Navigate through hallways to find game doors
 * 4. DOOR STATES: Closed, Opening, Open, Playing, Locked
 * 5. VISUAL IDENTITY: Dark, mysterious atmosphere with glowing door frames
 * 6. INTERACTION: Click doors to open and enter games
 * 7. PERSISTENCE: Remember door positions and states
 */

// ============================================================================
// Configuration and Constants
// ============================================================================
namespace DearDoorConfig {
    // Window settings
    constexpr int DEFAULT_WINDOW_WIDTH = 1600;
    constexpr int DEFAULT_WINDOW_HEIGHT = 900;
    constexpr int MIN_WINDOW_WIDTH = 1024;
    constexpr int MIN_WINDOW_HEIGHT = 576;
    
    // Door dimensions
    constexpr float DOOR_WIDTH = 180.0f;
    constexpr float DOOR_HEIGHT = 320.0f;
    constexpr float DOOR_FRAME_THICKNESS = 8.0f;
    constexpr float DOOR_HANDLE_SIZE = 12.0f;
    constexpr float DOOR_ARCH_RADIUS = 90.0f;
    
    // Hallway layout
    constexpr float HALLWAY_WIDTH = 1200.0f;
    constexpr float HALLWAY_SPACING = 400.0f;
    constexpr int DOORS_PER_HALLWAY = 5;
    constexpr int MAX_HALLWAYS = 20;
    
    // Animation timing (seconds)
    constexpr float DOOR_OPEN_ANIMATION_TIME = 0.8f;
    constexpr float DOOR_CLOSE_ANIMATION_TIME = 0.6f;
    constexpr float TRANSITION_FADE_TIME = 0.5f;
    constexpr float GLOW_PULSE_SPEED = 2.0f;
    constexpr float PARTICLE_LIFETIME = 2.0f;
    
    // Colors (DarkDearDoor palette)
    constexpr ImVec4 COLOR_BACKGROUND = ImVec4(0.05f, 0.05f, 0.08f, 1.0f);
    constexpr ImVec4 COLOR_FLOOR = ImVec4(0.08f, 0.06f, 0.10f, 1.0f);
    constexpr ImVec4 COLOR_WALL = ImVec4(0.10f, 0.08f, 0.12f, 1.0f);
    constexpr ImVec4 COLOR_CEILING = ImVec4(0.07f, 0.07f, 0.10f, 1.0f);
    constexpr ImVec4 COLOR_DOOR_FRAME = ImVec4(0.35f, 0.25f, 0.15f, 1.0f); // Wood
    constexpr ImVec4 COLOR_DOOR_PANEL = ImVec4(0.20f, 0.15f, 0.10f, 1.0f);
    constexpr ImVec4 COLOR_DOOR_HANDLE = ImVec4(0.80f, 0.70f, 0.50f, 1.0f); // Brass
    constexpr ImVec4 COLOR_GLOW_BLUE = ImVec4(0.20f, 0.50f, 0.90f, 1.0f);
    constexpr ImVec4 COLOR_GLOW_PURPLE = ImVec4(0.60f, 0.30f, 0.80f, 1.0f);
    constexpr ImVec4 COLOR_GLOW_GOLD = ImVec4(0.90f, 0.70f, 0.30f, 1.0f);
    constexpr ImVec4 COLOR_TEXT_PRIMARY = ImVec4(0.90f, 0.85f, 0.80f, 1.0f);
    constexpr ImVec4 COLOR_TEXT_SECONDARY = ImVec4(0.60f, 0.55f, 0.50f, 1.0f);
}

// ============================================================================
// Game Door System
// ============================================================================

// Door states representing the lifecycle of a game
enum class DoorState {
    LOCKED,         // Game not installed or unavailable
    CLOSED,         // Available but not opened
    OPENING,        // Animation: door opening
    OPEN,           // Door open, showing game info
    ENTERING,       // Player entering the game
    PLAYING,        // Game is running
    CLOSING,        // Animation: door closing
    ERROR           // Something went wrong
};

// Room categories for organizing games
enum class RoomCategory {
    ALL_GAMES,
    RECENTLY_PLAYED,
    FAVORITES,
    INSTALLED,
    ACTION,
    ADVENTURE,
    RPG,
    STRATEGY,
    SIMULATION,
    INDIE,
    MULTIPLAYER,
    CUSTOM_ROOM
};

// Structure to hold comprehensive game information
struct DearDoorGameInfo {
    AppId_t steamAppId;
    std::string name;
    std::string description;
    std::string installDirectory;
    std::string iconPath;
    std::string headerImagePath;
    std::string libraryImagePath;
    
    bool isInstalled;
    bool isRunning;
    bool isFavorite;
    bool isRecent;
    
    int playtimeMinutes;
    int lastPlayedTimestamp;
    int achievementCount;
    int totalAchievements;
    
    std::vector<std::string> tags;
    std::vector<std::string> genres;
    
    RoomCategory category;
    DoorState doorState;
    
    // Visual properties
    ImVec4 doorColor;
    ImVec4 glowColor;
    float doorGlowIntensity;
    float doorScale;
    int doorPositionX;
    int doorPositionY;
    int hallwayIndex;
};

// ============================================================================
// Door Animation System
// ============================================================================
class DoorAnimationSystem {
private:
    struct DoorAnimation {
        float progress;          // 0.0 to 1.0
        float speed;            // Animation speed multiplier
        bool isAnimating;
        DoorState targetState;
        std::chrono::steady_clock::time_point startTime;
    };
    
    std::map<AppId_t, DoorAnimation> m_animations;
    
public:
    void StartAnimation(AppId_t appId, DoorState targetState, float duration) {
        DoorAnimation anim;
        anim.progress = 0.0f;
        anim.speed = 1.0f / duration;
        anim.isAnimating = true;
        anim.targetState = targetState;
        anim.startTime = std::chrono::steady_clock::now();
        m_animations[appId] = anim;
    }
    
    void UpdateAnimation(AppId_t appId, float deltaTime) {
        auto it = m_animations.find(appId);
        if (it != m_animations.end() && it->second.isAnimating) {
            it->second.progress += deltaTime * it->second.speed;
            if (it->second.progress >= 1.0f) {
                it->second.progress = 1.0f;
                it->second.isAnimating = false;
            }
        }
    }
    
    float GetAnimationProgress(AppId_t appId) {
        auto it = m_animations.find(appId);
        if (it != m_animations.end()) {
            return it->second.progress;
        }
        return 0.0f;
    }
    
    bool IsAnimating(AppId_t appId) {
        auto it = m_animations.find(appId);
        return it != m_animations.end() && it->second.isAnimating;
    }
    
    DoorState GetTargetState(AppId_t appId) {
        auto it = m_animations.find(appId);
        if (it != m_animations.end()) {
            return it->second.targetState;
        }
        return DoorState::CLOSED;
    }
};

// ============================================================================
// Particle Effect System for Doors
// ============================================================================
class DoorParticleSystem {
private:
    struct Particle {
        ImVec2 position;
        ImVec2 velocity;
        float life;
        float maxLife;
        float size;
        ImVec4 color;
        float rotation;
        float rotationSpeed;
    };
    
    std::map<AppId_t, std::vector<Particle>> m_doorParticles;
    std::random_device m_rd;
    std::mt19937 m_gen;
    
public:
    DoorParticleSystem() : m_gen(m_rd()) {}
    
    void EmitParticles(AppId_t appId, const ImVec2& doorCenter, int count, const ImVec4& color) {
        auto& particles = m_doorParticles[appId];
        
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> speedDist(10.0f, 50.0f);
        std::uniform_real_distribution<float> lifeDist(0.5f, 2.0f);
        std::uniform_real_distribution<float> sizeDist(1.0f, 4.0f);
        
        for (int i = 0; i < count; i++) {
            Particle p;
            float angle = angleDist(m_gen);
            float speed = speedDist(m_gen);
            
            p.position = doorCenter;
            p.velocity = ImVec2(cos(angle) * speed, sin(angle) * speed);
            p.life = 0.0f;
            p.maxLife = lifeDist(m_gen);
            p.size = sizeDist(m_gen);
            p.color = color;
            p.rotation = angleDist(m_gen);
            p.rotationSpeed = (speedDist(m_gen) - 25.0f) * 0.1f;
            
            particles.push_back(p);
        }
    }
    
    void UpdateParticles(float deltaTime) {
        for (auto& [appId, particles] : m_doorParticles) {
            for (auto& p : particles) {
                p.life += deltaTime;
                p.position.x += p.velocity.x * deltaTime;
                p.position.y += p.velocity.y * deltaTime;
                p.rotation += p.rotationSpeed * deltaTime;
                
                // Fade out
                float lifeRatio = p.life / p.maxLife;
                p.color.w = 1.0f - lifeRatio;
            }
            
            // Remove dead particles
            particles.erase(
                std::remove_if(particles.begin(), particles.end(),
                              [](const Particle& p) { return p.life >= p.maxLife; }),
                particles.end()
            );
        }
    }
    
    void DrawParticles(ImDrawList* drawList, AppId_t appId) {
        auto it = m_doorParticles.find(appId);
        if (it != m_doorParticles.end()) {
            for (const auto& p : it->second) {
                ImU32 color = ImGui::ColorConvertFloat4ToU32(p.color);
                if (p.size > 2.5f) {
                    // Draw as small squares for larger particles
                    ImVec2 halfSize(p.size * 0.5f, p.size * 0.5f);
                    drawList->AddRectFilled(
                        ImVec2(p.position.x - halfSize.x, p.position.y - halfSize.y),
                        ImVec2(p.position.x + halfSize.x, p.position.y + halfSize.y),
                        color
                    );
                } else {
                    drawList->AddCircleFilled(p.position, p.size, color);
                }
            }
        }
    }
    
    void ClearParticles(AppId_t appId) {
        auto it = m_doorParticles.find(appId);
        if (it != m_doorParticles.end()) {
            it->second.clear();
        }
    }
};

// ============================================================================
// Door Rendering Engine
// ============================================================================
class DoorRenderer {
private:
    struct DoorVisualState {
        float openAmount;        // 0.0 (closed) to 1.0 (fully open)
        float glowIntensity;     // Current glow level
        float bobOffset;         // Floating animation offset
        float shakeOffset;       // Screen shake when opening
        ImVec4 currentGlowColor;
        float archProgress;      // For animated arch drawing
    };
    
    std::map<AppId_t, DoorVisualState> m_visualStates;
    DoorAnimationSystem m_animationSystem;
    DoorParticleSystem m_particleSystem;
    
public:
    void UpdateDoorVisuals(const DearDoorGameInfo& game, float deltaTime) {
        auto& visual = m_visualStates[game.steamAppId];
        
        // Update animations
        m_animationSystem.UpdateAnimation(game.steamAppId, deltaTime);
        
        if (m_animationSystem.IsAnimating(game.steamAppId)) {
            float progress = m_animationSystem.GetAnimationProgress(game.steamAppId);
            DoorState target = m_animationSystem.GetTargetState(game.steamAppId);
            
            switch (target) {
                case DoorState::OPENING:
                    visual.openAmount = progress;
                    visual.glowIntensity = progress;
                    break;
                case DoorState::CLOSING:
                    visual.openAmount = 1.0f - progress;
                    visual.glowIntensity = 1.0f - progress;
                    break;
                case DoorState::OPEN:
                    visual.openAmount = 1.0f;
                    visual.glowIntensity = 1.0f;
                    break;
                default:
                    break;
            }
            
            // Emit particles during opening
            if (target == DoorState::OPENING && progress < 0.5f) {
                ImVec2 doorCenter(game.doorPositionX, game.doorPositionY);
                m_particleSystem.EmitParticles(
                    game.steamAppId, doorCenter, 3, game.glowColor
                );
            }
        }
        
        // Update idle animations
        float time = static_cast<float>(glfwGetTime());
        visual.bobOffset = sin(time * 2.0f + game.doorPositionX) * 3.0f;
        visual.glowIntensity += (game.doorGlowIntensity - visual.glowIntensity) * deltaTime * 2.0f;
        visual.currentGlowColor = game.glowColor;
        
        // Update particles
        m_particleSystem.UpdateParticles(deltaTime);
    }
    
    void DrawDoor(ImDrawList* drawList, const DearDoorGameInfo& game) {
        auto visual = m_visualStates[game.steamAppId];
        
        float x = game.doorPositionX;
        float y = game.doorPositionY + visual.bobOffset;
        float width = DearDoorConfig::DOOR_WIDTH * game.doorScale;
        float height = DearDoorConfig::DOOR_HEIGHT * game.doorScale;
        
        // Draw door shadow
        ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.0f, 0.0f, 0.0f, 0.3f)
        );
        drawList->AddRectFilled(
            ImVec2(x - width * 0.5f + 10, y + 10),
            ImVec2(x + width * 0.5f + 10, y + height + 10),
            shadowColor, 8.0f
        );
        
        // Draw door frame (with arch)
        DrawDoorFrame(drawList, x, y, width, height, game);
        
        // Draw door panel
        float openOffset = visual.openAmount * width * 0.8f;
        if (visual.openAmount < 1.0f) {
            DrawDoorPanel(drawList, x - openOffset, y, width, height, game);
        }
        
        // Draw door handle
        if (visual.openAmount < 0.5f) {
            DrawDoorHandle(drawList, x + width * 0.3f - openOffset, y + height * 0.5f);
        }
        
        // Draw glow effect
        DrawDoorGlow(drawList, x, y, width, height, game, visual.glowIntensity);
        
        // Draw particles
        m_particleSystem.DrawParticles(drawList, game.steamAppId);
        
        // Draw game name
        DrawDoorLabel(drawList, x, y + height + 20, game.name, game);
        
        // Draw status indicator
        DrawDoorStatus(drawList, x + width * 0.5f - 10, y - 10, game);
    }
    
private:
    void DrawDoorFrame(ImDrawList* drawList, float x, float y, float width, float height,
                      const DearDoorGameInfo& game) {
        float frameThickness = DearDoorConfig::DOOR_FRAME_THICKNESS;
        ImU32 frameColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_DOOR_FRAME);
        
        // Left frame
        drawList->AddRectFilled(
            ImVec2(x - width * 0.5f, y),
            ImVec2(x - width * 0.5f + frameThickness, y + height),
            frameColor
        );
        
        // Right frame
        drawList->AddRectFilled(
            ImVec2(x + width * 0.5f - frameThickness, y),
            ImVec2(x + width * 0.5f, y + height),
            frameColor
        );
        
        // Top frame (with arch)
        float archRadius = DearDoorConfig::DOOR_ARCH_RADIUS;
        drawList->AddRectFilled(
            ImVec2(x - width * 0.5f, y - frameThickness),
            ImVec2(x + width * 0.5f, y),
            frameColor
        );
        
        // Draw arch
        drawList->PathArcTo(
            ImVec2(x, y),
            archRadius,
            3.14159f, 6.28318f,
            32
        );
        drawList->PathStroke(frameColor, false, frameThickness);
    }
    
    void DrawDoorPanel(ImDrawList* drawList, float x, float y, float width, float height,
                      const DearDoorGameInfo& game) {
        ImU32 panelColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_DOOR_PANEL);
        
        // Main panel
        drawList->AddRectFilled(
            ImVec2(x - width * 0.4f, y + 10),
            ImVec2(x + width * 0.4f, y + height - 10),
            panelColor, 4.0f
        );
        
        // Panel details (recessed rectangles)
        ImU32 detailColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.15f, 0.10f, 0.08f, 1.0f)
        );
        
        float panelWidth = width * 0.6f;
        float panelHeight = height * 0.25f;
        float panelSpacing = 20.0f;
        
        // Top panel
        drawList->AddRectFilled(
            ImVec2(x - panelWidth * 0.5f, y + 30),
            ImVec2(x + panelWidth * 0.5f, y + 30 + panelHeight),
            detailColor, 2.0f
        );
        
        // Bottom panel
        drawList->AddRectFilled(
            ImVec2(x - panelWidth * 0.5f, y + height - 30 - panelHeight),
            ImVec2(x + panelWidth * 0.5f, y + height - 30),
            detailColor, 2.0f
        );
    }
    
    void DrawDoorHandle(ImDrawList* drawList, float x, float y) {
        ImU32 handleColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_DOOR_HANDLE);
        float handleSize = DearDoorConfig::DOOR_HANDLE_SIZE;
        
        drawList->AddCircleFilled(ImVec2(x, y), handleSize, handleColor);
        drawList->AddCircle(ImVec2(x, y), handleSize, 
                           ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.3f)),
                           16, 2.0f);
    }
    
    void DrawDoorGlow(ImDrawList* drawList, float x, float y, float width, float height,
                     const DearDoorGameInfo& game, float intensity) {
        if (intensity <= 0.01f) return;
        
        float glowThickness = 4.0f + intensity * 8.0f;
        ImVec4 glowColor = game.glowColor;
        glowColor.w = intensity * 0.6f;
        
        ImU32 glowColorU32 = ImGui::ColorConvertFloat4ToU32(glowColor);
        
        // Draw glow around frame
        drawList->AddRect(
            ImVec2(x - width * 0.5f - glowThickness, y - glowThickness),
            ImVec2(x + width * 0.5f + glowThickness, y + height + glowThickness),
            glowColorU32, 8.0f, 0, glowThickness
        );
        
        // Inner glow
        ImVec4 innerGlow = glowColor;
        innerGlow.w = intensity * 0.3f;
        ImU32 innerGlowU32 = ImGui::ColorConvertFloat4ToU32(innerGlow);
        
        if (intensity > 0.5f) {
            drawList->AddRectFilled(
                ImVec2(x - width * 0.3f, y + height * 0.3f),
                ImVec2(x + width * 0.3f, y + height * 0.7f),
                innerGlowU32
            );
        }
    }
    
    void DrawDoorLabel(ImDrawList* drawList, float x, float y, const std::string& name,
                      const DearDoorGameInfo& game) {
        ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
        
        // Truncate if too long
        std::string displayName = name;
        if (textSize.x > DearDoorConfig::DOOR_WIDTH + 40) {
            while (ImGui::CalcTextSize((displayName + "...").c_str()).x > 
                   DearDoorConfig::DOOR_WIDTH + 40 && displayName.length() > 0) {
                displayName.pop_back();
            }
            displayName += "...";
        }
        
        // Draw text background
        ImVec2 textPos(x - textSize.x * 0.5f, y);
        ImVec2 bgMin(textPos.x - 5, textPos.y - 3);
        ImVec2 bgMax(textPos.x + textSize.x + 5, textPos.y + textSize.y + 3);
        
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.0f, 0.0f, 0.0f, 0.7f)
        );
        drawList->AddRectFilled(bgMin, bgMax, bgColor, 4.0f);
        
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_TEXT_PRIMARY);
        drawList->AddText(textPos, textColor, displayName.c_str());
    }
    
    void DrawDoorStatus(ImDrawList* drawList, float x, float y,
                       const DearDoorGameInfo& game) {
        ImVec4 statusColor;
        const char* statusText;
        
        switch (game.doorState) {
            case DoorState::PLAYING:
                statusColor = DearDoorConfig::COLOR_GLOW_GOLD;
                statusText = "● Playing";
                break;
            case DoorState::OPEN:
                statusColor = DearDoorConfig::COLOR_GLOW_BLUE;
                statusText = "● Open";
                break;
            case DoorState::LOCKED:
                statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                statusText = "● Locked";
                break;
            case DoorState::ERROR:
                statusColor = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
                statusText = "● Error";
                break;
            default:
                statusColor = DearDoorConfig::COLOR_GLOW_PURPLE;
                statusText = "● Ready";
                break;
        }
        
        ImU32 color = ImGui::ColorConvertFloat4ToU32(statusColor);
        ImVec2 textSize = ImGui::CalcTextSize(statusText);
        drawList->AddText(ImVec2(x - textSize.x * 0.5f, y - textSize.y * 0.5f),
                         color, statusText);
    }
};

// ============================================================================
// Hallway System
// ============================================================================
class HallwaySystem {
private:
    struct Hallway {
        int index;
        std::string name;
        RoomCategory category;
        std::vector<AppId_t> doorIds;
        float xPosition;  // For scrolling
        float scrollVelocity;
        bool isActive;
    };
    
    std::vector<Hallway> m_hallways;
    float m_currentScrollPosition = 0.0f;
    float m_targetScrollPosition = 0.0f;
    int m_activeHallwayIndex = 0;
    
public:
    HallwaySystem() {
        InitializeHallways();
    }
    
    void InitializeHallways() {
        m_hallways.clear();
        
        // Create default hallways
        CreateHallway("All Games", RoomCategory::ALL_GAMES);
        CreateHallway("Recently Played", RoomCategory::RECENTLY_PLAYED);
        CreateHallway("Favorites", RoomCategory::FAVORITES);
        CreateHallway("Installed", RoomCategory::INSTALLED);
        CreateHallway("Action", RoomCategory::ACTION);
        CreateHallway("Adventure", RoomCategory::ADVENTURE);
        CreateHallway("RPG", RoomCategory::RPG);
        CreateHallway("Strategy", RoomCategory::STRATEGY);
        CreateHallway("Simulation", RoomCategory::SIMULATION);
        CreateHallway("Indie", RoomCategory::INDIE);
        CreateHallway("Multiplayer", RoomCategory::MULTIPLAYER);
    }
    
    void CreateHallway(const std::string& name, RoomCategory category) {
        Hallway hallway;
        hallway.index = m_hallways.size();
        hallway.name = name;
        hallway.category = category;
        hallway.xPosition = hallway.index * DearDoorConfig::HALLWAY_SPACING;
        hallway.scrollVelocity = 0.0f;
        hallway.isActive = (hallway.index == 0);
        m_hallways.push_back(hallway);
    }
    
    void NavigateToHallway(int index) {
        if (index >= 0 && index < m_hallways.size()) {
            m_targetScrollPosition = index * DearDoorConfig::HALLWAY_SPACING;
            m_activeHallwayIndex = index;
            
            for (auto& hallway : m_hallways) {
                hallway.isActive = (hallway.index == index);
            }
        }
    }
    
    void NavigateNext() {
        if (m_activeHallwayIndex < m_hallways.size() - 1) {
            NavigateToHallway(m_activeHallwayIndex + 1);
        }
    }
    
    void NavigatePrevious() {
        if (m_activeHallwayIndex > 0) {
            NavigateToHallway(m_activeHallwayIndex - 1);
        }
    }
    
    void Update(float deltaTime) {
        // Smooth scroll animation
        float scrollDifference = m_targetScrollPosition - m_currentScrollPosition;
        float scrollSpeed = 8.0f;
        m_currentScrollPosition += scrollDifference * scrollSpeed * deltaTime;
        
        if (abs(scrollDifference) < 0.1f) {
            m_currentScrollPosition = m_targetScrollPosition;
        }
    }
    
    std::vector<Hallway>& GetHallways() { return m_hallways; }
    int GetActiveHallwayIndex() const { return m_activeHallwayIndex; }
    float GetScrollPosition() const { return m_currentScrollPosition; }
};

// ============================================================================
// DearDoor Main Application
// ============================================================================
class DearDoorApplication {
private:
    GLFWwindow* m_window;
    std::vector<DearDoorGameInfo> m_games;
    DoorRenderer m_doorRenderer;
    HallwaySystem m_hallwaySystem;
    DoorParticleSystem m_particleSystem;
    
    bool m_isInitialized;
    bool m_showSettings;
    bool m_showGameDetails;
    AppId_t m_selectedGameId;
    
    float m_lastFrameTime;
    ImVec2 m_mousePosition;
    
    // UI State
    char m_searchBuffer[256];
    std::string m_searchQuery;
    bool m_showFavoritesOnly;
    bool m_showInstalledOnly;
    int m_currentViewMode;  // 0: Hallway, 1: Grid, 2: List
    
public:
    DearDoorApplication() 
        : m_window(nullptr)
        , m_isInitialized(false)
        , m_showSettings(false)
        , m_showGameDetails(false)
        , m_selectedGameId(0)
        , m_lastFrameTime(0.0f)
        , m_showFavoritesOnly(false)
        , m_showInstalledOnly(false)
        , m_currentViewMode(0) {
        memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
    }
    
    ~DearDoorApplication() {
        Shutdown();
    }
    
    bool Initialize() {
        std::cout << "Initializing DearDoor - Steam Game Portal System..." << std::endl;
        
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        // Configure GLFW
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);  // Anti-aliasing
        
        // Create window
        m_window = glfwCreateWindow(
            DearDoorConfig::DEFAULT_WINDOW_WIDTH,
            DearDoorConfig::DEFAULT_WINDOW_HEIGHT,
            "DearDoor - Your Steam Game Portal",
            nullptr, nullptr
        );
        
        if (!m_window) {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(m_window, this);
        
        // Setup callbacks
        SetupCallbacks();
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        // Load fonts
        SetupFonts(io);
        
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Setup DearDoor style
        SetupDearDoorStyle();
        
        // Initialize Steam
        if (!SteamManager::GetInstance().Initialize()) {
            std::cerr << "Failed to initialize Steam" << std::endl;
            return false;
        }
        
        // Load games
        LoadGames();
        
        m_isInitialized = true;
        m_lastFrameTime = glfwGetTime();
        
        std::cout << "DearDoor initialized successfully!" << std::endl;
        std::cout << "Loaded " << m_games.size() << " games from Steam library" << std::endl;
        
        return true;
    }
    
    void Run() {
        while (!glfwWindowShouldClose(m_window) && m_isInitialized) {
            glfwPollEvents();
            
            // Calculate delta time
            double currentTime = glfwGetTime();
            float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
            m_lastFrameTime = currentTime;
            
            // Update systems
            Update(deltaTime);
            
            // Render
            Render();
            
            // Swap buffers
            glfwSwapBuffers(m_window);
            
            // Process Steam callbacks
            SteamAPI_RunCallbacks();
        }
    }
    
    void Shutdown() {
        if (m_isInitialized) {
            SteamManager::GetInstance().Shutdown();
            
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            
            if (m_window) {
                glfwDestroyWindow(m_window);
                m_window = nullptr;
            }
            
            glfwTerminate();
            m_isInitialized = false;
        }
    }
    
private:
    void SetupCallbacks() {
        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            glViewport(0, 0, width, height);
        });
        
        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
            DearDoorApplication* app = static_cast<DearDoorApplication*>(
                glfwGetWindowUserPointer(window)
            );
            if (app) {
                app->m_mousePosition = ImVec2(static_cast<float>(xpos), static_cast<float>(ypos));
            }
        });
    }
    
    void SetupFonts(ImGuiIO& io) {
        // Load default font
        io.Fonts->AddFontDefault();
        
        // Load larger font for titles
        ImFontConfig config;
        config.SizePixels = 24.0f;
        io.Fonts->AddFontDefault(&config);
    }
    
    void SetupDearDoorStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Colors
        style.Colors[ImGuiCol_WindowBg] = DearDoorConfig::COLOR_BACKGROUND;
        style.Colors[ImGuiCol_ChildBg] = DearDoorConfig::COLOR_WALL;
        style.Colors[ImGuiCol_PopupBg] = DearDoorConfig::COLOR_WALL;
        style.Colors[ImGuiCol_Border] = ImVec4(0.3f, 0.25f, 0.2f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.12f, 0.10f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.20f, 0.15f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.25f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.15f, 0.12f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.25f, 0.18f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.30f, 0.20f, 1.0f);
        style.Colors[ImGuiCol_Text] = DearDoorConfig::COLOR_TEXT_PRIMARY;
        style.Colors[ImGuiCol_TextDisabled] = DearDoorConfig::COLOR_TEXT_SECONDARY;
        
        // Style
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
        
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12;
    }
    
    void LoadGames() {
        m_games.clear();
        
        auto steamGames = SteamManager::GetInstance().GetOwnedGames();
        
        int hallwayIndex = 0;
        int doorIndex = 0;
        
        for (const auto& steamGame : steamGames) {
            DearDoorGameInfo game;
            
            // Copy basic info
            game.steamAppId = steamGame.appId;
            game.name = steamGame.name;
            game.installDirectory = steamGame.installDir;
            game.isInstalled = steamGame.isInstalled;
            game.isRunning = steamGame.isRunning;
            game.headerImagePath = steamGame.headerImagePath;
            
            // Set default states
            game.isFavorite = false;
            game.isRecent = false;
            game.playtimeMinutes = 0;
            game.lastPlayedTimestamp = 0;
            game.achievementCount = 0;
            game.totalAchievements = 0;
            
            // Determine door state
            if (!game.isInstalled) {
                game.doorState = DoorState::LOCKED;
            } else if (game.isRunning) {
                game.doorState = DoorState::PLAYING;
            } else {
                game.doorState = DoorState::CLOSED;
            }
            
            // Assign visual properties
            game.doorColor = GenerateDoorColor(doorIndex);
            game.glowColor = GenerateGlowColor(doorIndex);
            game.doorGlowIntensity = game.isInstalled ? 0.6f : 0.2f;
            game.doorScale = 1.0f;
            
            // Position doors in hallway
            game.hallwayIndex = hallwayIndex;
            game.doorPositionX = CalculateDoorX(doorIndex);
            game.doorPositionY = CalculateDoorY();
            
            // Assign category
            game.category = RoomCategory::ALL_GAMES;
            
            // Add tags (simplified)
            game.tags.push_back("Steam Game");
            game.genres.push_back("General");
            
            m_games.push_back(game);
            
            // Update indices
            doorIndex++;
            if (doorIndex >= DearDoorConfig::DOORS_PER_HALLWAY) {
                doorIndex = 0;
                hallwayIndex++;
                if (hallwayIndex >= DearDoorConfig::MAX_HALLWAYS) {
                    hallwayIndex = 0;  // Wrap around
                }
            }
        }
        
        std::cout << "Loaded " << m_games.size() << " game doors" << std::endl;
    }
    
    ImVec4 GenerateDoorColor(int index) {
        // Generate unique door colors based on index
        float hue = (index * 0.618034f);  // Golden ratio
        float saturation = 0.5f;
        float value = 0.6f;
        
        // Convert HSV to RGB
        float h = fmod(hue, 1.0f) * 6.0f;
        float f = h - floor(h);
        float p = value * (1.0f - saturation);
        float q = value * (1.0f - saturation * f);
        float t = value * (1.0f - saturation * (1.0f - f));
        
        float r, g, b;
        int hi = static_cast<int>(h) % 6;
        
        switch (hi) {
            case 0: r = value; g = t; b = p; break;
            case 1: r = q; g = value; b = p; break;
            case 2: r = p; g = value; b = t; break;
            case 3: r = p; g = q; b = value; break;
            case 4: r = t; g = p; b = value; break;
            default: r = value; g = p; b = q; break;
        }
        
        return ImVec4(r * 0.5f + 0.2f, g * 0.5f + 0.15f, b * 0.5f + 0.1f, 1.0f);
    }
    
    ImVec4 GenerateGlowColor(int index) {
        // Generate complementary glow colors
        ImVec4 baseColor = GenerateDoorColor(index + 30);
        return ImVec4(
            baseColor.x * 1.2f,
            baseColor.y * 1.2f,
            baseColor.z * 1.2f,
            1.0f
        );
    }
    
    float CalculateDoorX(int doorIndex) {
        float spacing = DearDoorConfig::DOOR_WIDTH + 60.0f;
        float startX = DearDoorConfig::DEFAULT_WINDOW_WIDTH * 0.5f - 
                       (DearDoorConfig::DOORS_PER_HALLWAY - 1) * spacing * 0.5f;
        return startX + doorIndex * spacing;
    }
    
    float CalculateDoorY() {
        return DearDoorConfig::DEFAULT_WINDOW_HEIGHT * 0.55f;
    }
    
    void Update(float deltaTime) {
        // Update hallway system
        m_hallwaySystem.Update(deltaTime);
        
        // Update door visuals
        for (auto& game : m_games) {
            m_doorRenderer.UpdateDoorVisuals(game, deltaTime);
        }
        
        // Handle keyboard input
        HandleKeyboardInput();
        
        // Update search
        m_searchQuery = m_searchBuffer;
    }
    
    void HandleKeyboardInput() {
        // Hallway navigation
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow))) {
            m_hallwaySystem.NavigatePrevious();
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow))) {
            m_hallwaySystem.NavigateNext();
        }
        
        // View modes
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F1))) {
            m_currentViewMode = 0;  // Hallway view
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F2))) {
            m_currentViewMode = 1;  // Grid view
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F3))) {
            m_currentViewMode = 2;  // List view
        }
        
        // Toggle settings
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F10))) {
            m_showSettings = !m_showSettings;
        }
        
        // Exit
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
            glfwSetWindowShouldClose(m_window, true);
        }
    }
    
    void Render() {
        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Clear screen
        glClearColor(
            DearDoorConfig::COLOR_BACKGROUND.x,
            DearDoorConfig::COLOR_BACKGROUND.y,
            DearDoorConfig::COLOR_BACKGROUND.z,
            DearDoorConfig::COLOR_BACKGROUND.w
        );
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render main interface
        RenderMainInterface();
        
        // Render settings window
        if (m_showSettings) {
            RenderSettingsWindow();
        }
        
        // Render game details
        if (m_showGameDetails) {
            RenderGameDetails();
        }
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    
    void RenderMainInterface() {
        // Fullscreen window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("DearDoor", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBringToFrontOnFocus |
                    ImGuiWindowFlags_NoBackground);
        
        // Render based on view mode
        switch (m_currentViewMode) {
            case 0:
                RenderHallwayView();
                break;
            case 1:
                RenderGridView();
                break;
            case 2:
                RenderListView();
                break;
        }
        
        // Render top bar
        RenderTopBar();
        
        // Render bottom navigation
        RenderBottomNavigation();
        
        ImGui::End();
    }
    
    void RenderHallwayView() {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        
        // Draw floor
        ImU32 floorColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_FLOOR);
        drawList->AddRectFilled(
            ImVec2(0, screenSize.y * 0.7f),
            screenSize,
            floorColor
        );
        
        // Draw walls
        ImU32 wallColor = ImGui::ColorConvertFloat4ToU32(DearDoorConfig::COLOR_WALL);
        drawList->AddRectFilled(
            ImVec2(0, 0),
            ImVec2(screenSize.x, screenSize.y * 0.7f),
            wallColor
        );
        
        // Draw perspective lines
        float horizonY = screenSize.y * 0.7f;
        ImU32 perspectiveColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.3f, 0.25f, 0.2f, 0.3f)
        );
        
        for (int i = -5; i <= 5; i++) {
            float x = screenSize.x * 0.5f + i * 200.0f;
            drawList->AddLine(
                ImVec2(screenSize.x * 0.5f, horizonY),
                ImVec2(x, screenSize.y),
                perspectiveColor, 1.0f
            );
        }
        
        // Draw doors for active hallway
        int activeHallwayIndex = m_hallwaySystem.GetActiveHallwayIndex();
        float scrollOffset = -m_hallwaySystem.GetScrollPosition();
        
        for (auto& game : m_games) {
            if (game.hallwayIndex == activeHallwayIndex) {
                // Apply scroll offset
                DearDoorGameInfo drawGame = game;
                drawGame.doorPositionX += scrollOffset;
                m_doorRenderer.DrawDoor(drawList, drawGame);
            }
        }
    }
    
    void RenderGridView() {
        ImGui::BeginChild("GameGrid", ImVec2(0, 0), false);
        
        int columns = 5;
        float cellWidth = DearDoorConfig::DOOR_WIDTH + 40;
        float cellHeight = DearDoorConfig::DOOR_HEIGHT + 60;
        
        int index = 0;
        for (auto& game : m_games) {
            // Filter based on search
            if (!m_searchQuery.empty()) {
                std::string lowerName = game.name;
                std::string lowerQuery = m_searchQuery;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
                if (lowerName.find(lowerQuery) == std::string::npos) continue;
            }
            
            int row = index / columns;
            int col = index % columns;
            
            float x = col * cellWidth + 50;
            float y = row * cellHeight + 50;
            
            DearDoorGameInfo drawGame = game;
            drawGame.doorPositionX = x;
            drawGame.doorPositionY = y;
            drawGame.doorScale = 0.8f;
            
            m_doorRenderer.DrawDoor(ImGui::GetWindowDrawList(), drawGame);
            
            index++;
        }
        
        ImGui::EndChild();
    }
    
    void RenderListView() {
        ImGui::BeginChild("GameList", ImVec2(0, 0), false);
        
        for (auto& game : m_games) {
            // Filter based on search
            if (!m_searchQuery.empty()) {
                std::string lowerName = game.name;
                std::string lowerQuery = m_searchQuery;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
                if (lowerName.find(lowerQuery) == std::string::npos) continue;
            }
            
            ImGui::PushID(game.steamAppId);
            
            // Game entry
            ImGui::SetCursorPosX(20);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            
            // Status indicator
            ImVec4 statusColor;
            const char* statusIcon;
            switch (game.doorState) {
                case DoorState::PLAYING:
                    statusColor = DearDoorConfig::COLOR_GLOW_GOLD;
                    statusIcon = "▶";
                    break;
                case DoorState::OPEN:
                    statusColor = DearDoorConfig::COLOR_GLOW_BLUE;
                    statusIcon = "●";
                    break;
                case DoorState::LOCKED:
                    statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    statusIcon = "🔒";
                    break;
                default:
                    statusColor = DearDoorConfig::COLOR_GLOW_PURPLE;
                    statusIcon = "○";
                    break;
            }
            
            ImGui::TextColored(statusColor, "%s", statusIcon);
            ImGui::SameLine();
            
            // Game name
            ImGui::Text("%s", game.name.c_str());
            ImGui::SameLine(400);
            
            // Info
            ImGui::TextColored(DearDoorConfig::COLOR_TEXT_SECONDARY, 
                             "%s", game.isInstalled ? "Installed" : "Not Installed");
            
            ImGui::SameLine(550);
            
            // Launch button
            if (game.isInstalled && game.doorState != DoorState::PLAYING) {
                if (ImGui::Button("Launch", ImVec2(80, 25))) {
                    LaunchGame(game.steamAppId);
                }
            } else if (game.doorState == DoorState::PLAYING) {
                ImGui::TextColored(DearDoorConfig::COLOR_GLOW_GOLD, "Running");
            }
            
            ImGui::Separator();
            ImGui::PopID();
        }
        
        ImGui::EndChild();
    }
    
    void RenderTopBar() {
        ImGui::SetCursorPos(ImVec2(10, 10));
        
        // Title
        ImGui::TextColored(DearDoorConfig::COLOR_GLOW_GOLD, "DearDoor");
        ImGui::SameLine();
        
        // Search bar
        ImGui::SetCursorPosX(200);
        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##search", "Search games...", 
                                m_searchBuffer, sizeof(m_searchBuffer));
        ImGui::PopItemWidth();
        
        // View mode buttons
        ImGui::SameLine();
        if (ImGui::Button("Hallway", ImVec2(80, 25))) m_currentViewMode = 0;
        ImGui::SameLine();
        if (ImGui::Button("Grid", ImVec2(60, 25))) m_currentViewMode = 1;
        ImGui::SameLine();
        if (ImGui::Button("List", ImVec2(60, 25))) m_currentViewMode = 2;
        
        // Settings button
        ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 100);
        if (ImGui::Button("Settings", ImVec2(80, 25))) {
            m_showSettings = true;
        }
        
        ImGui::Separator();
    }
    
    void RenderBottomNavigation() {
        ImGui::SetCursorPosY(ImGui::GetIO().DisplaySize.y - 40);
        
        // Hallway navigation
        if (m_currentViewMode == 0) {
            ImGui::SetCursorPosX(20);
            if (ImGui::Button("< Previous", ImVec2(100, 25))) {
                m_hallwaySystem.NavigatePrevious();
            }
            
            ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 120);
            if (ImGui::Button("Next >", ImVec2(100, 25))) {
                m_hallwaySystem.NavigateNext();
            }
            
            // Hallway indicator
            ImGui::SetCursorPosX(ImGui::GetIO().DisplaySize.x * 0.5f - 100);
            int activeIndex = m_hallwaySystem.GetActiveHallwayIndex();
            auto& hallways = m_hallwaySystem.GetHallways();
            if (activeIndex >= 0 && activeIndex < hallways.size()) {
                ImGui::Text("Hallway: %s", hallways[activeIndex].name.c_str());
            }
        }
        
        // Footer
        ImGui::SetCursorPosY(ImGui::GetIO().DisplaySize.y - 20);
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(DearDoorConfig::COLOR_TEXT_SECONDARY, 
                          "DearDoor v1.0 | %zu games | F1-F3: Views | F10: Settings | ESC: Exit", 
                          m_games.size());
    }
    
    void RenderSettingsWindow() {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", &m_showSettings);
        
        ImGui::Text("Display Settings");
        ImGui::Separator();
        
        static bool fullscreen = false;
        if (ImGui::Checkbox("Fullscreen", &fullscreen)) {
            // Toggle fullscreen
        }
        
        ImGui::SliderFloat("Door Scale", &m_doorScale, 0.5f, 1.5f);
        ImGui::SliderFloat("Glow Intensity", &m_glowIntensity, 0.0f, 1.0f);
        
        ImGui::Spacing();
        ImGui::Text("Filters");
        ImGui::Separator();
        
        ImGui::Checkbox("Show Favorites Only", &m_showFavoritesOnly);
        ImGui::Checkbox("Show Installed Only", &m_showInstalledOnly);
        
        ImGui::Spacing();
        ImGui::Text("About");
        ImGui::Separator();
        ImGui::TextWrapped("DearDoor is a Steam game launcher that organizes your games as doors in an immersive hallway environment.");
        
        if (ImGui::Button("Close", ImVec2(100, 30))) {
            m_showSettings = false;
        }
        
        ImGui::End();
    }
    
    void RenderGameDetails() {
        if (m_selectedGameId == 0) return;
        
        // Find game
        auto it = std::find_if(m_games.begin(), m_games.end(),
                              [this](const DearDoorGameInfo& game) {
                                  return game.steamAppId == m_selectedGameId;
                              });
        
        if (it == m_games.end()) return;
        
        DearDoorGameInfo& game = *it;
        
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Game Details", &m_showGameDetails);
        
        ImGui::Text("%s", game.name.c_str());
        ImGui::Separator();
        
        ImGui::Text("Status: ");
        ImGui::SameLine();
        switch (game.doorState) {
            case DoorState::PLAYING:
                ImGui::TextColored(DearDoorConfig::COLOR_GLOW_GOLD, "Playing");
                break;
            case DoorState::LOCKED:
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Locked");
                break;
            default:
                ImGui::TextColored(DearDoorConfig::COLOR_GLOW_GREEN, "Ready");
                break;
        }
        
        ImGui::Text("Installed: %s", game.isInstalled ? "Yes" : "No");
        ImGui::Text("Install Directory: %s", game.installDirectory.c_str());
        
        if (game.isInstalled && game.doorState != DoorState::PLAYING) {
            if (ImGui::Button("Launch Game", ImVec2(150, 40))) {
                LaunchGame(game.steamAppId);
                m_showGameDetails = false;
            }
        }
        
        if (ImGui::Button("Close", ImVec2(100, 30))) {
            m_showGameDetails = false;
        }
        
        ImGui::End();
    }
    
    void LaunchGame(AppId_t appId) {
        // Find game
        auto it = std::find_if(m_games.begin(), m_games.end(),
                              [appId](const DearDoorGameInfo& game) {
                                  return game.steamAppId == appId;
                              });
        
        if (it == m_games.end()) return;
        
        DearDoorGameInfo& game = *it;
        
        // Update door state
        game.doorState = DoorState::OPENING;
        
        // Start animation
        // m_doorRenderer.StartOpeningAnimation(appId);
        
        // Launch through Steam
        if (SteamManager::GetInstance().LaunchGame(appId)) {
            game.doorState = DoorState::PLAYING;
            std::cout << "Launched game: " << game.name << std::endl;
        } else {
            game.doorState = DoorState::ERROR;
            std::cerr << "Failed to launch game: " << game.name << std::endl;
        }
    }
    
    // Additional member variables
    float m_doorScale = 1.0f;
    float m_glowIntensity = 0.6f;
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    std::cout << "========================================" << std::endl;
    std::cout << "         DearDoor - Game Portal         " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Connect to your Steam games through doors" << std::endl;
    std::cout << std::endl;
    
    DearDoorApplication app;
    
    if (!app.Initialize()) {
        std::cerr << "Failed to initialize DearDoor" << std::endl;
        std::cerr << "Make sure Steam is running" << std::endl;
        system("pause");
        return -1;
    }
    
    std::cout << "DearDoor is running!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Arrow Keys: Navigate hallways" << std::endl;
    std::cout << "  - Click Door: Open game" << std::endl;
    std::cout << "  - F1: Hallway View" << std::endl;
    std::cout << "  - F2: Grid View" << std::endl;
    std::cout << "  - F3: List View" << std::endl;
    std::cout << "  - F10: Settings" << std::endl;
    std::cout << "  - ESC: Exit" << std::endl;
    
    app.Run();
    
    std::cout << "DearDoor closed. Goodbye!" << std::endl;
    return 0;
}