#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"

// Screen architecture includes
#include "screens/ScreenManager.h"
#include "screens/CopyScreen.h"
#include "screens/ReadScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/PlaceholderScreen.h"
#include "Config.h"
#include "FontManager.h"
#include "WebUI.h"
#include "UIHelpers.h"
#include "SleepManager.h"

// ============================================
// Hardware Configuration
// ============================================
AsyncWebServer server(80);

// M5Paper S3 SD Card pins (correct pinout)
#define SD_CS   47
#define SD_SCK  39
#define SD_MISO 40
#define SD_MOSI 38

// ============================================
// Constants (also defined in BaseScreen.h)
// ============================================
const int TAB_BAR_HEIGHT = 60;
const int BATTERY_WIDTH = 80;
const int TAB_WIDTH = (SCREEN_WIDTH - BATTERY_WIDTH) / TAB_COUNT;

const char* TAB_LABELS[] = {
    "단어",
    "문형",
    "필사",
    "읽기",
    "통계",
    "설정"
};

// SD Card directories
const char* DIR_BOOKS = "/books";
const char* DIR_DICT = "/dict";
const char* DIR_FONTS = "/fonts";
const char* DIR_USERDATA = "/userdata";

// Config file path (stored in internal LittleFS)
const char* CONFIG_PATH = "/config.json";

// ============================================
// Global Config Instance (from Config.h)
// ============================================
Config config;

// ============================================
// Screen Instances (static allocation)
// ============================================
CopyScreen copyScreen;
ReadScreen readScreen;
SettingsScreen settingsScreen;
PlaceholderScreen wordScreen("Word", "단어 학습", "Coming soon - SRS 단어 학습");
PlaceholderScreen grammarScreen("Grammar", "문형 학습", "Coming soon - N2/N1 문법 패턴");
PlaceholderScreen statsScreen("Stats", "학습 통계", "Coming soon - 학습 진행 현황");

// ============================================
// State
// ============================================
bool sdCardMounted = false;
bool wifiMode = false;
bool needsFullRedraw = true;
bool needsTabRedraw = false;

// WiFi state
String currentUploadPath = "";
File uploadFile;

// Battery state
int batteryPercent = 100;
unsigned long lastBatteryCheck = 0;
const unsigned long BATTERY_CHECK_INTERVAL = 60000;

// ============================================
// SD Card Functions
// ============================================
bool initSDCard() {
    Serial.println("  SPI.begin...");
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    yield();

    Serial.println("  SD.begin...");
    if (!SD.begin(SD_CS, SPI, 25000000)) {
        Serial.println("SD Card mount failed");
        return false;
    }
    Serial.println("  SD.begin OK");

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("No SD card attached");
        return false;
    }

    Serial.print("SD Card Type: ");
    switch (cardType) {
        case CARD_MMC:  Serial.println("MMC"); break;
        case CARD_SD:   Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("UNKNOWN"); break;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    return true;
}

bool ensureDirectories() {
    const char* dirs[] = {DIR_BOOKS, DIR_DICT, DIR_FONTS, DIR_USERDATA};

    for (const char* dir : dirs) {
        if (!SD.exists(dir)) {
            if (!SD.mkdir(dir)) {
                Serial.printf("Failed to create directory: %s\n", dir);
                return false;
            }
            Serial.printf("Created directory: %s\n", dir);
        }
    }
    return true;
}

void showSDCardError(const char* message) {
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);

    int centerY = CONTENT_HEIGHT / 2;

    M5.Display.setCursor(PAD_X, centerY - 40);
    M5.Display.setTextSize(1.0);
    M5.Display.println("SD Card Error");

    M5.Display.setCursor(PAD_X, centerY + 20);
    M5.Display.setTextSize(1.0);
    M5.Display.println(message);

    M5.Display.setCursor(PAD_X, centerY + 60);
    M5.Display.println("Please insert SD card and restart.");

    M5.Display.display();
}

uint64_t getSDCardFreeSpace() {
    return SD.totalBytes() - SD.usedBytes();
}

// ============================================
// Configuration Functions
// ============================================
bool loadConfig() {
    if (!LittleFS.exists(CONFIG_PATH)) {
        Serial.println("Config file not found, using defaults");
        return false;
    }

    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) {
        Serial.println("Failed to open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        Serial.printf("Config parse error: %s\n", error.c_str());
        return false;
    }

    config.language = doc["language"] | "ko";
    config.newCardsPerDay = doc["learning"]["newCardsPerDay"] | 20;
    config.reviewLimit = doc["learning"]["reviewLimit"] | -1;
    config.fontSize = doc["display"]["fontSize"] | "medium";
    config.startScreen = doc["display"]["startScreen"] | "copy";
    config.dailyEpub = doc["display"]["dailyEpub"] | "";
    config.primaryFont = doc["display"]["primaryFont"] | "";
    config.fallbackFont = doc["display"]["fallbackFont"] | "";
    config.fontSizePt = doc["display"]["fontSizePt"] | 24;
    config.sleepMinutes = doc["system"]["sleepMinutes"] | 5;
    config.apSsid = doc["wifi"]["ap"]["ssid"] | "Papers3-JP";
    config.apPassword = doc["wifi"]["ap"]["password"] | "12345678";
    config.staSsid = doc["wifi"]["station"]["ssid"] | "";
    config.staPassword = doc["wifi"]["station"]["password"] | "";
    config.autoConnect = doc["wifi"]["station"]["autoConnect"] | false;

    Serial.println("Config loaded successfully");
    return true;
}

bool saveConfig() {
    JsonDocument doc;

    doc["language"] = config.language;
    doc["learning"]["newCardsPerDay"] = config.newCardsPerDay;
    doc["learning"]["reviewLimit"] = config.reviewLimit;
    doc["display"]["fontSize"] = config.fontSize;
    doc["display"]["startScreen"] = config.startScreen;
    doc["display"]["dailyEpub"] = config.dailyEpub;
    doc["display"]["primaryFont"] = config.primaryFont;
    doc["display"]["fallbackFont"] = config.fallbackFont;
    doc["display"]["fontSizePt"] = config.fontSizePt;
    doc["system"]["sleepMinutes"] = config.sleepMinutes;
    doc["wifi"]["ap"]["ssid"] = config.apSsid;
    doc["wifi"]["ap"]["password"] = config.apPassword;
    doc["wifi"]["station"]["ssid"] = config.staSsid;
    doc["wifi"]["station"]["password"] = config.staPassword;
    doc["wifi"]["station"]["autoConnect"] = config.autoConnect;

    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) {
        Serial.println("Failed to create config file");
        return false;
    }

    serializeJsonPretty(doc, f);
    f.close();

    Serial.println("Config saved successfully");
    return true;
}

// ============================================
// Battery Functions
// ============================================
int readBatteryPercent() {
    // Use M5Unified Power API for M5Paper S3
    int percent = M5.Power.getBatteryLevel();

    // If M5Unified returns -1 or invalid, try voltage-based calculation
    if (percent < 0 || percent > 100) {
        int32_t voltage = M5.Power.getBatteryVoltage();
        if (voltage > 0) {
            // Convert mV to percent (3000mV = 0%, 4200mV = 100%)
            percent = (voltage - 3000) * 100 / 1200;
        } else {
            // Fallback to ADC reading
            int adcValue = analogRead(10);
            float v = (adcValue / 4095.0) * 3.3 * 2;
            percent = (int)((v - 3.0) / 1.2 * 100);
        }
    }

    return constrain(percent, 0, 100);
}

void updateBattery() {
    unsigned long now = millis();
    if (now - lastBatteryCheck >= BATTERY_CHECK_INTERVAL || lastBatteryCheck == 0) {
        batteryPercent = readBatteryPercent();
        lastBatteryCheck = now;
        needsTabRedraw = true;
    }
}

// ============================================
// Tab Bar Functions
// ============================================
void drawTabBar() {
    int tabY = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    TabIndex currentTab = ScreenManager::instance().getCurrentTab();

    M5.Display.fillRect(0, tabY, SCREEN_WIDTH, TAB_BAR_HEIGHT, TFT_WHITE);
    M5.Display.drawLine(0, tabY, SCREEN_WIDTH, tabY, TFT_BLACK);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(UI::SIZE_BODY);

    for (int i = 0; i < TAB_COUNT; i++) {
        int tabX = i * TAB_WIDTH;

        if (i == currentTab) {
            M5.Display.fillRect(tabX + 2, tabY + 2, TAB_WIDTH - 4, TAB_BAR_HEIGHT - 4, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
        } else {
            M5.Display.setTextColor(TFT_BLACK);
        }

        int labelWidth = M5.Display.textWidth(TAB_LABELS[i]);
        int labelX = tabX + (TAB_WIDTH - labelWidth) / 2;
        int labelY = tabY + (TAB_BAR_HEIGHT - M5.Display.fontHeight()) / 2;

        // Draw bold tab label
        UI::drawBoldText(TAB_LABELS[i], labelX, labelY);
    }

    M5.Display.setTextColor(TFT_BLACK);

    // Battery indicator
    int battX = TAB_COUNT * TAB_WIDTH + 10;
    int battY = tabY + (TAB_BAR_HEIGHT - 24) / 2;

    // Battery outline
    uint16_t battColor = TFT_BLACK;
    if (batteryPercent <= 10) {
        battColor = TFT_RED;  // Low battery warning
    } else if (batteryPercent <= 20) {
        battColor = 0xFD20;  // Orange warning (RGB565)
    }

    M5.Display.drawRect(battX, battY, 40, 24, battColor);
    M5.Display.fillRect(battX + 40, battY + 6, 4, 12, battColor);

    // Battery fill
    int fillWidth = (36 * batteryPercent) / 100;
    if (fillWidth > 0) {
        M5.Display.fillRect(battX + 2, battY + 2, fillWidth, 20, battColor);
    }

    // Reset font state
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
}

int handleTabTouch(int touchX, int touchY) {
    int tabY = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    if (touchY < tabY) {
        return -1;
    }

    for (int i = 0; i < TAB_COUNT; i++) {
        int tabX = i * TAB_WIDTH;
        if (touchX >= tabX && touchX < tabX + TAB_WIDTH) {
            return i;
        }
    }

    return -1;
}

// ============================================
// WiFi & Web Server Functions
// ============================================
void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", WEB_UI_HTML);
    });

    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"cardSize\":" + String(SD.cardSize() / (1024 * 1024)) +
                      ",\"freeSpace\":" + String(getSDCardFreeSpace() / (1024 * 1024)) + "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request) {
        String dir = "/books";
        if (request->hasParam("dir")) {
            dir = request->getParam("dir")->value();
        }

        String json = "[";
        File root = SD.open(dir);
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            bool first = true;
            while (file) {
                if (!first) json += ",";
                if (file.isDirectory()) {
                    json += "{\"name\":\"" + String(file.name()) + "\",\"size\":-1,\"isDir\":true}";
                } else {
                    json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + ",\"isDir\":false}";
                }
                first = false;
                file = root.openNextFile();
            }
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    server.on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (request->hasParam("path")) {
            String path = request->getParam("path")->value();
            if (SD.remove(path)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed");
            }
        } else {
            request->send(400, "text/plain", "Missing path");
        }
    });

    server.on("/api/mkdir", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("path")) {
            String path = request->getParam("path")->value();
            if (SD.mkdir(path)) {
                request->send(200, "text/plain", "OK");
            } else if (SD.exists(path)) {
                request->send(200, "text/plain", "Already exists");
            } else {
                request->send(500, "text/plain", "Failed");
            }
        } else {
            request->send(400, "text/plain", "Missing path");
        }
    });

    server.on("/api/rmdir", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (request->hasParam("path")) {
            String path = request->getParam("path")->value();
            if (SD.rmdir(path)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed (folder not empty?)");
            }
        } else {
            request->send(400, "text/plain", "Missing path");
        }
    });

    server.on("/api/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "OK");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0) {
                String dir = "/books";
                if (request->hasParam("dir")) {
                    dir = request->getParam("dir")->value();
                }
                currentUploadPath = dir + "/" + filename;
                uploadFile = SD.open(currentUploadPath, FILE_WRITE);
            }

            if (uploadFile && len) {
                uploadFile.write(data, len);
            }

            if (final && uploadFile) {
                uploadFile.close();
            }
        }
    );

    server.on("/api/exit", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Restarting...");
        delay(1000);
        ESP.restart();
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"apSsid\":\"" + config.apSsid + "\",";
        json += "\"apPass\":\"" + config.apPassword + "\",";
        json += "\"staSsid\":\"" + config.staSsid + "\",";
        json += "\"staPass\":\"" + config.staPassword + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    server.on("/api/config/ap", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                config.apSsid = doc["ssid"] | config.apSsid;
                config.apPassword = doc["password"] | config.apPassword;
                saveConfig();
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid JSON");
            }
        }
    );

    server.on("/api/config/sta", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            JsonDocument doc;
            if (!deserializeJson(doc, body)) {
                config.staSsid = doc["ssid"] | config.staSsid;
                config.staPassword = doc["password"] | config.staPassword;
                saveConfig();
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Invalid JSON");
            }
        }
    );

    server.begin();
}

void startWiFiMode() {
    wifiMode = true;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.apSsid.c_str(), config.apPassword.c_str());

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    setupWebServer();

    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);

    int y = PAD_Y;

    // Back button header
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("< 뒤로가기");
    M5.Display.drawLine(PAD_X, y + 35, SCREEN_WIDTH - PAD_X, y + 35, TFT_BLACK);
    y += 50;

    // Title
    M5.Display.setCursor(PAD_X, y);
    M5.Display.println("WiFi 파일 전송 모드");
    y += 60;

    M5.Display.setCursor(PAD_X, y);
    M5.Display.println("1. WiFi 연결:");
    y += 40;

    M5.Display.setCursor(PAD_X + 40, y);
    M5.Display.printf("SSID: %s", config.apSsid.c_str());
    y += 35;
    M5.Display.setCursor(PAD_X + 40, y);
    M5.Display.printf("비밀번호: %s", config.apPassword.c_str());
    y += 50;

    M5.Display.setCursor(PAD_X, y);
    M5.Display.println("2. 브라우저 열기:");
    y += 40;

    M5.Display.setCursor(PAD_X + 40, y);
    M5.Display.printf("http://%s", IP.toString().c_str());
    y += 80;

    // Exit button at bottom
    int btnW = 300;
    int btnH = 60;
    int btnX = (SCREEN_WIDTH - btnW) / 2;
    int btnY = CONTENT_HEIGHT - btnH - 40;

    M5.Display.fillRect(btnX, btnY, btnW, btnH, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    String exitText = "종료하기";
    int textW = M5.Display.textWidth(exitText.c_str());
    M5.Display.setCursor(btnX + (btnW - textW) / 2, btnY + 18);
    M5.Display.print(exitText);

    M5.Display.setTextColor(TFT_BLACK);

    M5.Display.display();
}

void stopWiFiMode() {
    wifiMode = false;
    server.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi mode stopped");
}

// ============================================
// Display Refresh
// ============================================
int refreshCount = 0;
const int FULL_CLEAR_INTERVAL = 3;  // More frequent full clear to reduce ghosting

void refreshDisplay() {
    if (needsFullRedraw || ScreenManager::instance().needsRedraw()) {
        refreshCount++;

        // Force full clear on first refresh to remove old ghosting
        static bool firstRefresh = true;
        if (firstRefresh) {
            refreshCount = FULL_CLEAR_INTERVAL;
            firstRefresh = false;
        }

        // Full clear cycle only periodically (reduces ghosting)
        if (refreshCount >= FULL_CLEAR_INTERVAL) {
            // Use quality mode for full clear
            M5.Display.setEpdMode(epd_mode_t::epd_quality);
            M5.Display.fillScreen(TFT_BLACK);
            M5.Display.display();
            M5.Display.waitDisplay();
            delay(50);

            M5.Display.fillScreen(TFT_WHITE);
            M5.Display.display();
            M5.Display.waitDisplay();
            refreshCount = 0;
        }

        // Use fast mode for normal updates (less flicker)
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
        M5.Display.fillScreen(TFT_WHITE);
        ScreenManager::instance().draw();
        drawTabBar();
        M5.Display.display();
        M5.Display.waitDisplay();
        needsFullRedraw = false;
        needsTabRedraw = false;
    } else if (needsTabRedraw) {
        // Tab-only update - fastest mode
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        drawTabBar();
        M5.Display.display();
        M5.Display.waitDisplay();
        needsTabRedraw = false;
    }
}

// ============================================
// Setup
// ============================================
// Global font buffer - allocated before M5.begin() to avoid fragmentation
static uint8_t* g_fontBuffer = nullptr;
static size_t g_fontBufferSize = 0;

void setup() {
    // === CRITICAL: Reserve PSRAM for font BEFORE M5.begin() fragments it ===
    // Allocate 6MB buffer for font (will be used by FontManager later)
    g_fontBufferSize = 6 * 1024 * 1024;  // 6MB
    g_fontBuffer = (uint8_t*)heap_caps_malloc(g_fontBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    auto cfg = M5.config();
    cfg.clear_display = false;  // We handle clear in refreshDisplay()
    M5.begin(cfg);

    // Wait for USB CDC Serial (ESP32-S3)
    delay(1000);
    Serial.begin(115200);
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 3000)) {
        delay(100);
    }

    Serial.println("\n\n=== Papers3 JP Learner Starting ===");
    Serial.printf("Pre-allocated font buffer: %p (%d bytes)\n", g_fontBuffer, g_fontBufferSize);
    Serial.printf("PSRAM after M5.begin: Total=%d, Free=%d, Largest=%d\n",
                  ESP.getPsramSize(), ESP.getFreePsram(),
                  heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    Serial.flush();

    // Configure display
    M5.Display.setRotation(1);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);

    // Initialize LittleFS
    if (!LittleFS.begin(false)) {
        if (LittleFS.format()) {
            LittleFS.begin(false);
        }
    }

    // Load configuration
    loadConfig();

    // Initialize SD Card
    yield();
    sdCardMounted = initSDCard();

    if (!sdCardMounted) {
        showSDCardError("SD card not detected.");
        return;
    }

    // Initialize FontManager with pre-allocated buffer
    FontManager& fm = FontManager::instance();
    if (g_fontBuffer != nullptr) {
        fm.setExternalBuffer(g_fontBuffer, g_fontBufferSize);
    }
    fm.init();

    // Load configured font
    if (config.primaryFont.length() > 0) {
        Serial.printf("Loading font: %s\n", config.primaryFont.c_str());
        Serial.flush();
        bool loaded = fm.setPrimaryFont(config.primaryFont);
        Serial.printf("Font load result: %d, hasCustomFont: %d\n", loaded, fm.hasCustomFont());
        Serial.flush();
    }
    if (config.fallbackFont.length() > 0) {
        fm.setFallbackFont(config.fallbackFont);
    }

    yield();
    if (!ensureDirectories()) {
        showSDCardError("Failed to create directories.");
        return;
    }

    // Initialize battery
    analogReadResolution(12);
    updateBattery();

    // ============================================
    // Register Screens with ScreenManager
    // ============================================
    ScreenManager& sm = ScreenManager::instance();

    sm.registerScreen(TAB_WORD, &wordScreen);
    sm.registerScreen(TAB_GRAMMAR, &grammarScreen);
    sm.registerScreen(TAB_COPY, &copyScreen);
    sm.registerScreen(TAB_READ, &readScreen);
    sm.registerScreen(TAB_STATS, &statsScreen);
    sm.registerScreen(TAB_SETTINGS, &settingsScreen);

    // Set WiFi callback for settings screen
    settingsScreen.setWiFiModeCallback(startWiFiMode);

    // Switch to initial tab (calls onEnter which loads content)
    sm.switchTo(TAB_COPY);

    // Initialize sleep manager
    SleepManager::instance().init();

    // Draw initial screen
    needsFullRedraw = true;
    refreshDisplay();
    Serial.println("Setup complete!");
}

// ============================================
// Sleep Timeout Update (called from Settings)
// ============================================
void updateSleepTimeout() {
    SleepManager::instance().setSleepMinutes(config.sleepMinutes);
    Serial.printf("Sleep timeout updated: %d minutes\n", config.sleepMinutes);
}

// ============================================
// Loop
// ============================================
void loop() {
    M5.update();

    if (!sdCardMounted) {
        delay(1000);
        return;
    }

    SleepManager& sleepMgr = SleepManager::instance();

    // Check sleep timeout (only if not in WiFi mode)
    if (!wifiMode) {
        sleepMgr.update();

        // If we just woke up from sleep, trigger full redraw and skip this loop
        if (sleepMgr.justWokeUp()) {
            needsFullRedraw = true;
            refreshCount = FULL_CLEAR_INTERVAL;
            sleepMgr.clearWakeFlag();
            // Force immediate redraw
            refreshDisplay();
            return;
        }
    }

    updateBattery();

    auto touch = M5.Touch.getDetail();

    // WiFi mode handling
    if (wifiMode) {
        if (touch.wasPressed()) {
            sleepMgr.resetActivity();
            delay(300);
            stopWiFiMode();
            copyScreen.loadTodayContent();
            needsFullRedraw = true;
        }
        delay(50);
        return;
    }

    ScreenManager& sm = ScreenManager::instance();

    // Touch handling
    if (touch.wasPressed()) {
        sleepMgr.resetActivity();  // Reset sleep timer on touch

        int touchY = touch.y;
        int tabY = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

        if (touchY < tabY) {
            // Content area touch
            if (sm.handleTouchStart(touch.x, touch.y)) {
                needsFullRedraw = true;
            }
        } else {
            // Tab bar touch
            int tabTouched = handleTabTouch(touch.x, touch.y);
            if (tabTouched >= 0) {
                sm.switchTo((TabIndex)tabTouched);
                needsFullRedraw = true;
                // Force full clear on tab switch to eliminate ghosting
                refreshCount = FULL_CLEAR_INTERVAL;
            }
        }
    } else if (touch.isPressed()) {
        sleepMgr.resetActivity();  // Reset sleep timer on touch
        // Dragging
        if (sm.handleTouchMove(touch.x, touch.y)) {
            needsFullRedraw = true;
            refreshDisplay();
        }
    } else if (touch.wasReleased()) {
        sleepMgr.resetActivity();  // Reset sleep timer on touch
        // End drag
        if (sm.handleTouchEnd()) {
            needsFullRedraw = true;
        }
    }

    refreshDisplay();
    delay(10);
}
