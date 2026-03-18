#pragma once

#include <Arduino.h>

/**
 * Common configuration constants for Papers3 JP Learner
 *
 * This header provides shared constants used across multiple files.
 */

// ============================================
// Screen Dimensions (M5Paper S3: 960x540)
// ============================================
constexpr int SCREEN_WIDTH = 960;
constexpr int SCREEN_HEIGHT = 540;
constexpr int TAB_HEIGHT = 80;
constexpr int CONTENT_HEIGHT = SCREEN_HEIGHT - TAB_HEIGHT;  // 460

// Common padding
constexpr int PAD_X = 20;
constexpr int PAD_Y = 20;

// ============================================
// Tab System
// ============================================
enum TabIndex {
    TAB_WORD = 0,
    TAB_GRAMMAR,
    TAB_COPY,
    TAB_READ,
    TAB_STATS,
    TAB_SETTINGS,
    TAB_COUNT
};

// ============================================
// SD Card directories
// ============================================
extern const char* DIR_BOOKS;
extern const char* DIR_DICT;
extern const char* DIR_FONTS;
extern const char* DIR_USERDATA;

// Configuration structure
struct Config {
    // Language
    String language = "ko";

    // Learning settings
    int newCardsPerDay = 20;
    int reviewLimit = -1;  // -1 = unlimited

    // Display settings
    String fontSize = "medium";  // small, medium, large
    String startScreen = "copy"; // word, grammar, copy, read

    // Daily reading (Copy screen)
    String dailyEpub = "";  // EPUB filename for daily content (empty = auto-detect)

    // System settings
    int sleepMinutes = 5;

    // WiFi AP settings (for file transfer)
    String apSsid = "Papers3-JP";
    String apPassword = "12345678";

    // WiFi Station settings (for external connection)
    String staSsid = "";
    String staPassword = "";
    bool autoConnect = false;
};

// Global config instance (defined in main.cpp)
extern Config config;

// Config functions
extern bool loadConfig();
extern bool saveConfig();
