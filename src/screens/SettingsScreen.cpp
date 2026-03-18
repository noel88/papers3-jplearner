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
    , _epubScrollOffset(0)
    , _fontScrollOffset(0)
    , _selectingFallback(false) {
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
        case SettingsState::Font:
            drawFontSettings();
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
        case SettingsState::Font:
            return handleFontSettingsTouch(x, y);
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
        "폰트 설정",
        "화면 설정",
        "학습 설정",
        "시스템 설정"
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
            case 3:  // 폰트 설정
                scanFontFiles();
                _selectingFallback = false;
                _state = SettingsState::Font;
                requestRedraw();
                return true;
            case 4:  // 화면 설정
                _state = SettingsState::Display;
                requestRedraw();
                return true;
            case 5:  // 학습 설정
                _state = SettingsState::Learning;
                requestRedraw();
                return true;
            case 6:  // 시스템 설정
                _state = SettingsState::System;
                requestRedraw();
                return true;
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

// ============================================
// Font Selection
// ============================================

void SettingsScreen::scanFontFiles() {
    _fontFiles.clear();
    _fontScrollOffset = 0;

    // Add "내장 폰트" option first
    _fontFiles.push_back("(내장 폰트)");

    File dir = SD.open(DIR_FONTS);
    if (!dir) {
        Serial.println("SettingsScreen: Cannot open fonts directory");
        return;
    }

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        entry.close();

        if (isFile) {
            String nameLower = name;
            nameLower.toLowerCase();
            if (nameLower.endsWith(".ttf")) {
                _fontFiles.push_back(name);
                Serial.printf("SettingsScreen: Found font: %s\n", name.c_str());
            }
        }
    }
    dir.close();

    Serial.printf("SettingsScreen: Total %d font files\n", _fontFiles.size() - 1);
}

void SettingsScreen::drawFontSettings() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Title with back button
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(PAD_X, PAD_Y);
    if (_selectingFallback) {
        M5.Display.print("< 대체 폰트 선택");
    } else {
        M5.Display.print("< 폰트 설정");
    }
    M5.Display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    // Current font info
    int y = 60;
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("Primary: ");
    M5.Display.print(config.primaryFont.length() > 0 ? config.primaryFont.c_str() : "(built-in)");
    M5.Display.setCursor(PAD_X + 350, y);
    M5.Display.printf("Size: %dpt", config.fontSizePt);
    y += 22;
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("Fallback: ");
    M5.Display.print(config.fallbackFont.length() > 0 ? config.fallbackFont.c_str() : "(none)");

    // Font size buttons
    int sizeY = y + 8;
    int sizeBtnW = 50;
    int sizeBtnH = 35;
    int sizeX = SCREEN_WIDTH - PAD_X - sizeBtnW * 2 - 80;

    // Minus button
    M5.Display.drawRect(sizeX, sizeY, sizeBtnW, sizeBtnH, TFT_BLACK);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setCursor(sizeX + 18, sizeY + 10);
    M5.Display.print("-");

    // Size display
    M5.Display.setCursor(sizeX + sizeBtnW + 10, sizeY + 10);
    M5.Display.printf("%d", config.fontSizePt);

    // Plus button
    M5.Display.drawRect(sizeX + sizeBtnW + 50, sizeY, sizeBtnW, sizeBtnH, TFT_BLACK);
    M5.Display.setCursor(sizeX + sizeBtnW + 68, sizeY + 10);
    M5.Display.print("+");

    // Mode buttons
    y += 50;
    int btnW = 200;
    int btnH = 45;
    int gap = 20;

    // Primary font button
    bool primarySelected = !_selectingFallback;
    if (primarySelected) {
        M5.Display.fillRect(PAD_X, y, btnW, btnH, TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE);
    } else {
        M5.Display.drawRect(PAD_X, y, btnW, btnH, TFT_BLACK);
        M5.Display.setTextColor(TFT_BLACK);
    }
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(0.9);
    M5.Display.setCursor(PAD_X + 20, y + 10);
    M5.Display.print("기본 폰트");

    // Fallback font button
    bool fallbackSelected = _selectingFallback;
    if (fallbackSelected) {
        M5.Display.fillRect(PAD_X + btnW + gap, y, btnW, btnH, TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE);
    } else {
        M5.Display.drawRect(PAD_X + btnW + gap, y, btnW, btnH, TFT_BLACK);
        M5.Display.setTextColor(TFT_BLACK);
    }
    M5.Display.setCursor(PAD_X + btnW + gap + 20, y + 10);
    M5.Display.print("대체 폰트");

    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(0.8);

    // Font list
    y += btnH + 15;
    int listStartY = y;
    int maxVisible = 4;  // Show 4 fonts

    for (int i = _fontScrollOffset; i < _fontFiles.size() && i < _fontScrollOffset + maxVisible; i++) {
        bool isSelected = false;

        // Check if this is the selected font
        String currentFont = _selectingFallback ? config.fallbackFont : config.primaryFont;
        if (i == 0 && currentFont.length() == 0) {
            isSelected = true;
        } else if (i > 0 && currentFont == _fontFiles[i]) {
            isSelected = true;
        }

        // Background for alternating rows
        if ((i - _fontScrollOffset) % 2 == 0) {
            M5.Display.fillRect(0, y, SCREEN_WIDTH, ITEM_HEIGHT - 1, 0xF7BE);
        }

        // Selection indicator
        if (isSelected) {
            M5.Display.fillRect(0, y, 8, ITEM_HEIGHT - 1, TFT_BLACK);
        }

        // Font name
        M5.Display.setFont(&fonts::Font2);
        M5.Display.setCursor(PAD_X + 10, y + 18);
        String displayName = _fontFiles[i];
        // Truncate if too long
        int maxWidth = SCREEN_WIDTH - PAD_X * 2 - 40;
        while (M5.Display.textWidth(displayName.c_str()) > maxWidth && displayName.length() > 10) {
            displayName = displayName.substring(0, displayName.length() - 1);
        }
        if (displayName.length() < _fontFiles[i].length()) {
            displayName += "...";
        }
        M5.Display.print(displayName);

        // Bottom border
        M5.Display.drawLine(PAD_X, y + ITEM_HEIGHT - 1,
                           SCREEN_WIDTH - PAD_X, y + ITEM_HEIGHT - 1, 0xDEDB);

        y += ITEM_HEIGHT;
    }

    // Navigation buttons
    bool needsPaging = _fontFiles.size() > maxVisible;
    if (needsPaging) {
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextSize(1.0);

        int btnY = y + 10;
        int navBtnW = 150;
        int navBtnH = 45;

        // Previous
        if (_fontScrollOffset > 0) {
            M5.Display.fillRect(PAD_X, btnY, navBtnW, navBtnH, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(PAD_X + 35, btnY + 10);
            M5.Display.print("< 이전");
        }

        // Next
        int nextBtnX = SCREEN_WIDTH - PAD_X - navBtnW;
        if (_fontScrollOffset + maxVisible < _fontFiles.size()) {
            M5.Display.fillRect(nextBtnX, btnY, navBtnW, navBtnH, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
            M5.Display.setCursor(nextBtnX + 35, btnY + 10);
            M5.Display.print("다음 >");
        }

        M5.Display.setTextColor(TFT_BLACK);
    }

    // Help text
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(0x8410);
    M5.Display.setCursor(PAD_X, CONTENT_HEIGHT - 25);
    M5.Display.print("Place TTF files in /fonts/ folder on SD card");
    M5.Display.setTextColor(TFT_BLACK);
}

bool SettingsScreen::handleFontSettingsTouch(int x, int y) {
    // Back button (top area)
    if (y < BACK_BUTTON_HEIGHT) {
        _state = SettingsState::Main;
        requestRedraw();
        return true;
    }

    // Font size buttons (at y=90, right side)
    int sizeY = 90;
    int sizeBtnW = 50;
    int sizeBtnH = 35;
    int sizeX = SCREEN_WIDTH - PAD_X - sizeBtnW * 2 - 80;

    if (y >= sizeY && y <= sizeY + sizeBtnH) {
        // Minus button
        if (x >= sizeX && x <= sizeX + sizeBtnW) {
            if (config.fontSizePt > 16) {
                config.fontSizePt -= 2;
                saveConfig();
                requestRedraw();
            }
            return true;
        }
        // Plus button
        if (x >= sizeX + sizeBtnW + 50 && x <= sizeX + sizeBtnW * 2 + 50) {
            if (config.fontSizePt < 36) {
                config.fontSizePt += 2;
                saveConfig();
                requestRedraw();
            }
            return true;
        }
    }

    // Mode toggle buttons (Primary / Fallback)
    int modeY = 132;  // After size buttons
    int btnW = 200;
    int btnH = 45;
    int gap = 20;

    if (y >= modeY && y <= modeY + btnH) {
        if (x >= PAD_X && x <= PAD_X + btnW) {
            // Primary font button
            _selectingFallback = false;
            requestRedraw();
            return true;
        } else if (x >= PAD_X + btnW + gap && x <= PAD_X + btnW * 2 + gap) {
            // Fallback font button
            _selectingFallback = true;
            requestRedraw();
            return true;
        }
    }

    // Font list
    int listStartY = modeY + btnH + 15;  // 132 + 45 + 15 = 192
    int maxVisible = 4;
    int listEndY = listStartY + maxVisible * ITEM_HEIGHT;

    // Navigation buttons
    bool needsPaging = _fontFiles.size() > maxVisible;
    if (needsPaging) {
        int navBtnY = listEndY + 10;
        int navBtnW = 150;
        int navBtnH = 45;

        if (y >= navBtnY && y <= navBtnY + navBtnH) {
            // Previous
            if (x >= PAD_X && x <= PAD_X + navBtnW && _fontScrollOffset > 0) {
                _fontScrollOffset -= maxVisible;
                if (_fontScrollOffset < 0) _fontScrollOffset = 0;
                requestRedraw();
                return true;
            }

            // Next
            int nextBtnX = SCREEN_WIDTH - PAD_X - navBtnW;
            if (x >= nextBtnX && x <= nextBtnX + navBtnW &&
                _fontScrollOffset + maxVisible < _fontFiles.size()) {
                _fontScrollOffset += maxVisible;
                requestRedraw();
                return true;
            }
        }
    }

    // Font selection
    if (y >= listStartY && y < listEndY) {
        int visibleCount = min((int)_fontFiles.size() - _fontScrollOffset, maxVisible);
        if (y < listStartY + visibleCount * ITEM_HEIGHT) {
            int touchedIndex = (y - listStartY) / ITEM_HEIGHT + _fontScrollOffset;

            if (touchedIndex >= 0 && touchedIndex < _fontFiles.size()) {
                String selectedFont = (touchedIndex == 0) ? "" : _fontFiles[touchedIndex];

                if (_selectingFallback) {
                    config.fallbackFont = selectedFont;
                    Serial.printf("SettingsScreen: Fallback font: %s\n",
                                  selectedFont.length() > 0 ? selectedFont.c_str() : "(none)");
                } else {
                    config.primaryFont = selectedFont;
                    Serial.printf("SettingsScreen: Primary font set to: '%s'\n",
                                  selectedFont.length() > 0 ? selectedFont.c_str() : "(built-in)");
                }
                Serial.flush();

                // Save config
                saveConfig();
                Serial.println("SettingsScreen: Config saved");
                Serial.flush();
                requestRedraw();
                return true;
            }
        }
    }

    return false;
}
