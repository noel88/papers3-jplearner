#include "screens/SettingsScreen.h"
#include "Config.h"
#include <M5Unified.h>
#include <SD.h>

// Forward declaration for SD card info
extern uint64_t getSDCardFreeSpace();

SettingsScreen::SettingsScreen()
    : BaseScreen()
    , _state(SettingsState::Main)
    , _wifiModeCallback(nullptr)
    , _epubScrollOffset(0) {
}

void SettingsScreen::onExit() {
    BaseScreen::onExit();
    // Reset to main menu when leaving settings
    _state = SettingsState::Main;
}

void SettingsScreen::draw() {
    switch (_state) {
        case SettingsState::Main:
            drawMainMenu();
            break;
        case SettingsState::WiFiAP:
            drawWiFiAPSettings();
            break;
        case SettingsState::WiFiSTA:
            drawWiFiSTASettings();
            break;
        case SettingsState::Display:
            drawDisplaySettings();
            break;
        case SettingsState::DailyEpub:
            drawDailyEpubSettings();
            break;
        case SettingsState::Learning:
            drawLearningSettings();
            break;
        case SettingsState::System:
            drawSystemSettings();
            break;
        default:
            drawMainMenu();
            break;
    }
}

bool SettingsScreen::handleTouchStart(int x, int y) {
    switch (_state) {
        case SettingsState::Main:
            return handleMainMenuTouch(x, y);
        case SettingsState::WiFiAP:
            return handleWiFiAPTouch(x, y);
        case SettingsState::WiFiSTA:
            return handleWiFiSTATouch(x, y);
        case SettingsState::DailyEpub:
            return handleDailyEpubTouch(x, y);
        case SettingsState::Display:
        case SettingsState::Learning:
        case SettingsState::System:
            return handleBackButton(y);
        default:
            return false;
    }
}

// ============================================
// Drawing Methods
// ============================================

void SettingsScreen::drawMainMenu() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Title
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(PAD_X, PAD_Y);
    M5.Display.print("설정");

    // Divider
    M5.Display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    M5.Display.setTextSize(1.0);
    int y = ITEMS_START_Y;

    // Menu items
    const char* menuItems[] = {
        "WiFi 파일 전송",
        "WiFi 연결 설정",
        "필사 EPUB 선택",
        "화면 설정",
        "학습 설정",
        "시스템 설정",
        "SD 카드 정보"
    };

    for (int i = 0; i < 7; i++) {
        // Draw item background (alternating)
        if (i % 2 == 0) {
            M5.Display.fillRect(0, y, SCREEN_WIDTH, ITEM_HEIGHT - 1, 0xF7BE);
        }

        // Draw item label
        M5.Display.setCursor(PAD_X, y + 15);
        M5.Display.print(menuItems[i]);

        // Draw arrow
        M5.Display.setCursor(SCREEN_WIDTH - 60, y + 15);
        M5.Display.print(">");

        // Draw bottom border
        M5.Display.drawLine(PAD_X, y + ITEM_HEIGHT - 1,
                           SCREEN_WIDTH - PAD_X, y + ITEM_HEIGHT - 1, 0xDEDB);

        y += ITEM_HEIGHT;
    }

    // SD Card info at bottom
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(PAD_X, CONTENT_HEIGHT - 30);
    M5.Display.printf("SD: %llu MB free / %llu MB total",
        getSDCardFreeSpace() / (1024 * 1024),
        SD.totalBytes() / (1024 * 1024));

    M5.Display.setFont(&fonts::efontKR_24);
}

void SettingsScreen::drawWiFiAPSettings() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Title with back button
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(PAD_X, PAD_Y);
    M5.Display.print("< WiFi 파일 전송");
    M5.Display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    M5.Display.setTextSize(1.0);
    int y = ITEMS_START_Y + 10;

    // Current AP settings
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("현재 설정:");
    y += 40;

    M5.Display.setCursor(PAD_X + 20, y);
    M5.Display.printf("SSID: %s", config.apSsid.c_str());
    y += 35;

    M5.Display.setCursor(PAD_X + 20, y);
    M5.Display.printf("비밀번호: %s", config.apPassword.c_str());
    y += 50;

    // Start button
    int btnY = y + 20;
    int btnW = 300;
    int btnH = 60;
    int btnX = (SCREEN_WIDTH - btnW) / 2;

    M5.Display.fillRect(btnX, btnY, btnW, btnH, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(1.0);

    int textW = M5.Display.textWidth("파일 전송 시작");
    M5.Display.setCursor(btnX + (btnW - textW) / 2, btnY + 18);
    M5.Display.print("파일 전송 시작");

    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(1.0);

    // Instructions
    y = btnY + btnH + 30;
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("1. Start file transfer");
    y += 25;
    M5.Display.setCursor(PAD_X, y);
    M5.Display.printf("2. Connect to WiFi '%s'", config.apSsid.c_str());
    y += 25;
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("3. Open http://192.168.4.1 in browser");

    M5.Display.setFont(&fonts::efontKR_24);
}

void SettingsScreen::drawWiFiSTASettings() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Title with back button
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(PAD_X, PAD_Y);
    M5.Display.print("< WiFi 연결 설정");
    M5.Display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    M5.Display.setTextSize(1.0);
    int y = ITEMS_START_Y + 10;

    // Current Station settings
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("외부 WiFi 연결:");
    y += 40;

    if (config.staSsid.length() > 0) {
        M5.Display.setCursor(PAD_X + 20, y);
        M5.Display.printf("SSID: %s", config.staSsid.c_str());
        y += 35;

        M5.Display.setCursor(PAD_X + 20, y);
        M5.Display.print("상태: 설정됨");
    } else {
        M5.Display.setCursor(PAD_X + 20, y);
        M5.Display.print("설정된 네트워크 없음");
    }
    y += 50;

    // Scan button
    int btnY = y + 20;
    int btnW = 250;
    int btnH = 50;
    int btnX = (SCREEN_WIDTH - btnW) / 2;

    M5.Display.drawRect(btnX, btnY, btnW, btnH, TFT_BLACK);
    M5.Display.setTextSize(1.1);

    int textW = M5.Display.textWidth("WiFi 스캔");
    M5.Display.setCursor(btnX + (btnW - textW) / 2, btnY + 12);
    M5.Display.print("WiFi 스캔");

    M5.Display.setTextSize(1.0);

    // Note
    y = btnY + btnH + 40;
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("Note: WiFi scan feature coming soon.");
    y += 25;
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("Currently use AP mode for file transfer.");

    M5.Display.setFont(&fonts::efontKR_24);
}

void SettingsScreen::drawDisplaySettings() {
    clearContentArea();
    drawCenteredText("화면 설정", CONTENT_HEIGHT / 2 - 40);
    drawCenteredText("< 뒤로가기: 상단 터치", CONTENT_HEIGHT / 2 + 20);
}

void SettingsScreen::drawLearningSettings() {
    clearContentArea();
    drawCenteredText("학습 설정", CONTENT_HEIGHT / 2 - 40);
    drawCenteredText("< 뒤로가기: 상단 터치", CONTENT_HEIGHT / 2 + 20);
}

void SettingsScreen::drawSystemSettings() {
    clearContentArea();
    drawCenteredText("시스템 설정", CONTENT_HEIGHT / 2 - 40);
    drawCenteredText("< 뒤로가기: 상단 터치", CONTENT_HEIGHT / 2 + 20);
}

// ============================================
// Touch Handlers
// ============================================

bool SettingsScreen::handleMainMenuTouch(int x, int y) {
    // Check which menu item was touched
    if (y >= ITEMS_START_Y && y < ITEMS_START_Y + 7 * ITEM_HEIGHT) {
        int itemIndex = (y - ITEMS_START_Y) / ITEM_HEIGHT;
        Serial.printf("SettingsScreen: item touched: %d\n", itemIndex);

        switch (itemIndex) {
            case 0:  // WiFi 파일 전송
                _state = SettingsState::WiFiAP;
                requestRedraw();
                return true;
            case 1:  // WiFi 연결 설정
                _state = SettingsState::WiFiSTA;
                requestRedraw();
                return true;
            case 2:  // 필사 EPUB 선택
                scanEpubFiles();
                _state = SettingsState::DailyEpub;
                requestRedraw();
                return true;
            case 3:  // 화면 설정
                _state = SettingsState::Display;
                requestRedraw();
                return true;
            case 4:  // 학습 설정
                _state = SettingsState::Learning;
                requestRedraw();
                return true;
            case 5:  // 시스템 설정
                _state = SettingsState::System;
                requestRedraw();
                return true;
            case 6:  // SD 카드 정보
                // Just show info, no action needed
                break;
        }
    }
    return false;
}

bool SettingsScreen::handleWiFiAPTouch(int x, int y) {
    // Back button (top area)
    if (y < BACK_BUTTON_HEIGHT) {
        _state = SettingsState::Main;
        requestRedraw();
        return true;
    }

    // Start file transfer button
    int btnY = ITEMS_START_Y + 10 + 40 + 35 + 50 + 20;
    int btnH = 60;
    int btnW = 300;
    int btnX = (SCREEN_WIDTH - btnW) / 2;

    if (y >= btnY && y <= btnY + btnH &&
        x >= btnX && x <= btnX + btnW) {
        if (_wifiModeCallback) {
            _wifiModeCallback();
        }
        return false;  // WiFi mode handles its own display
    }

    return false;
}

bool SettingsScreen::handleWiFiSTATouch(int x, int y) {
    // Back button (top area)
    if (y < BACK_BUTTON_HEIGHT) {
        _state = SettingsState::Main;
        requestRedraw();
        return true;
    }
    // WiFi scan button - TODO: implement
    return false;
}

bool SettingsScreen::handleBackButton(int y) {
    if (y < BACK_BUTTON_HEIGHT) {
        _state = SettingsState::Main;
        requestRedraw();
        return true;
    }
    return false;
}

// ============================================
// EPUB Selection
// ============================================

void SettingsScreen::scanEpubFiles() {
    _epubFiles.clear();
    _epubScrollOffset = 0;

    // Add "자동 선택" option first
    _epubFiles.push_back("(자동 선택)");

    File dir = SD.open("/books");
    if (!dir) {
        Serial.println("SettingsScreen: Cannot open books directory");
        return;
    }

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        entry.close();

        if (isFile && name.endsWith(".epub")) {
            _epubFiles.push_back(name);
            Serial.printf("SettingsScreen: Found EPUB: %s\n", name.c_str());
        }
    }
    dir.close();

    Serial.printf("SettingsScreen: Total %d EPUB files\n", _epubFiles.size() - 1);
}

void SettingsScreen::drawDailyEpubSettings() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Title with back button
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(PAD_X, PAD_Y);
    M5.Display.print("< 필사 EPUB 선택");
    M5.Display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    // Current selection info
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(PAD_X, 55);
    M5.Display.printf("Page %d/%d", _epubScrollOffset / 6 + 1,
                      (_epubFiles.size() + 5) / 6);

    M5.Display.setTextSize(0.8);

    // List EPUB files
    int y = 85;
    int maxVisible = 5;  // Show 5 items to leave room for buttons

    for (int i = _epubScrollOffset; i < _epubFiles.size() && i < _epubScrollOffset + maxVisible; i++) {
        bool isSelected = false;

        // Check if this is the selected file
        if (i == 0 && config.dailyEpub.length() == 0) {
            isSelected = true;  // Auto-detect selected
        } else if (i > 0 && config.dailyEpub == _epubFiles[i]) {
            isSelected = true;
        }

        // Background for alternating rows
        if ((i - _epubScrollOffset) % 2 == 0) {
            M5.Display.fillRect(0, y, SCREEN_WIDTH, ITEM_HEIGHT - 1, 0xF7BE);
        }

        // Selection indicator
        if (isSelected) {
            M5.Display.fillRect(0, y, 8, ITEM_HEIGHT - 1, TFT_BLACK);
        }

        // Use Korean font for first item, Japanese for EPUB filenames
        if (i == 0) {
            M5.Display.setFont(&fonts::efontKR_24);
        } else {
            M5.Display.setFont(&fonts::efontJA_24);
        }
        M5.Display.setTextSize(0.8);

        // File name (truncate if too long)
        String displayName = _epubFiles[i];
        int maxWidth = SCREEN_WIDTH - PAD_X * 2 - 60;

        // Truncate for display
        while (M5.Display.textWidth(displayName.c_str()) > maxWidth && displayName.length() > 10) {
            displayName = displayName.substring(0, displayName.length() - 1);
        }
        if (displayName.length() < _epubFiles[i].length()) {
            displayName += "...";
        }

        M5.Display.setCursor(PAD_X + 10, y + 12);
        M5.Display.print(displayName);

        // Draw bottom border
        M5.Display.drawLine(PAD_X, y + ITEM_HEIGHT - 1,
                           SCREEN_WIDTH - PAD_X, y + ITEM_HEIGHT - 1, 0xDEDB);

        y += ITEM_HEIGHT;
    }

    // Navigation buttons - only show if more than one page
    bool needsPaging = _epubFiles.size() > maxVisible;

    if (needsPaging) {
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextSize(1.0);

        int btnY = y + 10;  // Right after the list
        int btnW = 180;
        int btnH = 50;

        // Previous page button
        if (_epubScrollOffset > 0) {
            M5.Display.fillRect(PAD_X, btnY, btnW, btnH, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(PAD_X + 40, btnY + 12);
            M5.Display.print("< 이전");
        }

        // Next page button
        int nextBtnX = SCREEN_WIDTH - PAD_X - btnW;
        if (_epubScrollOffset + maxVisible < _epubFiles.size()) {
            M5.Display.fillRect(nextBtnX, btnY, btnW, btnH, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(nextBtnX + 40, btnY + 12);
            M5.Display.print("다음 >");
        }

        M5.Display.setTextColor(TFT_BLACK);
    }

    // Help text at bottom
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(0x8410);  // Gray
    M5.Display.setCursor(PAD_X, CONTENT_HEIGHT - 25);
    M5.Display.print("Tap to select. Touch top to go back.");
    M5.Display.setTextColor(TFT_BLACK);
}

bool SettingsScreen::handleDailyEpubTouch(int x, int y) {
    // Back button (top area)
    if (y < BACK_BUTTON_HEIGHT) {
        _state = SettingsState::Main;
        requestRedraw();
        return true;
    }

    int maxVisible = 5;
    int listStartY = 85;
    int visibleCount = min((int)_epubFiles.size() - _epubScrollOffset, maxVisible);
    int listEndY = listStartY + visibleCount * ITEM_HEIGHT;

    // Navigation buttons (only if paging is needed)
    bool needsPaging = _epubFiles.size() > maxVisible;
    if (needsPaging) {
        int btnY = listEndY + 10;
        int btnW = 180;
        int btnH = 50;

        if (y >= btnY && y <= btnY + btnH) {
            // Previous page button
            if (x >= PAD_X && x <= PAD_X + btnW && _epubScrollOffset > 0) {
                _epubScrollOffset -= maxVisible;
                if (_epubScrollOffset < 0) _epubScrollOffset = 0;
                requestRedraw();
                return true;
            }

            // Next page button
            int nextBtnX = SCREEN_WIDTH - PAD_X - btnW;
            if (x >= nextBtnX && x <= nextBtnX + btnW &&
                _epubScrollOffset + maxVisible < _epubFiles.size()) {
                _epubScrollOffset += maxVisible;
                requestRedraw();
                return true;
            }
        }
    }

    // EPUB list selection - only within actual list items
    if (y >= listStartY && y < listEndY) {
        int touchedIndex = (y - listStartY) / ITEM_HEIGHT + _epubScrollOffset;

        if (touchedIndex >= 0 && touchedIndex < _epubFiles.size()) {
            if (touchedIndex == 0) {
                // Auto-detect
                config.dailyEpub = "";
            } else {
                config.dailyEpub = _epubFiles[touchedIndex];
            }

            Serial.printf("SettingsScreen: Selected EPUB: %s\n",
                          config.dailyEpub.length() > 0 ? config.dailyEpub.c_str() : "(auto)");

            // Save config
            saveConfig();

            // Go back to main menu after selection
            _state = SettingsState::Main;
            requestRedraw();
            return true;
        }
    }

    return false;
}
