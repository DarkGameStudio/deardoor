// portal.cpp
// DarkDearDoor Portal System - Extends SteamGameRoom
// Provides dimensional portal navigation between game rooms

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

// Steamworks
#include <steam/steam_api.h>

// GLFW
#include <GLFW/glfw3.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Portal Theme Extension
// ============================================================================
namespace PortalTheme {
    // Portal-specific colors (DarkDearDoor inspired)
    constexpr ImVec4 PortalBlue      = ImVec4(0.2f, 0.5f, 0.9f, 1.0f);
    constexpr ImVec4 PortalOrange    = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
    constexpr ImVec4 PortalPurple    = ImVec4(0.6f, 0.3f, 0.8f, 1.0f);
    constexpr ImVec4 PortalGreen     = ImVec4(0.2f, 0.8f, 0.4f, 1.0f);
    constexpr ImVec4 PortalCyan      = ImVec4(0.0f, 0.8f, 0.8f, 1.0f);
    constexpr ImVec4 PortalRed       = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
    
    // Portal animation states
    constexpr float PortalAnimSpeed  = 2.0f;
    constexpr float PortalGlowPulse  = 0.5f;
    constexpr int PortalSegments     = 64;
    
    // Portal dimensions
    constexpr float PortalWidth      = 300.0f;
    constexpr float PortalHeight     = 420.0f;
    constexpr float PortalRingThickness = 8.0f;
}

// ============================================================================
// Portal Effects System
// ============================================================================
class PortalEffects {
public:
    struct Particle {
        ImVec2 position;
        ImVec2 velocity;
        float life;
        float maxLife;
        ImVec4 color;
        float size;
    };
    
    struct PortalVisual {
        ImVec2 center;
        float radius;
        float rotation;
        float pulsePhase;
        ImVec4 primaryColor;
        ImVec4 secondaryColor;
        std::vector<Particle> particles;
        bool isActive;
        std::string targetGame;
        AppId_t targetAppId;
    };
    
    PortalEffects() {
        InitializePortalVisuals();
    }
    
    void InitializePortalVisuals() {
        // Create default portals
        m_portals.clear();
        
        // Main hub portal (center)
        PortalVisual hubPortal;
        hubPortal.center = ImVec2(Theme::WindowWidth * 0.5f, Theme::WindowHeight * 0.5f);
        hubPortal.radius = 150.0f;
        hubPortal.rotation = 0.0f;
        hubPortal.pulsePhase = 0.0f;
        hubPortal.primaryColor = PortalTheme::PortalBlue;
        hubPortal.secondaryColor = PortalTheme::PortalCyan;
        hubPortal.isActive = true;
        hubPortal.targetGame = "Game Hub";
        hubPortal.targetAppId = 0;
        m_portals.push_back(hubPortal);
        
        // Create orbiting portals for games
        CreateGamePortals();
    }
    
    void CreateGamePortals() {
        auto games = SteamManager::GetInstance().GetOwnedGames();
        int portalCount = std::min(static_cast<int>(games.size()), 12);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
        
        for (int i = 0; i < portalCount; i++) {
            PortalVisual portal;
            
            // Position portals in a circle around hub
            float angle = (2.0f * 3.14159f * i) / portalCount;
            float orbitRadius = 250.0f + (i % 3) * 50.0f;
            portal.center = ImVec2(
                Theme::WindowWidth * 0.5f + cos(angle) * orbitRadius,
                Theme::WindowHeight * 0.5f + sin(angle) * orbitRadius
            );
            
            portal.radius = 60.0f + (i % 4) * 10.0f;
            portal.rotation = angle;
            portal.pulsePhase = i * 0.5f;
            
            // Assign colors based on index
            ImVec4 colors[] = {
                PortalTheme::PortalOrange,
                PortalTheme::PortalPurple,
                PortalTheme::PortalGreen,
                PortalTheme::PortalCyan,
                PortalTheme::PortalRed,
                PortalTheme::PortalBlue
            };
            portal.primaryColor = colors[i % 6];
            portal.secondaryColor = colors[(i + 2) % 6];
            
            portal.isActive = games[i].isInstalled;
            portal.targetGame = games[i].name;
            portal.targetAppId = games[i].appId;
            
            InitializePortalParticles(portal);
            m_portals.push_back(portal);
        }
    }
    
    void InitializePortalParticles(PortalVisual& portal) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
        std::uniform_real_distribution<float> speedDist(20.0f, 80.0f);
        std::uniform_real_distribution<float> lifeDist(0.5f, 2.0f);
        
        const int particleCount = 50;
        portal.particles.clear();
        
        for (int i = 0; i < particleCount; i++) {
            Particle particle;
            float angle = angleDist(gen);
            float speed = speedDist(gen);
            
            particle.position = portal.center;
            particle.velocity = ImVec2(cos(angle) * speed, sin(angle) * speed);
            particle.life = 0.0f;
            particle.maxLife = lifeDist(gen);
            particle.color = portal.primaryColor;
            particle.size = 2.0f + (i % 3);
            portal.particles.push_back(particle);
        }
    }
    
    void Update(float deltaTime) {
        for (auto& portal : m_portals) {
            portal.rotation += deltaTime * PortalTheme::PortalAnimSpeed;
            portal.pulsePhase += deltaTime * PortalTheme::PortalGlowPulse;
            
            // Update particles
            UpdateParticles(portal, deltaTime);
        }
    }
    
    void UpdateParticles(PortalVisual& portal, float deltaTime) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.14159f);
        
        for (auto& particle : portal.particles) {
            particle.life += deltaTime;
            particle.position.x += particle.velocity.x * deltaTime;
            particle.position.y += particle.velocity.y * deltaTime;
            
            // Reset dead particles
            if (particle.life >= particle.maxLife) {
                float angle = angleDist(gen);
                float speed = 20.0f + (rand() % 60);
                
                particle.position = portal.center;
                particle.velocity = ImVec2(cos(angle) * speed, sin(angle) * speed);
                particle.life = 0.0f;
            }
            
            // Fade out particles
            float lifeRatio = particle.life / particle.maxLife;
            particle.color.w = 1.0f - lifeRatio;
        }
    }
    
    void DrawPortal(PortalVisual& portal) {
        if (!portal.isActive) return;
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw particles
        for (const auto& particle : portal.particles) {
            ImU32 particleColor = ImGui::ColorConvertFloat4ToU32(particle.color);
            drawList->AddCircleFilled(particle.position, particle.size, particleColor);
        }
        
        // Draw portal rings
        float pulseRadius = portal.radius + sin(portal.pulsePhase * 3.0f) * 8.0f;
        
        // Outer glow
        for (int i = 0; i < 3; i++) {
            float glowRadius = pulseRadius + i * 5.0f;
            float alpha = 0.3f - i * 0.08f;
            ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(portal.primaryColor.x, portal.primaryColor.y, portal.primaryColor.z, alpha)
            );
            drawList->AddCircle(portal.center, glowRadius, glowColor, PortalTheme::PortalSegments, 2.0f);
        }
        
        // Main ring
        ImU32 ringColor = ImGui::ColorConvertFloat4ToU32(portal.primaryColor);
        drawList->AddCircle(portal.center, pulseRadius, ringColor, PortalTheme::PortalSegments, PortalTheme::PortalRingThickness);
        
        // Secondary ring
        ImU32 secondaryRingColor = ImGui::ColorConvertFloat4ToU32(portal.secondaryColor);
        drawList->AddCircle(portal.center, pulseRadius * 0.75f, secondaryRingColor, PortalTheme::PortalSegments, PortalTheme::PortalRingThickness * 0.6f);
        
        // Rotating segments
        const int segmentCount = 8;
        for (int i = 0; i < segmentCount; i++) {
            float angle = portal.rotation + (2.0f * 3.14159f * i) / segmentCount;
            ImVec2 segmentStart = ImVec2(
                portal.center.x + cos(angle) * pulseRadius * 0.9f,
                portal.center.y + sin(angle) * pulseRadius * 0.9f
            );
            ImVec2 segmentEnd = ImVec2(
                portal.center.x + cos(angle) * pulseRadius,
                portal.center.y + sin(angle) * pulseRadius
            );
            drawList->AddLine(segmentStart, segmentEnd, ringColor, 3.0f);
        }
        
        // Inner swirling effect
        for (int i = 0; i < PortalTheme::PortalSegments / 2; i++) {
            float angle1 = portal.rotation + i * 0.2f;
            float angle2 = angle1 + 0.1f;
            float innerRadius = pulseRadius * 0.3f;
            float outerRadius = pulseRadius * 0.6f;
            
            ImVec2 p1 = ImVec2(
                portal.center.x + cos(angle1) * innerRadius,
                portal.center.y + sin(angle1) * innerRadius
            );
            ImVec2 p2 = ImVec2(
                portal.center.x + cos(angle2) * outerRadius,
                portal.center.y + sin(angle2) * outerRadius
            );
            
            ImU32 swirlColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(portal.secondaryColor.x, portal.secondaryColor.y, portal.secondaryColor.z, 0.3f)
            );
            drawList->AddLine(p1, p2, swirlColor, 1.5f);
        }
        
        // Portal center glow
        ImU32 centerGlow = ImGui::ColorConvertFloat4ToU32(
            ImVec4(portal.primaryColor.x, portal.primaryColor.y, portal.primaryColor.z, 0.2f)
        );
        drawList->AddCircleFilled(portal.center, pulseRadius * 0.25f, centerGlow);
        
        // Game name label
        if (!portal.targetGame.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(portal.targetGame.c_str());
            ImVec2 textPos = ImVec2(
                portal.center.x - textSize.x * 0.5f,
                portal.center.y + pulseRadius + 15.0f
            );
            
            // Text background
            ImVec2 bgMin = ImVec2(textPos.x - 5, textPos.y - 3);
            ImVec2 bgMax = ImVec2(textPos.x + textSize.x + 5, textPos.y + textSize.y + 3);
            ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(0.0f, 0.0f, 0.0f, 0.7f)
            );
            drawList->AddRectFilled(bgMin, bgMax, bgColor, 4.0f);
            
            ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(portal.primaryColor.x, portal.primaryColor.y, portal.primaryColor.z, 1.0f)
            );
            drawList->AddText(textPos, textColor, portal.targetGame.c_str());
        }
    }
    
    void DrawAllPortals() {
        for (auto& portal : m_portals) {
            DrawPortal(portal);
        }
    }
    
    bool IsPointInPortal(const ImVec2& point, PortalVisual& portal) {
        float dx = point.x - portal.center.x;
        float dy = point.y - portal.center.y;
        float distance = sqrt(dx * dx + dy * dy);
        return distance <= portal.radius && portal.isActive;
    }
    
    PortalVisual* GetPortalAtPosition(const ImVec2& position) {
        for (auto& portal : m_portals) {
            if (IsPointInPortal(position, portal)) {
                return &portal;
            }
        }
        return nullptr;
    }
    
    std::vector<PortalVisual>& GetPortals() { return m_portals; }
    
private:
    std::vector<PortalVisual> m_portals;
};

// ============================================================================
// Portal Transition Manager
// ============================================================================
class PortalTransitionManager {
public:
    enum class TransitionState {
        Idle,
        FadingOut,
        PortalTravel,
        FadingIn
    };
    
    PortalTransitionManager() = default;
    
    void StartTransition(AppId_t targetAppId, const std::string& targetName) {
        if (m_isTransitioning) return;
        
        m_isTransitioning = true;
        m_state = TransitionState::FadingOut;
        m_transitionProgress = 0.0f;
        m_targetAppId = targetAppId;
        m_targetGameName = targetName;
        
        std::cout << "Portal transition initiated to: " << targetName << std::endl;
    }
    
    void Update(float deltaTime) {
        if (!m_isTransitioning) return;
        
        const float transitionSpeed = 2.0f;
        
        switch (m_state) {
            case TransitionState::FadingOut:
                m_transitionProgress += deltaTime * transitionSpeed;
                if (m_transitionProgress >= 1.0f) {
                    m_transitionProgress = 0.0f;
                    m_state = TransitionState::PortalTravel;
                    
                    // Launch the game
                    if (m_targetAppId != 0) {
                        SteamManager::GetInstance().LaunchGame(m_targetAppId);
                    }
                }
                break;
                
            case TransitionState::PortalTravel:
                m_transitionProgress += deltaTime * transitionSpeed * 0.5f;
                if (m_transitionProgress >= 1.0f) {
                    m_transitionProgress = 0.0f;
                    m_state = TransitionState::FadingIn;
                }
                break;
                
            case TransitionState::FadingIn:
                m_transitionProgress += deltaTime * transitionSpeed;
                if (m_transitionProgress >= 1.0f) {
                    m_isTransitioning = false;
                    m_state = TransitionState::Idle;
                    m_transitionProgress = 0.0f;
                    
                    std::cout << "Portal transition complete" << std::endl;
                }
                break;
                
            default:
                break;
        }
    }
    
    void DrawTransitionOverlay() {
        if (!m_isTransitioning) return;
        
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        
        float alpha = 0.0f;
        ImVec4 transitionColor = PortalTheme::PortalBlue;
        
        switch (m_state) {
            case TransitionState::FadingOut:
                alpha = m_transitionProgress;
                transitionColor = PortalTheme::PortalBlue;
                break;
            case TransitionState::PortalTravel:
                alpha = 1.0f;
                transitionColor = PortalTheme::PortalPurple;
                DrawPortalTravelEffect(drawList, screenSize);
                break;
            case TransitionState::FadingIn:
                alpha = 1.0f - m_transitionProgress;
                transitionColor = PortalTheme::PortalGreen;
                break;
            default:
                break;
        }
        
        // Draw overlay
        ImU32 overlayColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(transitionColor.x, transitionColor.y, transitionColor.z, alpha)
        );
        drawList->AddRectFilled(ImVec2(0, 0), screenSize, overlayColor);
        
        // Draw transition text
        if (m_state == TransitionState::PortalTravel) {
            std::string text = "Traveling to " + m_targetGameName + "...";
            ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
            ImVec2 textPos = ImVec2(
                (screenSize.x - textSize.x) * 0.5f,
                (screenSize.y - textSize.y) * 0.5f
            );
            
            ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
            );
            drawList->AddText(ImGui::GetFont(), 24.0f, textPos, textColor, text.c_str());
        }
    }
    
    bool IsTransitioning() const { return m_isTransitioning; }
    
private:
    void DrawPortalTravelEffect(ImDrawList* drawList, ImVec2 screenSize) {
        // Draw swirling portal effect during transition
        ImVec2 center = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);
        float time = static_cast<float>(glfwGetTime());
        
        // Draw expanding rings
        for (int i = 0; i < 5; i++) {
            float radius = 50.0f + (time * 200.0f + i * 40.0f);
            radius = fmod(radius, 400.0f);
            
            ImU32 ringColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(PortalTheme::PortalCyan.x, 
                       PortalTheme::PortalCyan.y, 
                       PortalTheme::PortalCyan.z, 
                       0.5f - (radius / 400.0f) * 0.5f)
            );
            
            drawList->AddCircle(center, radius, ringColor, 64, 3.0f);
        }
        
        // Draw spiral effect
        for (int i = 0; i < 20; i++) {
            float angle = time * 3.0f + i * 0.3f;
            float radius1 = i * 15.0f;
            float radius2 = radius1 + 20.0f;
            
            ImVec2 p1 = ImVec2(center.x + cos(angle) * radius1, center.y + sin(angle) * radius1);
            ImVec2 p2 = ImVec2(center.x + cos(angle + 0.1f) * radius2, center.y + sin(angle + 0.1f) * radius2);
            
            ImU32 spiralColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(PortalTheme::PortalPurple.x, 
                       PortalTheme::PortalPurple.y, 
                       PortalTheme::PortalPurple.z, 
                       0.3f)
            );
            
            drawList->AddLine(p1, p2, spiralColor, 2.0f);
        }
    }
    
    bool m_isTransitioning = false;
    TransitionState m_state = TransitionState::Idle;
    float m_transitionProgress = 0.0f;
    AppId_t m_targetAppId = 0;
    std::string m_targetGameName;
};

// ============================================================================
// Portal Room Window (Main Portal Interface)
// ============================================================================
class PortalRoomWindow {
public:
    PortalRoomWindow() {
        m_portalEffects = std::make_unique<PortalEffects>();
        m_transitionManager = std::make_unique<PortalTransitionManager>();
    }
    
    bool Initialize() {
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        m_window = glfwCreateWindow(
            static_cast<int>(Theme::WindowWidth),
            static_cast<int>(Theme::WindowHeight),
            "DarkDearDoor Portal Room",
            nullptr, nullptr
        );
        
        if (!m_window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(m_window, this);
        
        // Setup callbacks
        glfwSetMouseButtonCallback(m_window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_window, CursorPosCallback);
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        SetupPortalStyle();
        
        // Initialize Steam
        if (!SteamManager::GetInstance().Initialize()) {
            std::cerr << "Failed to initialize Steam" << std::endl;
            return false;
        }
        
        m_lastFrameTime = glfwGetTime();
        return true;
    }
    
    void Run() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();
            
            // Calculate delta time
            double currentTime = glfwGetTime();
            float deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
            m_lastFrameTime = currentTime;
            
            // Update portal effects
            m_portalEffects->Update(deltaTime);
            m_transitionManager->Update(deltaTime);
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Draw portal room
            DrawPortalRoom();
            
            // Handle portal interactions
            HandlePortalInteractions();
            
            // Draw transition overlay
            m_transitionManager->DrawTransitionOverlay();
            
            // Render
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            
            glClearColor(Theme::Background.r, Theme::Background.g, 
                        Theme::Background.b, Theme::Background.a);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(m_window);
            
            // Process Steam callbacks
            SteamAPI_RunCallbacks();
        }
    }
    
    void Shutdown() {
        SteamManager::GetInstance().Shutdown();
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }
    
private:
    void SetupPortalStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Dark theme similar to GameRoomWindow
        style.Colors[ImGuiCol_WindowBg] = ImVec4(Theme::Background.r, Theme::Background.g, 
                                                 Theme::Background.b, Theme::Background.a);
        style.Colors[ImGuiCol_Text] = ImVec4(Theme::TextPrimary.r, Theme::TextPrimary.g, 
                                             Theme::TextPrimary.b, Theme::TextPrimary.a);
        style.Colors[ImGuiCol_Border] = ImVec4(Theme::Border.r, Theme::Border.g, 
                                               Theme::Border.b, Theme::Border.a);
        
        style.WindowRounding = Theme::CornerRadius;
        style.FrameRounding = Theme::CornerRadius * 0.5f;
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 8);
    }
    
    void DrawPortalRoom() {
        // Fullscreen window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Portal Room", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBackground);
        
        // Draw background grid
        DrawBackgroundGrid();
        
        // Draw portals
        m_portalEffects->DrawAllPortals();
        
        // Draw header
        DrawPortalHeader();
        
        // Draw instructions
        DrawInstructions();
        
        ImGui::End();
    }
    
    void DrawBackgroundGrid() {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        
        // Draw subtle grid
        const float gridSpacing = 50.0f;
        ImU32 gridColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.3f, 0.3f, 0.35f, 0.1f)
        );
        
        for (float x = 0; x < screenSize.x; x += gridSpacing) {
            drawList->AddLine(ImVec2(x, 0), ImVec2(x, screenSize.y), gridColor, 1.0f);
        }
        
        for (float y = 0; y < screenSize.y; y += gridSpacing) {
            drawList->AddLine(ImVec2(0, y), ImVec2(screenSize.x, y), gridColor, 1.0f);
        }
    }
    
    void DrawPortalHeader() {
        ImGui::SetCursorPosY(10);
        ImGui::SetCursorPosX(20);
        
        ImGui::TextColored(PortalTheme::PortalCyan, "DARKDEARDOOR PORTAL ROOM");
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(Theme::WindowWidth - 300);
        
        std::string portalInfo = "Active Portals: " + 
                                 std::to_string(m_portalEffects->GetPortals().size());
        ImGui::TextColored(ImVec4(Theme::TextSecondary.r, Theme::TextSecondary.g, 
                                  Theme::TextSecondary.b, Theme::TextSecondary.a),
                          portalInfo.c_str());
        
        ImGui::Separator();
    }
    
    void DrawInstructions() {
        ImGui::SetCursorPosY(Theme::WindowHeight - 60);
        ImGui::SetCursorPosX(20);
        
        ImGui::TextColored(ImVec4(Theme::TextSecondary.r, Theme::TextSecondary.g, 
                                  Theme::TextSecondary.b, Theme::TextSecondary.a),
                          "Click on a portal to enter the game room | ESC to exit | F5 to refresh");
    }
    
    void HandlePortalInteractions() {
        // Check for mouse clicks on portals
        ImGuiIO& io = ImGui::GetIO();
        
        if (io.MouseClicked[0]) {  // Left click
            ImVec2 mousePos = io.MousePos;
            PortalEffects::PortalVisual* clickedPortal = 
                m_portalEffects->GetPortalAtPosition(mousePos);
            
            if (clickedPortal && clickedPortal->isActive) {
                std::cout << "Portal clicked: " << clickedPortal->targetGame << std::endl;
                
                if (clickedPortal->targetAppId != 0) {
                    // Start portal transition
                    m_transitionManager->StartTransition(
                        clickedPortal->targetAppId,
                        clickedPortal->targetGame
                    );
                }
            }
        }
        
        // Check for keyboard shortcuts
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F5))) {
            m_portalEffects->CreateGamePortals();
        }
        
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
            glfwSetWindowShouldClose(m_window, true);
        }
    }
    
    // GLFW callbacks
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        PortalRoomWindow* self = static_cast<PortalRoomWindow*>(
            glfwGetWindowUserPointer(window)
        );
        if (self) {
            // Handle mouse events if needed
        }
    }
    
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        PortalRoomWindow* self = static_cast<PortalRoomWindow*>(
            glfwGetWindowUserPointer(window)
        );
        if (self) {
            self->m_mousePosition = ImVec2(static_cast<float>(xpos), static_cast<float>(ypos));
        }
    }
    
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<PortalEffects> m_portalEffects;
    std::unique_ptr<PortalTransitionManager> m_transitionManager;
    ImVec2 m_mousePosition;
    double m_lastFrameTime = 0.0;
};

// ============================================================================
// Portal Room Entry Point
// ============================================================================
int PortalMain() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    std::cout << "=== DarkDearDoor Portal Room ===" << std::endl;
    std::cout << "Initializing dimensional portal system..." << std::endl;
    
    PortalRoomWindow portalRoom;
    
    if (!portalRoom.Initialize()) {
        std::cerr << "Failed to initialize portal room" << std::endl;
        std::cerr << "Make sure Steam is running and libraries are available" << std::endl;
        return -1;
    }
    
    std::cout << "Portal room initialized successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Click portal to enter game" << std::endl;
    std::cout << "  - F5 to refresh portals" << std::endl;
    std::cout << "  - ESC to exit" << std::endl;
    
    portalRoom.Run();
    
    std::cout << "Closing portal room..." << std::endl;
    return 0;
}

// If this file is compiled standalone, use PortalMain as entry point
#ifdef PORTAL_STANDALONE
int main(int argc, char** argv) {
    return PortalMain();
}
#endif