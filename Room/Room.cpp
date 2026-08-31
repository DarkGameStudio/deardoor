// SteamGameRoom_SingleFile.cpp
// Complete Steam Game Room Application - DarkDearDoor Style
// Combines all previous files into one compilation unit.

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <filesystem>

// Steamworks
#include <steam/steam_api.h>

// GLFW
#include <GLFW/glfw3.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// For Windows console UTF-8 (optional)
#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// Theme definition
// ============================================================================
namespace Theme {
    struct Color {
        float r, g, b, a;
    };

    // Dark background palette
    constexpr Color Background     = {0.08f, 0.08f, 0.10f, 1.0f};
    constexpr Color Surface        = {0.12f, 0.12f, 0.15f, 1.0f};
    constexpr Color SurfaceLight   = {0.16f, 0.16f, 0.20f, 1.0f};
    constexpr Color Border         = {0.25f, 0.25f, 0.30f, 1.0f};

    // Accent colors
    constexpr Color Accent         = {0.35f, 0.55f, 0.85f, 1.0f};  // Steel blue
    constexpr Color AccentHover    = {0.45f, 0.65f, 0.95f, 1.0f};
    constexpr Color Success        = {0.30f, 0.75f, 0.40f, 1.0f};
    constexpr Color Warning        = {0.90f, 0.70f, 0.30f, 1.0f};
    constexpr Color Error          = {0.85f, 0.30f, 0.30f, 1.0f};

    // Text colors
    constexpr Color TextPrimary    = {0.90f, 0.90f, 0.92f, 1.0f};
    constexpr Color TextSecondary  = {0.60f, 0.60f, 0.65f, 1.0f};
    constexpr Color TextDisabled   = {0.35f, 0.35f, 0.40f, 1.0f};

    // UI Dimensions
    constexpr float WindowWidth    = 1280.0f;
    constexpr float WindowHeight   = 720.0f;
    constexpr float CardWidth      = 200.0f;
    constexpr float CardHeight     = 280.0f;
    constexpr float CardSpacing    = 20.0f;
    constexpr float SearchBarHeight= 40.0f;
    constexpr float HeaderHeight   = 60.0f;
    constexpr float CornerRadius   = 8.0f;
}

// ============================================================================
// Steam Manager
// ============================================================================
struct GameInfo {
    AppId_t appId;
    std::string name;
    std::string installDir;
    bool isInstalled;
    bool isRunning;
    int iconHandle;
    std::string headerImagePath;
    std::vector<std::string> tags;
};

class SteamManager {
public:
    static SteamManager& GetInstance() {
        static SteamManager instance;
        return instance;
    }

    bool Initialize() {
        if (m_initialized) return true;

        if (!SteamAPI_Init()) {
            std::cerr << "Failed to initialize Steam API. Make sure Steam is running." << std::endl;
            return false;
        }

        // Register Steam callbacks
        m_GameOverlayActivated.Register(this, &SteamManager::OnGameOverlayActivated);
        m_AppInstalled.Register(this, &SteamManager::OnAppInstalled);
        m_AppUninstalled.Register(this, &SteamManager::OnAppUninstalled);

        m_initialized = true;
        UpdateGameList();

        std::cout << "Steam API initialized successfully. User: " << GetPersonaName() << std::endl;
        return true;
    }

    void Shutdown() {
        if (m_initialized) {
            SteamAPI_Shutdown();
            m_initialized = false;
        }
    }

    bool IsInitialized() const { return m_initialized; }

    std::vector<GameInfo> GetOwnedGames() {
        std::lock_guard<std::mutex> lock(m_gamesMutex);
        return m_games;
    }

    bool LaunchGame(AppId_t appId) {
        if (!m_initialized) return false;
        if (SteamApps()->LaunchApp(appId)) {
            if (m_gameLaunchedCallback) m_gameLaunchedCallback(appId);
            return true;
        }
        return false;
    }

    bool IsGameRunning(AppId_t appId) {
        if (!m_initialized) return false;
        return SteamApps()->BIsAppRunning(appId);
    }

    void RefreshGameList() { UpdateGameList(); }

    void SetGameListUpdatedCallback(std::function<void()> callback) {
        m_gameListUpdatedCallback = callback;
    }

    void SetGameLaunchedCallback(std::function<void(AppId_t)> callback) {
        m_gameLaunchedCallback = callback;
    }

    std::string GetPersonaName() {
        if (!m_initialized) return "";
        return SteamFriends()->GetPersonaName();
    }

    uint64_t GetSteamID() {
        if (!m_initialized) return 0;
        return SteamUser()->GetSteamID().ConvertToUint64();
    }

    // Steam callbacks
    STEAM_CALLBACK(SteamManager, OnGameOverlayActivated, GameOverlayActivated_t);
    STEAM_CALLBACK(SteamManager, OnAppInstalled, AppInstalled_t);
    STEAM_CALLBACK(SteamManager, OnAppUninstalled, AppUninstalled_t);

private:
    SteamManager() = default;
    ~SteamManager() { Shutdown(); }
    SteamManager(const SteamManager&) = delete;
    SteamManager& operator=(const SteamManager&) = delete;

    void UpdateGameList() {
        if (!m_initialized) return;

        std::vector<GameInfo> newGames;
        int gameCount = SteamApps()->GetAppCount();

        for (int i = 0; i < gameCount; i++) {
            AppId_t appId = SteamApps()->GetAppID(i);

            // Skip DLC and non-subscribed apps
            if (SteamApps()->BIsDlcInstalled(appId)) continue;
            if (!SteamApps()->BIsSubscribedApp(appId)) continue;

            GameInfo game;
            game.appId = appId;

            char name[256];
            int nameLength = SteamApps()->GetAppName(appId, name, sizeof(name));
            if (nameLength <= 0) continue;

            game.name = std::string(name);

            char installDir[1024];
            if (SteamApps()->GetAppInstallDir(appId, installDir, sizeof(installDir)) > 0) {
                game.installDir = std::string(installDir);
                game.isInstalled = true;
            } else {
                game.isInstalled = false;
            }

            game.isRunning = SteamApps()->BIsAppRunning(appId);
            game.iconHandle = SteamApps()->GetAppIcon(appId);

            // Try to locate header image
            std::filesystem::path steamPath = game.installDir;
            if (!steamPath.empty()) {
                std::filesystem::path headerPath = steamPath / "library" / "header.jpg";
                if (std::filesystem::exists(headerPath)) {
                    game.headerImagePath = headerPath.string();
                }
            }

            // Filter out tools/servers/demos
            std::string lowerName = game.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            bool isTool =
                lowerName.find("server") != std::string::npos ||
                lowerName.find("sdk") != std::string::npos ||
                lowerName.find("tool") != std::string::npos ||
                lowerName.find("dedicated") != std::string::npos;

            if (!isTool && game.isInstalled) {
                newGames.push_back(game);
            }
        }

        // Sort alphabetically
        std::sort(newGames.begin(), newGames.end(),
                  [](const GameInfo& a, const GameInfo& b) { return a.name < b.name; });

        {
            std::lock_guard<std::mutex> lock(m_gamesMutex);
            m_games = newGames;
        }

        if (m_gameListUpdatedCallback) m_gameListUpdatedCallback();
    }

    bool m_initialized = false;
    std::vector<GameInfo> m_games;
    std::mutex m_gamesMutex;
    std::function<void()> m_gameListUpdatedCallback;
    std::function<void(AppId_t)> m_gameLaunchedCallback;
};

// Steam callback implementations
void SteamManager::OnGameOverlayActivated(GameOverlayActivated_t* pCallback) {
    if (pCallback->m_bActive) {
        std::cout << "Steam overlay activated" << std::endl;
    }
}
void SteamManager::OnAppInstalled(AppInstalled_t* pCallback) {
    std::cout << "App installed: " << pCallback->m_nAppID << std::endl;
    UpdateGameList();
}
void SteamManager::OnAppUninstalled(AppUninstalled_t* pCallback) {
    std::cout << "App uninstalled: " << pCallback->m_nAppID << std::endl;
    UpdateGameList();
}

// ============================================================================
// Game Card
// ============================================================================
class GameCard {
public:
    GameCard(const GameInfo& gameInfo, float x, float y)
        : m_gameInfo(gameInfo), m_x(x), m_y(y) {}

    void Draw() {
        ImGui::PushID(m_gameInfo.appId);

        ImGui::SetCursorPos(ImVec2(m_x, m_y));
        ImGui::InvisibleButton("gamecard", ImVec2(m_width, m_height));

        m_hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) {
            m_selected = true;
            if (m_launchCallback) m_launchCallback(m_gameInfo.appId);
        }
        if (ImGui::IsItemClicked(1)) {  // Right click
            m_selected = false;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetItemRectMin();
        ImVec2 size = ImGui::GetItemRectSize();

        // Background
        ImU32 bgColor = m_hovered ?
            ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::SurfaceLight.r, Theme::SurfaceLight.g, Theme::SurfaceLight.b, Theme::SurfaceLight.a)) :
            ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Surface.r, Theme::Surface.g, Theme::Surface.b, Theme::Surface.a));
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, Theme::CornerRadius);

        // Border
        ImU32 borderColor = m_selected ?
            ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, Theme::Accent.a)) :
            ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Border.r, Theme::Border.g, Theme::Border.b, Theme::Border.a));
        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), borderColor, Theme::CornerRadius, 0, 2.0f);

        // Header image area (placeholder)
        ImVec2 imagePos = ImVec2(pos.x + 10, pos.y + 10);
        ImVec2 imageSize = ImVec2(size.x - 20, size.y * 0.5f);
        drawList->AddRectFilled(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
                                ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::SurfaceLight.r, Theme::SurfaceLight.g, Theme::SurfaceLight.b, Theme::SurfaceLight.a)),
                                Theme::CornerRadius * 0.5f);

        // Draw initial letter if no image
        std::string initial = m_gameInfo.name.substr(0, 1);
        ImVec2 textPos = ImVec2(imagePos.x + imageSize.x * 0.5f - 10, imagePos.y + imageSize.y * 0.5f - 15);
        drawList->AddText(ImGui::GetFont(), 32.0f, textPos,
                          ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::TextPrimary.r, Theme::TextPrimary.g, Theme::TextPrimary.b, Theme::TextPrimary.a)),
                          initial.c_str());

        // Game name (truncated if needed)
        textPos = ImVec2(pos.x + 10, pos.y + size.y * 0.5f + 15);
        std::string displayName = m_gameInfo.name;
        while (ImGui::CalcTextSize((displayName + "...").c_str()).x > size.x - 20 && displayName.length() > 0)
            displayName.pop_back();
        if (displayName.length() < m_gameInfo.name.length()) displayName += "...";
        drawList->AddText(textPos,
                          ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::TextPrimary.r, Theme::TextPrimary.g, Theme::TextPrimary.b, Theme::TextPrimary.a)),
                          displayName.c_str());

        // Status indicator
        ImVec2 statusPos = ImVec2(pos.x + 10, pos.y + size.y - 25);
        if (m_gameInfo.isRunning) {
            drawList->AddCircleFilled(ImVec2(statusPos.x + 5, statusPos.y + 5), 4.0f,
                                      ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Success.r, Theme::Success.g, Theme::Success.b, Theme::Success.a)));
            drawList->AddText(ImVec2(statusPos.x + 15, statusPos.y - 2),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Success.r, Theme::Success.g, Theme::Success.b, Theme::Success.a)),
                              "Running");
        } else if (m_gameInfo.isInstalled) {
            drawList->AddCircleFilled(ImVec2(statusPos.x + 5, statusPos.y + 5), 4.0f,
                                      ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, Theme::Accent.a)));
            drawList->AddText(ImVec2(statusPos.x + 15, statusPos.y - 2),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(Theme::TextSecondary.r, Theme::TextSecondary.g, Theme::TextSecondary.b, Theme::TextSecondary.a)),
                              "Ready");
        }

        ImGui::PopID();
    }

    void SetPosition(float x, float y) { m_x = x; m_y = y; }
    void SetSelected(bool selected) { m_selected = selected; }
    bool IsSelected() const { return m_selected; }
    AppId_t GetAppId() const { return m_gameInfo.appId; }
    std::string GetGameName() const { return m_gameInfo.name; }

    void SetLaunchCallback(std::function<void(AppId_t)> callback) { m_launchCallback = callback; }

private:
    GameInfo m_gameInfo;
    float m_x, m_y;
    float m_width = Theme::CardWidth;
    float m_height = Theme::CardHeight;
    bool m_hovered = false;
    bool m_selected = false;
    std::function<void(AppId_t)> m_launchCallback;
};

// ============================================================================
// Game Room Window (Main UI)
// ============================================================================
class GameRoomWindow {
public:
    GameRoomWindow() = default;
    ~GameRoomWindow() { Shutdown(); }

    bool Initialize() {
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(static_cast<int>(Theme::WindowWidth),
                                    static_cast<int>(Theme::WindowHeight),
                                    "Steam Game Room - DarkDearDoor Style",
                                    nullptr, nullptr);
        if (!m_window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);

        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        SetupImGuiStyle();

        // Initialize Steam
        if (!SteamManager::GetInstance().Initialize()) {
            std::cerr << "Failed to initialize Steam" << std::endl;
            return false;
        }

        // Set callbacks
        SteamManager::GetInstance().SetGameListUpdatedCallback([this]() { UpdateGameCards(); });
        SteamManager::GetInstance().SetGameLaunchedCallback([this](AppId_t appId) {
            std::cout << "Game launched: " << appId << std::endl;
            UpdateGameCards();
        });

        UpdateGameCards();
        return true;
    }

    void Run() {
        while (!glfwWindowShouldClose(m_window)) {
            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // Fullscreen window
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("GameRoom", nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoBringToFrontOnFocus);

            DrawHeader();
            DrawSearchBar();
            DrawGameGrid();
            DrawSidebar();
            DrawFooter();

            ImGui::End();

            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(Theme::Background.r, Theme::Background.g, Theme::Background.b, Theme::Background.a);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window);
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
    void SetupImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();

        style.Colors[ImGuiCol_WindowBg] = ImVec4(Theme::Background.r, Theme::Background.g, Theme::Background.b, Theme::Background.a);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(Theme::Surface.r, Theme::Surface.g, Theme::Surface.b, Theme::Surface.a);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(Theme::Surface.r, Theme::Surface.g, Theme::Surface.b, Theme::Surface.a);
        style.Colors[ImGuiCol_Border] = ImVec4(Theme::Border.r, Theme::Border.g, Theme::Border.b, Theme::Border.a);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(Theme::SurfaceLight.r, Theme::SurfaceLight.g, Theme::SurfaceLight.b, Theme::SurfaceLight.a);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(Theme::SurfaceLight.r + 0.05f, Theme::SurfaceLight.g + 0.05f, Theme::SurfaceLight.b + 0.05f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, Theme::Accent.a);
        style.Colors[ImGuiCol_Button] = ImVec4(Theme::SurfaceLight.r, Theme::SurfaceLight.g, Theme::SurfaceLight.b, Theme::SurfaceLight.a);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(Theme::AccentHover.r, Theme::AccentHover.g, Theme::AccentHover.b, Theme::AccentHover.a);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, Theme::Accent.a);
        style.Colors[ImGuiCol_Text] = ImVec4(Theme::TextPrimary.r, Theme::TextPrimary.g, Theme::TextPrimary.b, Theme::TextPrimary.a);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(Theme::TextDisabled.r, Theme::TextDisabled.g, Theme::TextDisabled.b, Theme::TextDisabled.a);
        style.Colors[ImGuiCol_Header] = ImVec4(Theme::SurfaceLight.r, Theme::SurfaceLight.g, Theme::SurfaceLight.b, Theme::SurfaceLight.a);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(Theme::AccentHover.r, Theme::AccentHover.g, Theme::AccentHover.b, Theme::AccentHover.a);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, Theme::Accent.a);

        style.WindowRounding = Theme::CornerRadius;
        style.FrameRounding = Theme::CornerRadius * 0.5f;
        style.ChildRounding = Theme::CornerRadius;
        style.PopupRounding = Theme::CornerRadius;
        style.ScrollbarRounding = Theme::CornerRadius;
        style.GrabRounding = Theme::CornerRadius * 0.5f;
        style.TabRounding = Theme::CornerRadius;

        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(8, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12;
    }

    void DrawHeader() {
        ImGui::SetCursorPosY(10);
        ImGui::SetCursorPosX(20);
        ImGui::TextColored(ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, 1.0f), "STEAM GAME ROOM");

        ImGui::SameLine();
        ImGui::SetCursorPosX(Theme::WindowWidth - 250);
        std::string userInfo = "User: " + SteamManager::GetInstance().GetPersonaName();
        ImGui::TextColored(ImVec4(Theme::TextSecondary.r, Theme::TextSecondary.g, Theme::TextSecondary.b, Theme::TextSecondary.a), userInfo.c_str());
        ImGui::Separator();
    }

    void DrawSearchBar() {
        ImGui::SetCursorPosY(Theme::HeaderHeight + 10);
        ImGui::SetCursorPosX(20);

        ImGui::PushItemWidth(300);
        ImGui::InputTextWithHint("##search", "Search games...", m_searchQuery.data(), m_searchQuery.capacity() + 1);
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Checkbox("Installed Only", &m_showInstalledOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Running Only", &m_showRunningOnly);

        ImGui::SameLine();
        ImGui::SetCursorPosX(Theme::WindowWidth - 150);
        if (ImGui::Button("Refresh", ImVec2(120, 30))) {
            UpdateGameCards();
        }
        ImGui::Separator();
    }

    void DrawGameGrid() {
        ImGui::SetCursorPosY(Theme::HeaderHeight + Theme::SearchBarHeight + 20);
        ImGui::SetCursorPosX(20);

        float availableWidth = Theme::WindowWidth - 220; // leave space for sidebar
        int columns = static_cast<int>(availableWidth / (Theme::CardWidth + Theme::CardSpacing));
        if (columns < 1) columns = 1;

        ImGui::BeginChild("GameGrid", ImVec2(availableWidth, Theme::WindowHeight - Theme::HeaderHeight - Theme::SearchBarHeight - 60));

        int index = 0;
        for (auto& card : m_gameCards) {
            int row = index / columns;
            int col = index % columns;
            float x = col * (Theme::CardWidth + Theme::CardSpacing);
            float y = row * (Theme::CardHeight + Theme::CardSpacing);
            card->SetPosition(x, y);
            card->Draw();
            index++;
        }

        ImGui::EndChild();
    }

    void DrawSidebar() {
        ImGui::SetCursorPosX(Theme::WindowWidth - 180);
        ImGui::SetCursorPosY(Theme::HeaderHeight + Theme::SearchBarHeight + 20);

        ImGui::BeginChild("Sidebar", ImVec2(160, Theme::WindowHeight - Theme::HeaderHeight - Theme::SearchBarHeight - 60), true);

        ImGui::TextColored(ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, 1.0f), "Statistics");
        ImGui::Separator();

        int totalGames = m_gameCards.size();
        int runningGames = 0;
        for (const auto& card : m_gameCards) {
            if (SteamManager::GetInstance().IsGameRunning(card->GetAppId())) runningGames++;
        }

        ImGui::Text("Total Games: %d", totalGames);
        ImGui::Text("Running: %d", runningGames);
        ImGui::Text("Ready: %d", totalGames - runningGames);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(Theme::Accent.r, Theme::Accent.g, Theme::Accent.b, 1.0f), "Quick Actions");
        ImGui::Separator();

        if (ImGui::Button("Launch Random", ImVec2(140, 30))) {
            if (!m_gameCards.empty()) {
                int randomIndex = rand() % m_gameCards.size();
                SteamManager::GetInstance().LaunchGame(m_gameCards[randomIndex]->GetAppId());
            }
        }

        if (ImGui::Button("View Library", ImVec2(140, 30))) {
            system("start steam://open/library");
        }

        ImGui::EndChild();
    }

    void DrawFooter() {
        ImGui::SetCursorPosY(Theme::WindowHeight - 30);
        ImGui::Separator();
        ImGui::SetCursorPosX(20);
        ImGui::TextColored(ImVec4(Theme::TextDisabled.r, Theme::TextDisabled.g, Theme::TextDisabled.b, Theme::TextDisabled.a),
                           "Steam Game Room v1.0 - DarkDearDoor Edition");
    }

    void ProcessInput() {
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_F5))) UpdateGameCards();
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) glfwSetWindowShouldClose(m_window, true);
    }

    void UpdateGameCards() {
        m_gameCards.clear();

        auto games = SteamManager::GetInstance().GetOwnedGames();
        for (const auto& game : games) {
            if (m_showInstalledOnly && !game.isInstalled) continue;
            if (m_showRunningOnly && !game.isRunning) continue;

            if (!m_searchQuery.empty()) {
                std::string lowerName = game.name;
                std::string lowerQuery = m_searchQuery;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
                if (lowerName.find(lowerQuery) == std::string::npos) continue;
            }

            auto card = std::make_unique<GameCard>(game, 0, 0);
            card->SetLaunchCallback([](AppId_t appId) {
                SteamManager::GetInstance().LaunchGame(appId);
            });
            m_gameCards.push_back(std::move(card));
        }
    }

    GLFWwindow* m_window = nullptr;
    std::vector<std::unique_ptr<GameCard>> m_gameCards;
    std::string m_searchQuery;
    bool m_showInstalledOnly = true;
    bool m_showRunningOnly = false;
};

// ============================================================================
// Main entry point
// ============================================================================
int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "=== Steam Game Room ===" << std::endl;
    std::cout << "Initializing DarkDearDoor style game launcher..." << std::endl;

    GameRoomWindow window;
    if (!window.Initialize()) {
        std::cerr << "Failed to initialize application" << std::endl;
        std::cerr << "Make sure Steam is running and you have the required libraries" << std::endl;
        system("pause");
        return -1;
    }

    std::cout << "Application started successfully!" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  - Click game card to launch" << std::endl;
    std::cout << "  - Right-click to deselect" << std::endl;
    std::cout << "  - F5 to refresh game list" << std::endl;
    std::cout << "  - ESC to exit" << std::endl;

    window.Run();
    std::cout << "Shutting down..." << std::endl;
    return 0;
}