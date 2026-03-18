#pragma once

#include <Arduino.h>

// Screen dimensions (M5Paper S3 landscape)
const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;

// Tab bar configuration
const int TAB_BAR_HEIGHT = 50;
const int TAB_COUNT = 6;
const int TAB_WIDTH = SCREEN_WIDTH / TAB_COUNT;

// Content area
const int CONTENT_HEIGHT = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

// Padding
const int PAD_X = 20;
const int PAD_Y = 15;

// Line spacing for wrapped text
const int LINE_SPACING = 8;

// SD Card pins for M5Paper S3
const int SD_CS = 47;
const int SD_SCK = 39;
const int SD_MISO = 40;
const int SD_MOSI = 38;

// File paths
const char* const DIR_BOOKS = "/books";
const char* const DIR_DICT = "/dict";
const char* const DIR_FONTS = "/fonts";
const char* const DIR_USERDATA = "/userdata";
const char* const CONFIG_PATH = "/config.json";

// Tab indices
enum TabIndex {
    TAB_WORD = 0,
    TAB_GRAMMAR = 1,
    TAB_COPY = 2,
    TAB_READ = 3,
    TAB_STATS = 4,
    TAB_SETTINGS = 5
};

// Tab labels
const char* const TAB_LABELS[] = {"단어", "문형", "필사", "읽기", "통계", "설정"};

// Battery check interval
const unsigned long BATTERY_CHECK_INTERVAL = 60000;  // 1 minute

// App configuration structure
struct AppConfig {
    // Display
    int fontSize = 24;
    int startScreen = TAB_COPY;

    // System
    int sleepMinutes = 30;

    // WiFi AP mode
    String apSsid = "M5Paper-JP";
    String apPassword = "12345678";

    // WiFi Station mode
    String staSsid = "";
    String staPassword = "";
    bool autoConnect = false;
};
