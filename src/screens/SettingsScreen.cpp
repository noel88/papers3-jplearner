#include "screens/SettingsScreen.h"
#include <M5Unified.h>
#include <SD.h>

// Forward declaration for SD card info
extern uint64_t getSDCardFreeSpace();

SettingsScreen::SettingsScreen()
    : BaseScreen()
    , _state(SettingsState::Main)
    , _wifiModeCallback(nullptr) {
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
        "화면 설정",
        "학습 설정",
        "시스템 설정",
        "SD 카드 정보"
    };

    for (int i = 0; i < 6; i++) {
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
    if (y >= ITEMS_START_Y && y < ITEMS_START_Y + 6 * ITEM_HEIGHT) {
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
            case 2:  // 화면 설정
                _state = SettingsState::Display;
                requestRedraw();
                return true;
            case 3:  // 학습 설정
                _state = SettingsState::Learning;
                requestRedraw();
                return true;
            case 4:  // 시스템 설정
                _state = SettingsState::System;
                requestRedraw();
                return true;
            case 5:  // SD 카드 정보
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
