#pragma once

#include "BaseScreen.h"
#include <vector>

// Forward declaration for Config access
struct Config;
extern Config config;
extern bool saveConfig();

/**
 * Settings menu states (State Pattern)
 */
enum class SettingsState {
    Main,
    WiFiAP,
    WiFiSTA,
    Display,
    Font,       // Font selection (TTF from SD card)
    DailyEpub,  // EPUB file selection for Copy screen
    Learning,
    System
};

/**
 * Settings Screen with hierarchical menu navigation.
 *
 * Design Pattern: State Pattern
 * - Each menu state has its own draw and touch handler
 * - Clean transitions between states
 *
 * Also demonstrates separation of concerns:
 * - Screen handles display and navigation
 * - Config struct holds actual settings data
 */
class SettingsScreen : public BaseScreen {
public:
    SettingsScreen();

    // ============================================
    // IScreen Implementation
    // ============================================

    const char* getName() const override { return "Settings"; }

    void draw() override;

    bool handleTouchStart(int x, int y) override;

    void onExit() override;

    // ============================================
    // WiFi Mode Callback
    // ============================================

    /**
     * Set callback for starting WiFi mode
     * This is called when user taps "파일 전송 시작" button
     */
    using WiFiModeCallback = void(*)();
    void setWiFiModeCallback(WiFiModeCallback callback) {
        _wifiModeCallback = callback;
    }

private:
    SettingsState _state;
    WiFiModeCallback _wifiModeCallback;

    // EPUB selection
    std::vector<String> _epubFiles;
    int _epubScrollOffset;

    // Font selection
    std::vector<String> _fontFiles;
    int _fontScrollOffset;
    bool _selectingFallback;  // true when selecting fallback font

    // ============================================
    // Drawing Methods for Each State
    // ============================================

    void drawMainMenu();
    void drawWiFiAPSettings();
    void drawWiFiSTASettings();
    void drawDisplaySettings();
    void drawFontSettings();
    void drawDailyEpubSettings();
    void drawLearningSettings();
    void drawSystemSettings();

    // ============================================
    // Touch Handlers for Each State
    // ============================================

    bool handleMainMenuTouch(int x, int y);
    bool handleWiFiAPTouch(int x, int y);
    bool handleWiFiSTATouch(int x, int y);
    bool handleFontSettingsTouch(int x, int y);
    bool handleDailyEpubTouch(int x, int y);
    bool handleBackButton(int y);

    // ============================================
    // Helper Methods
    // ============================================

    void scanEpubFiles();
    void scanFontFiles();

    // ============================================
    // Constants
    // ============================================

    static constexpr int ITEM_HEIGHT = 55;
    static constexpr int ITEMS_START_Y = 60;
    static constexpr int BACK_BUTTON_HEIGHT = 50;
};
