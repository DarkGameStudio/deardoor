// deardoor_main_theme.cpp
// DearDoor - Normal Purpose UI Main Theme
// Clean, Modern, Professional Steam Game Launcher Interface

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>

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

// STB Image for texture loading
#include "stb_image.h"

// ============================================================================
// DearDoor Theme Configuration
// ============================================================================
namespace DearDoorTheme {
    // Main color palette - Clean Professional Dark Theme
    namespace Colors {
        // Primary colors
        constexpr ImVec4 Primary        = ImVec4(0.20f, 0.45f, 0.85f, 1.0f);  // Professional Blue
        constexpr ImVec4 PrimaryDark    = ImVec4(0.15f, 0.35f, 0.65f, 1.0f);
        constexpr ImVec4 PrimaryLight   = ImVec4(0.30f, 0.55f, 0.95f, 1.0f);
        
        // Background colors
        constexpr ImVec4 Background     = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);  // Dark background
        constexpr ImVec4 Surface        = ImVec4(0.15f, 0.16f, 0.18f, 1.0f);  // Card surface
        constexpr ImVec4 SurfaceLight   = ImVec4(0.20f, 0.21f, 0.23f, 1.0f);  // Hover surface
        constexpr ImVec4 SurfaceDark    = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);  // Deep surface
        
        // Text colors
        constexpr ImVec4 TextPrimary    = ImVec4(0.92f, 0.93f, 0.95f, 1.0f);  // Main text
        constexpr ImVec4 TextSecondary  = ImVec4(0.65f, 0.67f, 0.70f, 1.0f);  // Secondary text
        constexpr ImVec4 TextDisabled   = ImVec4(0.40f, 0.42f, 0.45f, 1.0f);  // Disabled text
        
        // Accent colors
        constexpr ImVec4 Success        = ImVec4(0.25f, 0.70f, 0.35f, 1.0f);  // Green
        constexpr ImVec4 Warning        = ImVec4(0.90f, 0.70f, 0.25f, 1.0f);  // Yellow
        constexpr ImVec4 Error          = ImVec4(0.85f, 0.30f, 0.25f, 1.0f);  // Red
        constexpr ImVec4 Info           = ImVec4(0.25f, 0.60f, 0.90f, 1.0f);  // Blue
        
        // Border colors
        constexpr ImVec4 Border         = ImVec4(0.25f, 0.26f, 0.28f, 1.0f);
        constexpr ImVec4 BorderLight    = ImVec4(0.35f, 0.36f, 0.38f, 1.0f);
        
        // Button colors
        constexpr ImVec4 ButtonPrimary  = ImVec4(0.20f, 0.45f, 0.85f, 1.0f);
        constexpr ImVec4 ButtonHover    = ImVec4(0.25f, 0.50f, 0.90f, 1.0f);
        constexpr ImVec4 ButtonActive   = ImVec4(0.15f, 0.40f, 0.80f, 1.0f);
        constexpr ImVec4 ButtonDisabled = ImVec4(0.30f, 0.31f, 0.33f, 0.5f);
    }
    
    // Dimensions and spacing
    namespace Dimensions {
        constexpr float WindowWidth     = 1440.0f;
        constexpr float WindowHeight    = 810.0f;
        constexpr float SidebarWidth    = 240.0f;
        constexpr float TopBarHeight    = 60.0f;
        constexpr float BottomBarHeight = 40.0f;
        
        constexpr float CardWidth       = 200.0f;
        constexpr float CardHeight      = 260.0f;
        constexpr float CardSpacing     = 20.0f;
        constexpr float CardPadding     = 12.0f;
        
        constexpr float ButtonHeight    = 36.0f;
        constexpr float InputHeight     = 32.0f;
        constexpr float IconSize        = 24.0f;
        constexpr float CornerRadius    = 6.0f;
        constexpr float BorderWidth     = 1.5f;
    }
    
    // Font configuration
    namespace Fonts {
        constexpr float TitleSize       = 28.0f;
        constexpr float HeaderSize      = 22.0f;
        constexpr float SubheaderSize   = 18.0f;
        constexpr float BodySize        = 14.0f;
        constexpr float SmallSize       = 12.0f;
        constexpr float ButtonSize      = 14.0f;
    }
    
    // Animation timing
    namespace Animation {
        constexpr float HoverTransition = 0.15f;  // Seconds
        constexpr float PressTransition = 0.10f;
        constexpr float FadeTransition  = 0.25f;
        constexpr float SlideTransition = 0.30f;
    }
}

// ============================================================================
// Game Information Structure
// ============================================================================
struct GameInfo {
    AppId_t appId;
    std::string name;
    std::string description;
    std::string developer;
    std::string publisher;
    std::string installDir;
    std::string headerImagePath;
    std::string iconPath;
    std::string logoPath;
    
    bool isInstalled;
    bool isRunning;
    bool isFavorite;
    bool isRecent;
    
    int playtimeMinutes;
    int lastPlayedTimestamp;
    int totalAchievements;
    int unlockedAchievements;
    
    float rating;
    std::vector<std::string> tags;
    std::vector<std::string> genres;
    
    // UI state
    bool isSelected;
    bool isHovered;
    float hoverAnimation;
    float selectAnimation;
};

// ============================================================================
// UI Theme Manager
// ============================================================================
class UIThemeManager {
private:
    struct ThemeState {
        ImVec4 primaryColor;
        ImVec4 backgroundColor;
        ImVec4 surfaceColor;
        ImVec4 textColor;
        float cornerRadius;
        float borderWidth;
        float spacing;
        bool darkMode;
    };
    
    ThemeState m_currentTheme;
    std::map<std::string, ImFont*> m_fonts;
    std::map<std::string, ImTextureID> m_textures;
    
    // Theme presets
    std::map<std::string, ThemeState> m_themePresets;
    
public:
    UIThemeManager() {
        initializeDefaultTheme();
        initializeThemePresets();
    }
    
    void initializeDefaultTheme() {
        m_currentTheme.primaryColor = DearDoorTheme::Colors::Primary;
        m_currentTheme.backgroundColor = DearDoorTheme::Colors::Background;
        m_currentTheme.surfaceColor = DearDoorTheme::Colors::Surface;
        m_currentTheme.textColor = DearDoorTheme::Colors::TextPrimary;
        m_currentTheme.cornerRadius = DearDoorTheme::Dimensions::CornerRadius;
        m_currentTheme.borderWidth = DearDoorTheme::Dimensions::BorderWidth;
        m_currentTheme.spacing = DearDoorTheme::Dimensions::CardSpacing;
        m_currentTheme.darkMode = true;
    }
    
    void initializeThemePresets() {
        // Dark Theme (Default)
        ThemeState darkTheme = m_currentTheme;
        m_themePresets["Dark"] = darkTheme;
        
        // Light Theme
        ThemeState lightTheme;
        lightTheme.primaryColor = ImVec4(0.20f, 0.45f, 0.85f, 1.0f);
        lightTheme.backgroundColor = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
        lightTheme.surfaceColor = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
        lightTheme.textColor = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
        lightTheme.cornerRadius = 6.0f;
        lightTheme.borderWidth = 1.5f;
        lightTheme.spacing = 20.0f;
        lightTheme.darkMode = false;
        m_themePresets["Light"] = lightTheme;
        
        // OLED Theme
        ThemeState oledTheme;
        oledTheme.primaryColor = ImVec4(0.30f, 0.55f, 0.95f, 1.0f);
        oledTheme.backgroundColor = ImVec4(0.02f, 0.02f, 0.03f, 1.0f);
        oledTheme.surfaceColor = ImVec4(0.05f, 0.05f, 0.06f, 1.0f);
        oledTheme.textColor = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
        oledTheme.cornerRadius = 8.0f;
        oledTheme.borderWidth = 1.0f;
        oledTheme.spacing = 16.0f;
        oledTheme.darkMode = true;
        m_themePresets["OLED"] = oledTheme;
    }
    
    void applyTheme(const std::string& themeName) {
        auto it = m_themePresets.find(themeName);
        if (it != m_themePresets.end()) {
            m_currentTheme = it->second;
            applyToImGui();
        }
    }
    
    void applyToImGui() {
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Apply colors
        style.Colors[ImGuiCol_WindowBg] = m_currentTheme.backgroundColor;
        style.Colors[ImGuiCol_ChildBg] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_PopupBg] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_Border] = DearDoorTheme::Colors::Border;
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.23f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.28f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_TitleBg] = m_currentTheme.backgroundColor;
        style.Colors[ImGuiCol_TitleBgActive] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_TitleBgCollapsed] = m_currentTheme.backgroundColor;
        style.Colors[ImGuiCol_MenuBarBg] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.33f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.41f, 0.43f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_CheckMark] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_SliderGrab] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_SliderGrabActive] = DearDoorTheme::Colors::PrimaryLight;
        style.Colors[ImGuiCol_Button] = DearDoorTheme::Colors::ButtonPrimary;
        style.Colors[ImGuiCol_ButtonHovered] = DearDoorTheme::Colors::ButtonHover;
        style.Colors[ImGuiCol_ButtonActive] = DearDoorTheme::Colors::ButtonActive;
        style.Colors[ImGuiCol_Header] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_HeaderHovered] = DearDoorTheme::Colors::SurfaceLight;
        style.Colors[ImGuiCol_HeaderActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_Separator] = DearDoorTheme::Colors::Border;
        style.Colors[ImGuiCol_SeparatorHovered] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_SeparatorActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_ResizeGrip] = DearDoorTheme::Colors::Border;
        style.Colors[ImGuiCol_ResizeGripHovered] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_ResizeGripActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_Tab] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_TabHovered] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_TabActive] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_TabUnfocused] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_TabUnfocusedActive] = m_currentTheme.surfaceColor;
        style.Colors[ImGuiCol_PlotLines] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_PlotLinesHovered] = DearDoorTheme::Colors::PrimaryLight;
        style.Colors[ImGuiCol_PlotHistogram] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_PlotHistogramHovered] = DearDoorTheme::Colors::PrimaryLight;
        style.Colors[ImGuiCol_TextSelectedBg] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_DragDropTarget] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_NavHighlight] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_NavWindowingHighlight] = m_currentTheme.primaryColor;
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        
        // Apply style
        style.WindowRounding = m_currentTheme.cornerRadius;
        style.ChildRounding = m_currentTheme.cornerRadius;
        style.FrameRounding = m_currentTheme.cornerRadius * 0.5f;
        style.PopupRounding = m_currentTheme.cornerRadius;
        style.ScrollbarRounding = m_currentTheme.cornerRadius;
        style.GrabRounding = m_currentTheme.cornerRadius * 0.5f;
        style.TabRounding = m_currentTheme.cornerRadius;
        
        style.WindowPadding = ImVec2(16, 16);
        style.FramePadding = ImVec2(12, 6);
        style.ItemSpacing = ImVec2(8, 8);
        style.ItemInnerSpacing = ImVec2(8, 6);
        style.ScrollbarSize = 12;
        style.GrabMinSize = 10;
        style.WindowBorderSize = m_currentTheme.borderWidth;
        style.ChildBorderSize = m_currentTheme.borderWidth;
        style.PopupBorderSize = m_currentTheme.borderWidth;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 1;
        
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    }
    
    void loadFont(const std::string& name, const std::string& path, float size) {
        ImGuiIO& io = ImGui::GetIO();
        ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), size);
        if (font) {
            m_fonts[name] = font;
        }
    }
    
    void loadTexture(const std::string& name, const std::string& path) {
        int width, height, channels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        
        if (data) {
            GLuint textureID;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                        GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            m_textures[name] = (ImTextureID)(intptr_t)textureID;
            stbi_image_free(data);
        }
    }
    
    ImFont* getFont(const std::string& name) {
        auto it = m_fonts.find(name);
        return it != m_fonts.end() ? it->second : nullptr;
    }
    
    ImTextureID getTexture(const std::string& name) {
        auto it = m_textures.find(name);
        return it != m_textures.end() ? it->second : nullptr;
    }
    
    ThemeState getCurrentTheme() const { return m_currentTheme; }
};

// ============================================================================
// UI Components
// ============================================================================
class UIComponents {
private:
    UIThemeManager* m_themeManager;
    
public:
    UIComponents(UIThemeManager* themeManager) 
        : m_themeManager(themeManager) {}
    
    // Custom button component
    bool CustomButton(const char* label, const ImVec2& size, 
                     bool enabled = true, bool primary = false) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 buttonSize = size;
        
        bool hovered = ImGui::IsMouseHoveringRect(pos, 
                                                  ImVec2(pos.x + size.x, pos.y + size.y));
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        
        // Button background
        ImVec4 bgColor = primary ? DearDoorTheme::Colors::ButtonPrimary : 
                                  DearDoorTheme::Colors::Surface;
        
        if (!enabled) {
            bgColor = DearDoorTheme::Colors::ButtonDisabled;
        } else if (hovered) {
            bgColor = primary ? DearDoorTheme::Colors::ButtonHover : 
                               DearDoorTheme::Colors::SurfaceLight;
        }
        
        ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                               bgColorU32, style.FrameRounding);
        
        // Button border
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            primary && enabled ? DearDoorTheme::Colors::PrimaryLight : 
                                DearDoorTheme::Colors::Border
        );
        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                         borderColor, style.FrameRounding, 0, 1.0f);
        
        // Button text
        ImVec2 textSize = ImGui::CalcTextSize(label);
        ImVec2 textPos = ImVec2(pos.x + (size.x - textSize.x) * 0.5f,
                               pos.y + (size.y - textSize.y) * 0.5f);
        
        ImU32 textColor = ImGui::ColorConvertFloat4ToU32(
            enabled ? DearDoorTheme::Colors::TextPrimary : 
                     DearDoorTheme::Colors::TextDisabled
        );
        drawList->AddText(textPos, textColor, label);
        
        // Advance cursor
        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + style.ItemSpacing.y));
        
        return clicked && enabled;
    }
    
    // Custom card component
    bool GameCard(const GameInfo& game, const ImVec2& size) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 cardSize = size;
        
        bool hovered = ImGui::IsMouseHoveringRect(pos, 
                                                  ImVec2(pos.x + size.x, pos.y + size.y));
        bool clicked = hovered && ImGui::IsMouseClicked(0);
        
        // Card background
        ImVec4 bgColor = game.isSelected ? DearDoorTheme::Colors::SurfaceLight : 
                                          DearDoorTheme::Colors::Surface;
        if (hovered) {
            bgColor = DearDoorTheme::Colors::SurfaceLight;
        }
        
        ImU32 bgColorU32 = ImGui::ColorConvertFloat4ToU32(bgColor);
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                               bgColorU32, style.FrameRounding);
        
        // Card border
        ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(
            game.isSelected ? DearDoorTheme::Colors::Primary : 
                             DearDoorTheme::Colors::Border
        );
        float borderWidth = game.isSelected ? 2.0f : 1.0f;
        drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                         borderColor, style.FrameRounding, 0, borderWidth);
        
        // Game header image area
        ImVec2 imagePos = ImVec2(pos.x + 8, pos.y + 8);
        ImVec2 imageSize = ImVec2(size.x - 16, size.y * 0.5f);
        
        // Draw placeholder if no image
        ImU32 imageBgColor = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.20f, 0.22f, 0.25f, 1.0f)
        );
        drawList->AddRectFilled(imagePos, 
                               ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y),
                               imageBgColor, style.FrameRounding * 0.5f);
        
        // Game title
        ImVec2 titlePos = ImVec2(pos.x + 12, pos.y + size.y * 0.5f + 12);
        ImU32 titleColor = ImGui::ColorConvertFloat4ToU32(
            DearDoorTheme::Colors::TextPrimary
        );
        drawList->AddText(titlePos, titleColor, game.name.c_str());
        
        // Game status
        ImVec2 statusPos = ImVec2(pos.x + 12, pos.y + size.y - 30);
        ImVec4 statusColor = game.isRunning ? DearDoorTheme::Colors::Success : 
                            game.isInstalled ? DearDoorTheme::Colors::Info : 
                                             DearDoorTheme::Colors::TextDisabled;
        const char* statusText = game.isRunning ? "Running" : 
                                game.isInstalled ? "Installed" : "Not Installed";
        
        ImU32 statusColorU32 = ImGui::ColorConvertFloat4ToU32(statusColor);
        drawList->AddText(statusPos, statusColorU32, statusText);
        
        // Advance cursor
        ImGui::SetCursorScreenPos(ImVec2(pos.x + size.x + style.ItemSpacing.x, pos.y));
        
        return clicked;
    }
    
    // Custom search bar
    bool SearchBar(char* buffer, size_t bufferSize, const ImVec2& size) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        ImVec2 pos = ImGui::GetCursorScreenPos();
        
        // Draw search background
        ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
            DearDoorTheme::Colors::Surface
        );
        drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), 
                               bgColor, style.FrameRounding);
        
        // Draw search icon
        ImVec2 iconPos = ImVec2(pos.x + 10, pos.y + size.y * 0.5f - 6);
        ImU32 iconColor = ImGui::ColorConvertFloat4ToU32(
            DearDoorTheme::Colors::TextSecondary
        );
        drawList->AddCircle(iconPos, 6, iconColor, 12, 1.5f);
        drawList->AddLine(ImVec2(iconPos.x + 4, iconPos.y + 4),
                         ImVec2(iconPos.x + 8, iconPos.y + 8), iconColor, 1.5f);
        
        // Draw input text
        ImGui::SetCursorScreenPos(ImVec2(pos.x + 30, pos.y + 8));
        ImGui::PushItemWidth(size.x - 40);
        bool changed = ImGui::InputText("##search", buffer, bufferSize);
        ImGui::PopItemWidth();
        
        // Draw clear button if text exists
        if (buffer[0] != '\0') {
            ImVec2 clearPos = ImVec2(pos.x + size.x - 20, pos.y + size.y * 0.5f - 5);
            ImU32 clearColor = ImGui::ColorConvertFloat4ToU32(
                DearDoorTheme::Colors::TextSecondary
            );
            drawList->AddText(clearPos, clearColor, "×");
            
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
// DearDoor Main Window
// ============================================================================
class DearDoorMainWindow {
private:
    GLFWwindow* m_window;
    UIThemeManager m_themeManager;
    UIComponents m_uiComponents;
    
    // Application state
    bool m_isInitialized;
    bool m_showSidebar;
    bool m_showGrid;
    bool m_showList;
    bool m_showSettings;
    
    // Game data
    std::vector<GameInfo> m_games;
    std::vector<GameInfo> m_filteredGames;
    
    // UI state
    char m_searchBuffer[256];
    std::string m_selectedCategory;
    int m_currentViewMode;
    int m_sortMode;
    
    // Window dimensions
    int m_windowWidth;
    int m_windowHeight;
    
    // Animation state
    float m_sidebarAnimation;
    float m_contentAnimation;
    
public:
    DearDoorMainWindow()
        : m_window(nullptr)
        , m_uiComponents(&m_themeManager)
        , m_isInitialized(false)
        , m_showSidebar(true)
        , m_showGrid(true)
        , m_showList(false)
        , m_showSettings(false)
        , m_currentViewMode(0)
        , m_sortMode(0)
        , m_windowWidth(DearDoorTheme::Dimensions::WindowWidth)
        , m_windowHeight(DearDoorTheme::Dimensions::WindowHeight)
        , m_sidebarAnimation(1.0f)
        , m_contentAnimation(1.0f) {
        
        memset(m_searchBuffer, 0, sizeof(m_searchBuffer));
        m_selectedCategory = "All Games";
    }
    
    ~DearDoorMainWindow() {
        shutdown();
    }
    
    bool initialize() {
        std::cout << "========================================" << std::endl;
        std::cout << "DearDoor - Steam Game Launcher" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Initialize GLFW
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }
        
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        
        // Create window
        m_window = glfwCreateWindow(
            m_windowWidth, m_windowHeight,
            "DearDoor - Steam Game Launcher",
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
        setupCallbacks();
        
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
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Apply theme
        m_themeManager.applyTheme("Dark");
        
        // Initialize Steam
        if (!SteamManager::GetInstance().Initialize()) {
            std::cerr << "Failed to initialize Steam" << std::endl;
            return false;
        }
        
        // Load games
        loadGames();
        
        m_isInitialized = true;
        
        std::cout << "DearDoor initialized successfully" << std::endl;
        std::cout << "Loaded " << m_games.size() << " games" << std::endl;
        
        return true;
    }
    
    void run() {
        while (!glfwWindowShouldClose(m_window) && m_isInitialized) {
            glfwPollEvents();
            
            // Start ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            // Render UI
            renderUI();
            
            // Render ImGui
            ImGui::Render();
            
            // Clear screen
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(
                DearDoorTheme::Colors::Background.x,
                DearDoorTheme::Colors::Background.y,
                DearDoorTheme::Colors::Background.z,
                1.0f
            );
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(m_window);
            
            // Process Steam callbacks
            SteamAPI_RunCallbacks();
        }
    }
    
    void shutdown() {
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
    void setupCallbacks() {
        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            DearDoorMainWindow* app = static_cast<DearDoorMainWindow*>(
                glfwGetWindowUserPointer(window)
            );
            if (app) {
                app->m_windowWidth = width;
                app->m_windowHeight = height;
                glViewport(0, 0, width, height);
            }
        });
    }
    
    void loadGames() {
        m_games.clear();
        
        auto steamGames = SteamManager::GetInstance().GetOwnedGames();
        
        for (const auto& steamGame : steamGames) {
            GameInfo game;
            game.appId = steamGame.appId;
            game.name = steamGame.name;
            game.installDir = steamGame.installDir;
            game.isInstalled = steamGame.isInstalled;
            game.isRunning = steamGame.isRunning;
            game.headerImagePath = steamGame.headerImagePath;
            game.isFavorite = false;
            game.isRecent = false;
            game.playtimeMinutes = 0;
            game.rating = 0.0f;
            game.isSelected = false;
            game.isHovered = false;
            game.hoverAnimation = 0.0f;
            game.selectAnimation = 0.0f;
            
            m_games.push_back(game);
        }
        
        // Sort games alphabetically
        std::sort(m_games.begin(), m_games.end(),
                 [](const GameInfo& a, const GameInfo& b) {
                     return a.name < b.name;
                 });
        
        updateFilteredGames();
    }
    
    void updateFilteredGames() {
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
            if (m_selectedCategory == "Installed" && !game.isInstalled) {
                continue;
            }
            if (m_selectedCategory == "Running" && !game.isRunning) {
                continue;
            }
            if (m_selectedCategory == "Favorites" && !game.isFavorite) {
                continue;
            }
            
            m_filteredGames.push_back(game);
        }
    }
    
    void renderUI() {
        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(m_windowWidth, m_windowHeight));
        ImGui::Begin("DearDoor", nullptr,
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        // Render top bar
        renderTopBar();
        
        // Render sidebar
        if (m_showSidebar) {
            renderSidebar();
        }
        
        // Render main content
        renderMainContent();
        
        // Render status bar
        renderStatusBar();
        
        ImGui::End();
        
        // Render settings window
        if (m_showSettings) {
            renderSettingsWindow();
        }
    }
    
    void renderTopBar() {
        ImGui::SetCursorPos(ImVec2(16, 12));
        
        // App title
        ImGui::TextColored(DearDoorTheme::Colors::Primary, "DearDoor");
        ImGui::SameLine();
        
        // Search bar
        ImGui::SetCursorPosX(200);
        ImGui::PushItemWidth(300);
        if (m_uiComponents.SearchBar(m_searchBuffer, sizeof(m_searchBuffer), 
                                    ImVec2(300, DearDoorTheme::Dimensions::InputHeight))) {
            updateFilteredGames();
        }
        ImGui::PopItemWidth();
        
        // View mode buttons
        ImGui::SameLine();
        ImGui::SetCursorPosX(m_windowWidth - 300);
        
        if (m_uiComponents.CustomButton("Grid", ImVec2(60, 30), true, m_currentViewMode == 0)) {
            m_currentViewMode = 0;
        }
        ImGui::SameLine();
        if (m_uiComponents.CustomButton("List", ImVec2(60, 30), true, m_currentViewMode == 1)) {
            m_currentViewMode = 1;
        }
        ImGui::SameLine();
        if (m_uiComponents.CustomButton("Settings", ImVec2(80, 30))) {
            m_showSettings = true;
        }
        
        ImGui::Separator();
    }
    
    void renderSidebar() {
        ImGui::SetCursorPos(ImVec2(0, DearDoorTheme::Dimensions::TopBarHeight));
        
        ImGui::BeginChild("Sidebar", 
                         ImVec2(DearDoorTheme::Dimensions::SidebarWidth,
                               m_windowHeight - DearDoorTheme::Dimensions::TopBarHeight - 
                               DearDoorTheme::Dimensions::BottomBarHeight),
                         true);
        
        // Categories
        ImGui::TextColored(DearDoorTheme::Colors::TextSecondary, "LIBRARY");
        ImGui::Separator();
        
        std::vector<std::string> categories = {
            "All Games", "Installed", "Running", "Favorites", "Recent"
        };
        
        for (const auto& category : categories) {
            bool selected = (m_selectedCategory == category);
            
            if (ImGui::Selectable(category.c_str(), selected)) {
                m_selectedCategory = category;
                updateFilteredGames();
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Statistics
        ImGui::TextColored(DearDoorTheme::Colors::TextSecondary, "STATISTICS");
        ImGui::Separator();
        
        int totalGames = m_games.size();
        int installedGames = std::count_if(m_games.begin(), m_games.end(),
                                          [](const GameInfo& g) { return g.isInstalled; });
        int runningGames = std::count_if(m_games.begin(), m_games.end(),
                                        [](const GameInfo& g) { return g.isRunning; });
        
        ImGui::Text("Total: %d", totalGames);
        ImGui::Text("Installed: %d", installedGames);
        ImGui::Text("Running: %d", runningGames);
        
        ImGui::EndChild();
    }
    
    void renderMainContent() {
        float contentX = m_showSidebar ? DearDoorTheme::Dimensions::SidebarWidth : 0;
        float contentY = DearDoorTheme::Dimensions::TopBarHeight;
        float contentWidth = m_windowWidth - contentX;
        float contentHeight = m_windowHeight - contentY - DearDoorTheme::Dimensions::BottomBarHeight;
        
        ImGui::SetCursorPos(ImVec2(contentX, contentY));
        
        ImGui::BeginChild("Content", ImVec2(contentWidth, contentHeight), false);
        
        if (m_currentViewMode == 0) {
            renderGridView();
        } else {
            renderListView();
        }
        
        ImGui::EndChild();
    }
    
    void renderGridView() {
        // Calculate grid layout
        float availableWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(availableWidth / 
                       (DearDoorTheme::Dimensions::CardWidth + 
                        DearDoorTheme::Dimensions::CardSpacing)));
        
        int index = 0;
        for (auto& game : m_filteredGames) {
            int row = index / columns;
            int col = index % columns;
            
            float x = col * (DearDoorTheme::Dimensions::CardWidth + 
                            DearDoorTheme::Dimensions::CardSpacing) + 20;
            float y = row * (DearDoorTheme::Dimensions::CardHeight + 
                            DearDoorTheme::Dimensions::CardSpacing) + 20;
            
            ImGui::SetCursorPos(ImVec2(x, y));
            
            if (m_uiComponents.GameCard(game, 
                ImVec2(DearDoorTheme::Dimensions::CardWidth,
                      DearDoorTheme::Dimensions::CardHeight))) {
                // Handle game click
                launchGame(game.appId);
            }
            
            index++;
        }
    }
    
    void renderListView() {
        ImGui::BeginChild("GameList", ImVec2(0, 0), false);
        
        for (auto& game : m_filteredGames) {
            ImGui::PushID(game.appId);
            
            ImGui::SetCursorPosX(20);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
            
            // Game entry
            bool selected = game.isSelected;
            
            if (ImGui::Selectable("", selected, ImGuiSelectableFlags_AllowDoubleClick,
                                 ImVec2(ImGui::GetContentRegionAvail().x, 60))) {
                game.isSelected = !game.isSelected;
                
                if (ImGui::IsMouseDoubleClicked(0)) {
                    launchGame(game.appId);
                }
            }
            
            // Game info overlay
            ImVec2 pos = ImGui::GetItemRectMin();
            
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            
            // Game name
            ImVec2 namePos = ImVec2(pos.x + 10, pos.y + 10);
            ImU32 nameColor = ImGui::ColorConvertFloat4ToU32(
                DearDoorTheme::Colors::TextPrimary
            );
            drawList->AddText(namePos, nameColor, game.name.c_str());
            
            // Game status
            ImVec2 statusPos = ImVec2(pos.x + 10, pos.y + 35);
            ImVec4 statusColor = game.isRunning ? DearDoorTheme::Colors::Success : 
                                game.isInstalled ? DearDoorTheme::Colors::Info : 
                                                 DearDoorTheme::Colors::TextDisabled;
            const char* statusText = game.isRunning ? "Running" : 
                                    game.isInstalled ? "Installed" : "Not Installed";
            
            ImU32 statusColorU32 = ImGui::ColorConvertFloat4ToU32(statusColor);
            drawList->AddText(statusPos, statusColorU32, statusText);
            
            // Launch button
            ImVec2 buttonPos = ImVec2(pos.x + ImGui::GetContentRegionAvail().x - 100, 
                                     pos.y + 15);
            
            if (game.isInstalled && !game.isRunning) {
                ImGui::SetCursorScreenPos(buttonPos);
                if (m_uiComponents.CustomButton("Launch", ImVec2(80, 30), true, true)) {
                    launchGame(game.appId);
                }
            }
            
            ImGui::Separator();
            ImGui::PopID();
        }
        
        ImGui::EndChild();
    }
    
    void renderStatusBar() {
        ImGui::SetCursorPos(ImVec2(0, m_windowHeight - DearDoorTheme::Dimensions::BottomBarHeight));
        ImGui::Separator();
        
        ImGui::SetCursorPos(ImVec2(16, m_windowHeight - DearDoorTheme::Dimensions::BottomBarHeight + 10));
        
        // Status text
        ImGui::TextColored(DearDoorTheme::Colors::TextSecondary, 
                          "Ready | %zu games | %zu filtered", 
                          m_games.size(), m_filteredGames.size());
        
        // Right side info
        ImGui::SameLine();
        ImGui::SetCursorPosX(m_windowWidth - 200);
        ImGui::TextColored(DearDoorTheme::Colors::TextSecondary, "DearDoor v1.0");
    }
    
    void renderSettingsWindow() {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", &m_showSettings);
        
        ImGui::Text("Appearance");
        ImGui::Separator();
        
        // Theme selection
        static int currentTheme = 0;
        const char* themes[] = {"Dark", "Light", "OLED"};
        
        if (ImGui::Combo("Theme", &currentTheme, themes, 3)) {
            m_themeManager.applyTheme(themes[currentTheme]);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Display options
        ImGui::Text("Display");
        ImGui::Separator();
        
        ImGui::Checkbox("Show Sidebar", &m_showSidebar);
        ImGui::Checkbox("Show Grid", &m_showGrid);
        ImGui::Checkbox("Show List", &m_showList);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(100, 30))) {
            m_showSettings = false;
        }
        
        ImGui::End();
    }
    
    void launchGame(AppId_t appId) {
        if (SteamManager::GetInstance().LaunchGame(appId)) {
            std::cout << "Launched game: " << appId << std::endl;
            
            // Update game state
            auto it = std::find_if(m_games.begin(), m_games.end(),
                                  [appId](const GameInfo& g) { return g.appId == appId; });
            if (it != m_games.end()) {
                it->isRunning = true;
            }
        }
    }
    
    // Steam Manager implementation (simplified)
    class SteamManager {
    public:
        static SteamManager& GetInstance() {
            static SteamManager instance;
            return instance;
        }
        
        bool Initialize() {
            // Simplified initialization
            return true;
        }
        
        void Shutdown() {}
        
        std::vector<GameInfo> GetOwnedGames() {
            std::vector<GameInfo> games;
            // In production, this would query Steam API
            return games;
        }
        
        bool LaunchGame(AppId_t appId) {
            // In production, this would launch via Steam
            return true;
        }
    };
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main() {
    DearDoorMainWindow app;
    
    if (!app.initialize()) {
        std::cerr << "Failed to initialize DearDoor" << std::endl;
        return -1;
    }
    
    app.run();
    app.shutdown();
    
    return 0;
}