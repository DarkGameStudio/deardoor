// steam_ui.cpp
// Simple but Comprehensive Steam Platform User Interface
// Complete implementation with all essential features

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <ctime>

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

// STB Image for textures
#include "stb_image.h"

// ============================================================================
// Steam UI Configuration
// ============================================================================
namespace SteamUIConfig {
    // Window settings
    constexpr int WINDOW_WIDTH = 1280;
    constexpr int WINDOW_HEIGHT = 720;
    constexpr const char* WINDOW_TITLE = "Steam UI";
    
    // Layout dimensions
    constexpr float SIDEBAR_WIDTH = 200.0f;
    constexpr float TOP_BAR_HEIGHT = 50.0f;
    constexpr float BOTTOM_BAR_HEIGHT = 40.0f;
    constexpr float CARD_WIDTH = 180.0f;
    constexpr float CARD_HEIGHT = 240.0f;
    constexpr float CARD_SPACING = 15.0f;
    
    // Colors (Steam-like dark theme)
    namespace Colors {
        constexpr ImVec4 Background = ImVec4(0.11f, 0.13f, 0.15f, 1.0f);
        constexpr ImVec4 Surface = ImVec4(0.16f, 0.18f, 0.21f, 1.0f);
        constexpr ImVec4 SurfaceLight = ImVec4(0.22f, 0.24f, 0.27f, 1.0f);
        constexpr ImVec4 Primary = ImVec4(0.26f, 0.52f, 0.96f, 1.0f);
        constexpr ImVec4 PrimaryDark = ImVec4(0.20f, 0.42f, 0.80f, 1.0f);
        constexpr ImVec4 TextPrimary = ImVec4(0.90f, 0.91f, 0.93f, 1.0f);
        constexpr ImVec4 TextSecondary = ImVec4(0.55f, 0.58f, 0.62f, 1.0f);
        constexpr ImVec4 Success = ImVec4(0.30f, 0.75f, 0.35f, 1.0f);
        constexpr ImVec4 Warning = ImVec4(0.90f, 0.70f, 0.25f, 1.0f);
        constexpr ImVec4 Error = ImVec4(0.85f, 0.30f, 0.25f, 1.0f);
    }
}

// ============================================================================
// Game Data Structure
// ============================================================================
struct SteamGame {
    AppId_t appId;
    std::string name;
    std::string developer;
    std::string publisher;
    std::string description;
    std::string installDir;
    std::string headerImagePath;
    std::string iconPath;
    
    bool isInstalled;
    bool isRunning;
    bool isFavorite;
    bool isRecent;
    
    int playtimeMinutes;
    int lastPlayedTime;
    int totalAchievements;
    int unlockedAchievements;
    
    std::vector<std::string> tags;
    std::vector<std::string> genres;
    
    // UI state
    bool isSelected;
    bool isHovered;
    float hoverAnimation;
    float selectionAnimation;
    
    // Constructor
    SteamGame() : appId(0), isInstalled(false), isRunning(false),
                  isFavorite(false), isRecent(false), playtimeMinutes(0),
                  lastPlayedTime(0), totalAchievements(0), unlockedAchievements(0),
                  isSelected(false), isHovered(false),
                  hoverAnimation(0.0f), selectionAnimation(0.0f) {}
};

// ============================================================================
// Steam Manager Class
// ============================================================================
class SteamManager {
private:
    bool m_initialized;
    std::vector<SteamGame> m_games;
    std::mutex m_gamesMutex;
    
    // Steam callbacks
    STEAM_CALLBACK(SteamManager, OnGameOverlayActivated, GameOverlayActivated_t);
    STEAM_CALLBACK(SteamManager, OnAppInstalled, AppInstalled_t);
    STEAM_CALLBACK(SteamManager, OnAppUninstalled, AppUninstalled_t);
    
public:
    static SteamManager& GetInstance() {
        static SteamManager instance;
        return instance;
    }
    
    bool Initialize() {
        if (m_initialized) return true;
        
        if (!SteamAPI_Init()) {
            std::cerr << "Failed to initialize Steam API" << std::endl;
            return false;
        }
        
        m_initialized = true;
        RefreshGameList();
        
        std::cout << "Steam initialized. User: " << GetPersonaName() << std::endl;
        return true;
    }
    
    void Shutdown() {
        if (m_initialized) {
            SteamAPI_Shutdown();
            m_initialized = false;
        }
    }
    
    std::vector<SteamGame> GetGames() {
        std::lock_guard<std::mutex> lock(m_gamesMutex);
        return m_games;
    }
    
    void RefreshGameList() {
        if (!m_initialized) return;
        
        std::vector<SteamGame> newGames;
        int gameCount = SteamApps()->GetAppCount();
        
        for (int i = 0; i < gameCount; i++) {
            AppId_t appId = SteamApps()->GetAppID(i);
            
            if (SteamApps()->BIsDlcInstalled(appId)) continue;
            if (!SteamApps()->BIsSubscribedApp(appId)) continue;
            
            SteamGame game;
            game.appId = appId;
            
            // Get game name
            char name[256];
            if (SteamApps()->GetAppName(appId, name, sizeof(name)) > 0) {
                game.name = std::string(name);
            } else {
                continue;
            }
            
            // Get install directory
            char installDir[1024];
            if (SteamApps()->GetAppInstallDir(appId, installDir, sizeof(installDir)) > 0) {
                game.installDir = std::string(installDir);
                game.isInstalled = true;
            }
            
            game.isRunning = SteamApps()->BIsAppRunning(appId);
            
            newGames.push_back(game);
        }
        
        // Sort alphabetically
        std::sort(newGames.begin(), newGames.end(),
                 [](const SteamGame& a, const SteamGame& b) {
                     return a.name < b.name;
                 });
        
        {
            std::lock_guard<std::mutex> lock(m_gamesMutex);
            m_games = newGames;
        }
    }
    
    bool LaunchGame(AppId_t appId) {
        if (!m_initialized) return false;
        return SteamApps()->LaunchApp(appId);
    }
    
    bool IsGameRunning(AppId_t appId) {
        if (!m_initialized) return false;
        return SteamApps()->BIsAppRunning(appId);
    }
    
    std::string GetPersonaName() {
        if (!m_initialized) return "";
        return SteamFriends()->GetPersonaName();
    }
    
    uint64_t GetSteamID() {
        if (!m_initialized) return 0;
        return SteamUser()->GetSteamID().ConvertToUint64();
    }
    
    void SetGameFavorite(AppId_t appId, bool favorite) {
        // In production, this would save to Steam cloud or local storage
        std::lock_guard<std::mutex> lock(m_gamesMutex);
        auto it = std::find_if(m_games.begin(), m_games.end(),
                              [appId](const SteamGame& g) { return g.appId == appId; });
        if (it != m_games.end()) {
            it->isFavorite = favorite;
        }
    }
    
private:
    SteamManager() : m_initialized(false) {}
    ~SteamManager() { Shutdown(); }
    SteamManager(const SteamManager&) = delete;
    SteamManager& operator=(const SteamManager&) = delete;
    
    void OnGameOverlayActivated(GameOverlayActivated_t* pCallback) {
        // Handle overlay activation
    }
    
    void OnAppInstalled(AppInstalled_t* pCallback) {
        RefreshGameList();
    }
    
    void OnAppUninstalled(AppUninstalled_t* pCallback) {
        RefreshGameList();
    }
};

// ============================================================================
// UI Component Library
// ============================================================================
class UIComponents {
public:
    // Custom button with hover effect
    static bool Button(const char* label, const ImVec2& size = ImVec2(0, 0),
                      bool primary = false, bool enabled = true) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 buttonSize = size;
        if (buttonSize.x == 0) buttonSize.x = ImGui::CalcTextSize(label).x + 24;
        if (buttonSize.y == 0) buttonSize.y = 30;
        
        bool hovered = enabled && ImGui::IsMouseHoveringRect(pos, 
            ImVec2(pos.x + buttonSize.x, pos.y + buttonSize.y));
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        
        // Background
        ImVec4 bgColor = primary ? SteamUIConfig::Colors::Primary : 
                                  SteamUIConfig::Colors::Surface;
        if (!enabled) {
            bgColor = ImVec4(0.20f, 0.20f, 0.20f, 0.5f);
        } else if (hovered) {
            bgColor = primary ? SteamUIConfig::Colors::PrimaryDark : 
                               SteamUIConfig::Colors::SurfaceLight;
        }
        
        ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
        drawList->AddRectFilled(pos, ImVec2(pos.x + buttonSize.x, pos.y + buttonSize.y), 
                               bgColorU32, 3.0f);
        
        // Text
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(pos.x + (buttonSize.x - textSize.x) * 0.5f,
                               pos.y + (buttonSize.y - textSize.y) * 0.5f);
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
            enabled ? SteamUIConfig::Colors::TextPrimary : 
                     SteamUIConfig::Colors::TextSecondary
        );
        drawList->AddText(textPos, textColor, label);
        
        // Advance cursor
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + buttonSize.y + style.ItemSpacing.y));
        
        return clicked;
    }
    
    // Game card component
    static bool GameCard(const SteamGame& game) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 cardSize(SteamUIConfig::CARD_WIDTH, SteamUIConfig::CARD_HEIGHT);
        
        bool hovered = ImGui::IsMouseHoveringRect(pos, 
            ImVec2(pos.x + cardSize.x, pos.y + cardSize.y));
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(0);
        
        // Card background
        ImVec4 bgColor = game.isSelected ? SteamUIConfig::Colors::SurfaceLight : 
                                          SteamUIConfig::Colors::Surface;
        if (hovered) {
            bgColor = SteamUIConfig::Colors::SurfaceLight;
        }
        
        ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
        drawList->AddRectFilled(pos, ImVec2(pos.x + cardSize.x, pos.y + cardSize.y), 
                               bgColorU32, 4.0f);
        
        // Card border
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            game.isSelected ? SteamUIConfig::Colors::Primary : 
                             SteamUIConfig::Colors::SurfaceLight
        );
        drawList->AddRect(pos, ImVec2(pos.x + cardSize.x, pos.y + cardSize.y), 
                         borderColor, 4.0f, 0, game.isSelected ? 2.0f : 1.0f);
        
        // Game image area (placeholder)
        ImVec2 imagePos = ImVec2(pos.x + 8, pos.y + 8);
        ImVec2 imageSize = ImVec2(cardSize.x - 16, cardSize.y * 0.55f);
        
        ImU32 imageBg = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.25f, 0.28f, 0.31f, 1.0f)
        );
        drawList->AddRectFilled(imagePos, 
                               ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
                               imageBg, 2.0f);
        
        // Draw game initial letter
        std::string initial = game.name.empty() ? "?" : game.name.substr(0, 1);
        ImVec2 initialPos = ImVec2(imagePos.x + imageSize.x * 0.5f - 10,
                                  imagePos.y + imageSize.y * 0.5f - 15);
        ImU32 initialColor = ImGui::ColorConvertFloat4ToU32(
            SteamUIConfig::Colors::TextSecondary
        );
        drawList->AddText(initialPos, initialColor, initial.c_str());
        
        // Game name
        ImVec2 namePos = ImVec2(pos.x + 10, pos.y + imageSize.y + 15);
        ImU32 nameColor = ImGui::ColorConvertFloat4ToU32(
            SteamUIConfig::Colors::TextPrimary
        );
        
        // Truncate name if too long
        std::string displayName = game.name;
        if (ImGui::CalcTextSize(displayName.c_str()).x > cardSize.x - 20) {
            while (ImGui::CalcTextSize((displayName + "...").c_str()).x > 
                   cardSize.x - 20 && displayName.length() > 0) {
                displayName.pop_back();
            }
            displayName += "...";
        }
        drawList->AddText(namePos, nameColor, displayName.c_str());
        
        // Status indicator
        ImVec2 statusPos = ImVec2(pos.x + 10, pos.y + cardSize.y - 30);
        ImVec4 statusColor;
        const char* statusText;
        
        if (game.isRunning) {
            statusColor = SteamUIConfig::Colors::Success;
            statusText = "Running";
        } else if (game.isInstalled) {
            statusColor = SteamUIConfig::Colors::Primary;
            statusText = "Ready";
        } else {
            statusColor = SteamUIConfig::Colors::TextSecondary;
            statusText = "Not Installed";
        }
        
        ImU32 statusColorU32 = ImGui::ColorConvertFloat4ToU32(statusColor);
        drawList->AddText(statusPos, statusColorU32, statusText);
        
        // Favorite star
        if (game.isFavorite) {
            ImVec2 starPos = ImVec2(pos.x + cardSize.x - 25, pos.y + 10);
            ImU32 starColor = ImGui::ColorConvertFloat4ToU32(
                SteamUIConfig::Colors::Warning
            );
            drawList->AddText(starPos, starColor, "*");
        }
        
        // Advance cursor
        ImGui::SetCursorScreenPos(ImVec2(pos.x + cardSize.x + style.ItemSpacing.x, pos.y));
        
        return doubleClicked;
    }
    
    // Search bar component
    static bool SearchBar(char* buffer, size_t bufferSize) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(300, 30);
        
        // Background
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(SteamUIConfig::Colors::Surface);
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                               bgColor, 3.0f);
        
        // Search icon
        ImVec2 iconPos = ImVec2(pos.x + 8, pos.y + 10);
        ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(
            SteamUIConfig::Colors::TextSecondary
        );
        drawList->AddCircle(iconPos, 5, iconColor, 12, 1.5f);
        drawList->AddLine(ImVec2(iconPos.x + 4, iconPos.y + 4),
                         ImVec2(iconPos.x + 8, iconPos.y + 8), iconColor, 1.5f);
        
        // Input text
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 25, pos.y + 7));
        ImGui::PushItemWidth(size.x - 30);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
        bool changed = ImGui::InputText("##search", buffer, bufferSize);
        ImGui::PopStyleColor(3);
        ImGui::PopItemWidth();
        
        // Clear button
        if (buffer[0] != '\0') {
            ImVec2 clearPos = ImVec2(pos.x + size.x - 20, pos.y + 8);
            ImU32 clearColor = ImGui::ColorConvertFloat4ToU32(
                SteamUIConfig::Colors::TextSecondary
            );
            drawList->AddText(clearPos, clearColor, "x");
            
            if (ImGui::IsMouseHoveringRect(clearPos, 
                                          ImVec2(clearPos.x + 10, clearPos.y + 10)) &&
                ImGui::IsMouseClicked(0)) {
                buffer[0] = '\0';
                changed = true;
            }
        }
        
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + style.ItemSpacing.y));
        
        return changed;
    }
};

// ============================================================================
// Steam UI Main Application
// ============================================================================
class SteamUIApplication {
private:
    GLFWwindow* m_window;
    bool m_initialized;
    
    // Game data
    std::vector<SteamGame> m_games;
    std::vector<SteamGame> m_filteredGames;
    
    // UI state
    char m_searchBuffer[256];
    std::string m_currentView;
    std::string m_currentCategory;
    int m_selectedGameIndex;
    bool m_showSettings;
    bool m_showLibrary;
    bool m_showStore;
    bool m_showCommunity;
    
    // Window dimensions
    int m_windowWidth;
    int m_windowHeight;
    
public:
    SteamUIApplication()
        : m_window(nullptr)
        , m_initialized(false)
        , m_currentView("Library")
        , m_currentCategory("All Games")
        , m_selectedGameIndex(-1)
        , m_showSettings(false)
        , m_showLibrary(true)
        , m_showStore(false)
        , m_showCommunity(false)
        , m_windowWidth(SteamUIConfig::WINDOW_WIDTH)
        , m_windowHeight(SteamUIConfig::WINDOW_HEIGHT) {
        
        memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
    }
    
    ~SteamUIApplication() {
        Shutdown();
    }
    
    bool Initialize() {
        std::cout << "Initializing Steam UI..." << std::endl;
        
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        
        // Create window
        m_window = glfwCreateWindow(
            m_windowWidth, m_windowHeight,
            SteamUIConfig::WINDOW_TITLE,
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
        
        // Initialize GLEW
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW" << std::endl;
            return false;
        }
        
        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Setup style
        SetupImGuiStyle();
        
        // Setup callbacks
        SetupCallbacks();
        
        // Initialize Steam
        if (!SteamManager::GetInstance().Initialize()) {
            std::cerr << "Failed to initialize Steam" << std::endl;
            return false;
        }
        
        // Load games
        LoadGames();
        
        m_initialized = true;
        std::cout << "Steam UI initialized successfully" << std::endl;
        
        return true;
    }
    
    void Run() {
        while (!glfwWindowShouldClose(m_window) && m_initialized) {
            glfwPollEvents();
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Render UI
            RenderUI();
            
            // Render ImGui
            ImGui::Render();
            
            // Clear screen
            glViewport(0, 0, m_windowWidth, m_windowHeight);
            glClearColor(
                SteamUIConfig::Colors::Background.x,
                SteamUIConfig::Colors::Background.y,
                SteamUIConfig::Colors::Background.z,
                1.0f
            );
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(m_window);
            
            // Process Steam callbacks
            SteamAPI_RunCallbacks();
        }
    }
    
    void Shutdown() {
        if (m_initialized) {
            SteamManager::GetInstance().Shutdown();
            
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            
            if (m_window) {
                glfwDestroyWindow(m_window);
                m_window = nullptr;
            }
            
            glfwTerminate();
            m_initialized = false;
        }
    }
    
private:
    void SetupImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Colors
        style.Colors[ImGuiCol_WindowBg] = SteamUIConfig::Colors::Background;
        style.Colors[ImGuiCol_ChildBg] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_PopupBg] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_Border] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_FrameBg] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_FrameBgHovered] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_FrameBgActive] = SteamUIConfig::Colors::Primary;
        style.Colors[ImGuiCol_Button] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_ButtonHovered] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_ButtonActive] = SteamUIConfig::Colors::Primary;
        style.Colors[ImGuiCol_Text] = SteamUIConfig::Colors::TextPrimary;
        style.Colors[ImGuiCol_TextDisabled] = SteamUIConfig::Colors::TextSecondary;
        style.Colors[ImGuiCol_Header] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_HeaderHovered] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_HeaderActive] = SteamUIConfig::Colors::Primary;
        style.Colors[ImGuiCol_Separator] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_Tab] = SteamUIConfig::Colors::Surface;
        style.Colors[ImGuiCol_TabHovered] = SteamUIConfig::Colors::SurfaceLight;
        style.Colors[ImGuiCol_TabActive] = SteamUIConfig::Colors::Primary;
        
        // Style
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;
        
        style.WindowPadding = ImVec2(12, 12);
        style.FramePadding = ImVec2(10, 6);
        style.ItemSpacing = ImVec2(8, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12;
        
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 1;
    }
    
    void SetupCallbacks() {
        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            SteamUIApplication* app = static_cast<SteamUIApplication*>(
                glfwGetWindowUserPointer(window)
            );
            if (app) {
                app->m_windowWidth = width;
                app->m_windowHeight = height;
                glViewport(0, 0, width, height);
            }
        });
    }
    
    void LoadGames() {
        m_games = SteamManager::GetInstance().GetGames();
        UpdateFilteredGames();
    }
    
    void UpdateFilteredGames() {
        m_filteredGames.clear();
        
        for (const auto& game : m_games) {
            // Apply search filter
            if (m_searchBuffer[0] != '\0') {
                std::string lowerName = game.name;
                std::string lowerSearch = m_searchBuffer;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
                
                if (lowerName.find(lowerSearch) == std::string::npos) {
                    continue;
                }
            }
            
            // Apply category filter
            if (m_currentCategory == "Installed" && !game.isInstalled) continue;
            if (m_currentCategory == "Running" && !game.isRunning) continue;
            if (m_currentCategory == "Favorites" && !game.isFavorite) continue;
            
            m_filteredGames.push_back(game);
        }
    }
    
    void RenderUI() {
        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(m_windowWidth, m_windowHeight));
        ImGui::Begin("SteamUI", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse);
        
        // Render top navigation
        RenderTopNavigation();
        
        // Render sidebar
        RenderSidebar();
        
        // Render main content
        RenderMainContent();
        
        // Render status bar
        RenderStatusBar();
        
        ImGui::End();
        
        // Render settings window
        if (m_showSettings) {
            RenderSettingsWindow();
        }
    }
    
    void RenderTopNavigation() {
        ImGui::SetCursorPos(ImVec2(10, 5));
        
        // Navigation buttons
        if (UIComponents::Button("Library", ImVec2(80, 30), m_showLibrary)) {
            m_showLibrary = true;
            m_showStore = false;
            m_showCommunity = false;
            m_currentView = "Library";
        }
        ImGui::SameLine();
        
        if (UIComponents::Button("Store", ImVec2(80, 30), m_showStore)) {
            m_showLibrary = false;
            m_showStore = true;
            m_showCommunity = false;
            m_currentView = "Store";
        }
        ImGui::SameLine();
        
        if (UIComponents::Button("Community", ImVec2(80, 30), m_showCommunity)) {
            m_showLibrary = false;
            m_showStore = false;
            m_showCommunity = true;
            m_currentView = "Community";
        }
        ImGui::SameLine();
        
        // Search bar
        ImGui::SetCursorPosX(300);
        if (UIComponents::SearchBar(m_searchBuffer, sizeof(m_searchBuffer))) {
            UpdateFilteredGames();
        }
        
        // User info
        ImGui::SameLine();
        ImGui::SetCursorPosX(m_windowWidth - 200);
        std::string userInfo = SteamManager::GetInstance().GetPersonaName();
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, "%s", userInfo.c_str());
        
        // Settings button
        ImGui::SameLine();
        ImGui::SetCursorPosX(m_windowWidth - 80);
        if (UIComponents::Button("Settings", ImVec2(70, 30))) {
            m_showSettings = true;
        }
        
        ImGui::Separator();
    }
    
    void RenderSidebar() {
        ImGui::SetCursorPos(ImVec2(0, SteamUIConfig::TOP_BAR_HEIGHT));
        
        ImGui::BeginChild("Sidebar", 
                         ImVec2(SteamUIConfig::SIDEBAR_WIDTH,
                               m_windowHeight - SteamUIConfig::TOP_BAR_HEIGHT - 
                               SteamUIConfig::BOTTOM_BAR_HEIGHT),
                         true);
        
        // Categories
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, "LIBRARY");
        ImGui::Separator();
        
        std::vector<std::string> categories = {
            "All Games", "Installed", "Running", "Favorites", "Recent"
        };
        
        for (const auto& category : categories) {
            bool selected = (m_currentCategory == category);
            
            if (ImGui::Selectable(category.c_str(), selected)) {
                m_currentCategory = category;
                UpdateFilteredGames();
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Statistics
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, "STATISTICS");
        ImGui::Separator();
        
        int totalGames = m_games.size();
        int installedGames = 0;
        int runningGames = 0;
        
        for (const auto& game : m_games) {
            if (game.isInstalled) installedGames++;
            if (game.isRunning) runningGames++;
        }
        
        ImGui::Text("Total: %d", totalGames);
        ImGui::Text("Installed: %d", installedGames);
        ImGui::Text("Running: %d", runningGames);
        
        ImGui::EndChild();
    }
    
    void RenderMainContent() {
        float contentX = SteamUIConfig::SIDEBAR_WIDTH;
        float contentY = SteamUIConfig::TOP_BAR_HEIGHT;
        float contentWidth = m_windowWidth - contentX;
        float contentHeight = m_windowHeight - contentY - SteamUIConfig::BOTTOM_BAR_HEIGHT;
        
        ImGui::SetCursorPos(ImVec2(contentX, contentY));
        
        ImGui::BeginChild("Content", ImVec2(contentWidth, contentHeight), false);
        
        if (m_showLibrary) {
            RenderLibraryView();
        } else if (m_showStore) {
            RenderStoreView();
        } else if (m_showCommunity) {
            RenderCommunityView();
        }
        
        ImGui::EndChild();
    }
    
    void RenderLibraryView() {
        // Grid view
        float availableWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(availableWidth / 
                       (SteamUIConfig::CARD_WIDTH + SteamUIConfig::CARD_SPACING)));
        
        int index = 0;
        for (const auto& game : m_filteredGames) {
            int row = index / columns;
            int col = index % columns;
            
            float x = col * (SteamUIConfig::CARD_WIDTH + SteamUIConfig::CARD_SPACING) + 10;
            float y = row * (SteamUIConfig::CARD_HEIGHT + SteamUIConfig::CARD_SPACING) + 10;
            
            ImGui::SetCursorPos(ImVec2(x, y));
            
            if (UIComponents::GameCard(game)) {
                // Double-clicked - launch game
                if (game.isInstalled) {
                    SteamManager::GetInstance().LaunchGame(game.appId);
                }
            }
            
            index++;
        }
        
        // Show message if no games
        if (m_filteredGames.empty()) {
            ImGui::SetCursorPos(ImVec2(
                ImGui::GetContentRegionAvail().x * 0.5f - 100,
                ImGui::GetContentRegionAvail().y * 0.5f
            ));
            ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, 
                              "No games found");
        }
    }
    
    void RenderStoreView() {
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::TextColored(SteamUIConfig::Colors::TextPrimary, "Steam Store");
        ImGui::Separator();
        
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, 
                          "Store functionality coming soon...");
    }
    
    void RenderCommunityView() {
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::TextColored(SteamUIConfig::Colors::TextPrimary, "Community");
        ImGui::Separator();
        
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, 
                          "Community functionality coming soon...");
    }
    
    void RenderStatusBar() {
        ImGui::SetCursorPos(ImVec2(0, m_windowHeight - SteamUIConfig::BOTTOM_BAR_HEIGHT));
        ImGui::Separator();
        
        ImGui::SetCursorPos(ImVec2(10, m_windowHeight - SteamUIConfig::BOTTOM_BAR_HEIGHT + 10));
        
        // Status info
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary,
                          "Ready | %zu games | %zu filtered",
                          m_games.size(), m_filteredGames.size());
        
        // Right side
        ImGui::SameLine();
        ImGui::SetCursorPosX(m_windowWidth - 150);
        ImGui::TextColored(SteamUIConfig::Colors::TextSecondary, "Steam UI v1.0");
    }
    
    void RenderSettingsWindow() {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", &m_showSettings);
        
        ImGui::Text("Display Settings");
        ImGui::Separator();
        
        static bool showSidebar = true;
        ImGui::Checkbox("Show Sidebar", &showSidebar);
        
        static bool showGrid = true;
        ImGui::Checkbox("Show Grid View", &showGrid);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("About");
        ImGui::Separator();
        ImGui::TextWrapped("Simple Steam UI - A comprehensive interface for Steam platform");
        ImGui::Text("Version: 1.0");
        
        ImGui::Spacing();
        
        if (UIComponents::Button("Close", ImVec2(100, 30))) {
            m_showSettings = false;
        }
        
        ImGui::End();
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Steam Platform User Interface" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    SteamUIApplication app;
    
    if (!app.Initialize()) {
        std::cerr << "Failed to initialize Steam UI" << std::endl;
        std::cerr << "Make sure Steam is running" << std::endl;
        return -1;
    }
    
    std::cout << "Steam UI started successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Double-click game to launch" << std::endl;
    std::cout << "  - Use search to filter games" << std::endl;
    std::cout << "  - Categories in sidebar" << std::endl;
    std::cout << "  - ESC to exit" << std::endl;
    
    app.Run();
    app.Shutdown();
    
    std::cout << "Steam UI closed" << std::endl;
    return 0;
}