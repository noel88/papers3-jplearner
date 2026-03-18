#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ============================================
// Hardware Configuration
// ============================================
AsyncWebServer server(80);

// Use M5.Display for all display operations
#define display M5.Display

// M5Paper S3 SD Card pins (correct pinout)
#define SD_CS   47
#define SD_SCK  39
#define SD_MISO 40
#define SD_MOSI 38

// ============================================
// Constants
// ============================================
const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 540;
const int TAB_BAR_HEIGHT = 60;
const int CONTENT_HEIGHT = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
const int CONTENT_Y = 0;  // Content starts at top

const int PAD_X = 30;
const int PAD_Y = 20;
const int LINE_SPACING = 16;

// Tab configuration
const int TAB_COUNT = 6;
const int BATTERY_WIDTH = 80;
const int TAB_WIDTH = (SCREEN_WIDTH - BATTERY_WIDTH) / TAB_COUNT;  // ~146px each

// Tab indices
enum TabIndex {
    TAB_WORD = 0,      // 단어
    TAB_GRAMMAR = 1,   // 문형
    TAB_COPY = 2,      // 필사
    TAB_READ = 3,      // 읽기
    TAB_STATS = 4,     // 통계
    TAB_SETTINGS = 5   // 설정
};

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
// Configuration Structure
// ============================================
struct Config {
    // Language
    String language = "ko";

    // Learning settings
    int newCardsPerDay = 20;
    int reviewLimit = -1;  // -1 = unlimited

    // Display settings
    String fontSize = "medium";  // small, medium, large
    String startScreen = "copy"; // word, grammar, copy, read

    // System settings
    int sleepMinutes = 5;

    // WiFi AP settings (for file transfer)
    String apSsid = "Papers3-JP";
    String apPassword = "12345678";

    // WiFi Station settings (for external connection)
    String staSsid = "";
    String staPassword = "";
    bool autoConnect = false;
} config;

// Settings menu state
enum SettingsMenuState {
    SETTINGS_MAIN,
    SETTINGS_WIFI_AP,
    SETTINGS_WIFI_STA,
    SETTINGS_DISPLAY,
    SETTINGS_LEARNING,
    SETTINGS_SYSTEM
};

SettingsMenuState settingsState = SETTINGS_MAIN;

// ============================================
// State
// ============================================
bool sdCardMounted = false;
bool wifiMode = false;
TabIndex currentTab = TAB_COPY;  // Start with 필사 (transcription)
bool needsFullRedraw = true;
bool needsTabRedraw = false;

// 필사 (Copy/Transcription) state
std::vector<String> sentences;
String todayTitle = "";
String todayAuthor = "";
String todayDate = "";
int currentPage = -1;

// WiFi state
String currentUploadPath = "";
File uploadFile;

// Battery state
int batteryPercent = 100;
unsigned long lastBatteryCheck = 0;
const unsigned long BATTERY_CHECK_INTERVAL = 60000;  // Check every minute

// ============================================
// HTML Page
// ============================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Papers3 JP Learner - File Manager</title>
    <style>
        * { box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: #f5f5f5;
        }
        h1 { color: #333; text-align: center; }
        .card {
            background: white;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 20px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .card h2 { margin-top: 0; color: #555; }
        select, input[type="file"] {
            width: 100%;
            padding: 10px;
            margin: 10px 0;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        button {
            background: #4CAF50;
            color: white;
            padding: 12px 24px;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
        }
        button:hover { background: #45a049; }
        button:disabled { background: #ccc; cursor: not-allowed; }
        .progress {
            width: 100%;
            height: 20px;
            background: #e0e0e0;
            border-radius: 10px;
            overflow: hidden;
            margin: 10px 0;
            display: none;
        }
        .progress-bar {
            height: 100%;
            background: #4CAF50;
            width: 0%;
            transition: width 0.3s;
        }
        .status {
            text-align: center;
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
        }
        .success { background: #d4edda; color: #155724; }
        .error { background: #f8d7da; color: #721c24; }
        .file-list {
            max-height: 300px;
            overflow-y: auto;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        .file-item {
            padding: 10px;
            border-bottom: 1px solid #eee;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .file-item:last-child { border-bottom: none; }
        .file-name { font-weight: 500; }
        .file-size { color: #888; font-size: 14px; }
        .btn-delete {
            background: #dc3545;
            padding: 5px 10px;
            font-size: 12px;
            width: auto;
        }
        .info { background: #e7f3ff; padding: 15px; border-radius: 4px; margin-bottom: 20px; }
        .info p { margin: 5px 0; }
    </style>
</head>
<body>
    <h1>Papers3 JP Learner</h1>

    <div class="info">
        <p><strong>SD Card:</strong> <span id="sdInfo">Loading...</span></p>
        <p><strong>Free Space:</strong> <span id="freeSpace">Loading...</span></p>
    </div>

    <div class="card">
        <h2>Upload File</h2>
        <select id="targetDir">
            <option value="/books">books (epub, txt)</option>
            <option value="/dict">dict (dictionary)</option>
            <option value="/fonts">fonts</option>
            <option value="/userdata">userdata</option>
        </select>
        <input type="file" id="fileInput" multiple>
        <div class="progress" id="progress">
            <div class="progress-bar" id="progressBar"></div>
        </div>
        <div id="status"></div>
        <button onclick="uploadFiles()" id="uploadBtn">Upload</button>
    </div>

    <div class="card">
        <h2>Files</h2>
        <select id="browseDir" onchange="loadFiles()">
            <option value="/books">books</option>
            <option value="/dict">dict</option>
            <option value="/fonts">fonts</option>
            <option value="/userdata">userdata</option>
        </select>
        <div class="file-list" id="fileList">
            <div class="file-item">Loading...</div>
        </div>
    </div>

    <div class="card">
        <h2>WiFi Settings</h2>
        <div style="margin-bottom: 15px;">
            <label><strong>AP Mode (File Transfer)</strong></label>
            <input type="text" id="apSsid" placeholder="AP SSID" style="width:100%;padding:8px;margin:5px 0;">
            <input type="text" id="apPass" placeholder="AP Password" style="width:100%;padding:8px;margin:5px 0;">
            <button onclick="saveApSettings()" style="background:#17a2b8;">Save AP Settings</button>
        </div>
        <div style="margin-top:20px;">
            <label><strong>External WiFi (for future sync)</strong></label>
            <input type="text" id="staSsid" placeholder="WiFi SSID" style="width:100%;padding:8px;margin:5px 0;">
            <input type="password" id="staPass" placeholder="WiFi Password" style="width:100%;padding:8px;margin:5px 0;">
            <button onclick="saveStaSettings()" style="background:#17a2b8;">Save WiFi Settings</button>
        </div>
        <div id="wifiStatus"></div>
    </div>

    <div class="card" style="text-align: center;">
        <button onclick="exitWifi()" style="background: #6c757d;">Exit WiFi Mode</button>
    </div>

    <script>
        async function loadInfo() {
            try {
                const res = await fetch('/api/info');
                const data = await res.json();
                document.getElementById('sdInfo').textContent = data.cardSize + ' MB';
                document.getElementById('freeSpace').textContent = data.freeSpace + ' MB';
            } catch(e) {
                document.getElementById('sdInfo').textContent = 'Error';
            }
        }

        async function loadFiles() {
            const dir = document.getElementById('browseDir').value;
            const list = document.getElementById('fileList');
            try {
                const res = await fetch('/api/files?dir=' + encodeURIComponent(dir));
                const files = await res.json();
                if (files.length === 0) {
                    list.innerHTML = '<div class="file-item">No files</div>';
                } else {
                    list.innerHTML = files.map(f => `
                        <div class="file-item">
                            <div>
                                <span class="file-name">${f.name}</span><br>
                                <span class="file-size">${formatSize(f.size)}</span>
                            </div>
                            <button class="btn-delete" onclick="deleteFile('${dir}/${f.name}')">Delete</button>
                        </div>
                    `).join('');
                }
            } catch(e) {
                list.innerHTML = '<div class="file-item">Error loading files</div>';
            }
        }

        function formatSize(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + ' KB';
            return (bytes/1024/1024).toFixed(1) + ' MB';
        }

        async function uploadFiles() {
            const input = document.getElementById('fileInput');
            const dir = document.getElementById('targetDir').value;
            const btn = document.getElementById('uploadBtn');
            const progress = document.getElementById('progress');
            const progressBar = document.getElementById('progressBar');
            const status = document.getElementById('status');

            if (!input.files.length) {
                status.innerHTML = '<div class="status error">Please select files</div>';
                return;
            }

            btn.disabled = true;
            progress.style.display = 'block';
            status.innerHTML = '';

            for (let i = 0; i < input.files.length; i++) {
                const file = input.files[i];
                const formData = new FormData();
                formData.append('file', file);
                formData.append('dir', dir);

                try {
                    const xhr = new XMLHttpRequest();
                    xhr.open('POST', '/api/upload', true);

                    xhr.upload.onprogress = (e) => {
                        if (e.lengthComputable) {
                            const pct = (e.loaded / e.total) * 100;
                            progressBar.style.width = pct + '%';
                        }
                    };

                    await new Promise((resolve, reject) => {
                        xhr.onload = () => {
                            if (xhr.status === 200) resolve();
                            else reject(new Error(xhr.statusText));
                        };
                        xhr.onerror = () => reject(new Error('Network error'));
                        xhr.send(formData);
                    });

                    status.innerHTML = `<div class="status success">${file.name} uploaded!</div>`;
                } catch(e) {
                    status.innerHTML = `<div class="status error">Error: ${e.message}</div>`;
                }
            }

            btn.disabled = false;
            progress.style.display = 'none';
            progressBar.style.width = '0%';
            input.value = '';
            loadFiles();
            loadInfo();
        }

        async function deleteFile(path) {
            if (!confirm('Delete ' + path + '?')) return;
            try {
                await fetch('/api/delete?path=' + encodeURIComponent(path), {method: 'DELETE'});
                loadFiles();
                loadInfo();
            } catch(e) {
                alert('Error deleting file');
            }
        }

        async function exitWifi() {
            if (!confirm('Exit WiFi mode and restart?')) return;
            try {
                await fetch('/api/exit');
            } catch(e) {}
            document.body.innerHTML = '<h1 style="text-align:center;margin-top:100px;">Restarting...</h1>';
        }

        async function loadConfig() {
            try {
                const res = await fetch('/api/config');
                const cfg = await res.json();
                document.getElementById('apSsid').value = cfg.apSsid || '';
                document.getElementById('apPass').value = cfg.apPass || '';
                document.getElementById('staSsid').value = cfg.staSsid || '';
                document.getElementById('staPass').value = cfg.staPass || '';
            } catch(e) {
                console.error('Failed to load config');
            }
        }

        async function saveApSettings() {
            const ssid = document.getElementById('apSsid').value;
            const pass = document.getElementById('apPass').value;
            const status = document.getElementById('wifiStatus');

            if (ssid.length < 1 || pass.length < 8) {
                status.innerHTML = '<div class="status error">SSID required, password min 8 chars</div>';
                return;
            }

            try {
                const res = await fetch('/api/config/ap', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password: pass})
                });
                if (res.ok) {
                    status.innerHTML = '<div class="status success">AP settings saved! Restart to apply.</div>';
                } else {
                    status.innerHTML = '<div class="status error">Failed to save</div>';
                }
            } catch(e) {
                status.innerHTML = '<div class="status error">Error: ' + e.message + '</div>';
            }
        }

        async function saveStaSettings() {
            const ssid = document.getElementById('staSsid').value;
            const pass = document.getElementById('staPass').value;
            const status = document.getElementById('wifiStatus');

            try {
                const res = await fetch('/api/config/sta', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({ssid, password: pass})
                });
                if (res.ok) {
                    status.innerHTML = '<div class="status success">WiFi settings saved!</div>';
                } else {
                    status.innerHTML = '<div class="status error">Failed to save</div>';
                }
            } catch(e) {
                status.innerHTML = '<div class="status error">Error: ' + e.message + '</div>';
            }
        }

        loadInfo();
        loadFiles();
        loadConfig();
    </script>
</body>
</html>
)rawliteral";

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
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK);

    int centerY = CONTENT_HEIGHT / 2;

    display.setCursor(PAD_X, centerY - 40);
    display.setTextSize(1.0);
    display.println("SD Card Error");

    display.setCursor(PAD_X, centerY + 20);
    display.setTextSize(1.0);
    display.println(message);

    display.setCursor(PAD_X, centerY + 60);
    display.println("Please insert SD card and restart.");

    display.display();
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

    // Language
    config.language = doc["language"] | "ko";

    // Learning
    config.newCardsPerDay = doc["learning"]["newCardsPerDay"] | 20;
    config.reviewLimit = doc["learning"]["reviewLimit"] | -1;

    // Display
    config.fontSize = doc["display"]["fontSize"] | "medium";
    config.startScreen = doc["display"]["startScreen"] | "copy";

    // System
    config.sleepMinutes = doc["system"]["sleepMinutes"] | 5;

    // WiFi AP
    config.apSsid = doc["wifi"]["ap"]["ssid"] | "Papers3-JP";
    config.apPassword = doc["wifi"]["ap"]["password"] | "12345678";

    // WiFi Station
    config.staSsid = doc["wifi"]["station"]["ssid"] | "";
    config.staPassword = doc["wifi"]["station"]["password"] | "";
    config.autoConnect = doc["wifi"]["station"]["autoConnect"] | false;

    Serial.println("Config loaded successfully");
    return true;
}

bool saveConfig() {
    JsonDocument doc;

    // Language
    doc["language"] = config.language;

    // Learning
    doc["learning"]["newCardsPerDay"] = config.newCardsPerDay;
    doc["learning"]["reviewLimit"] = config.reviewLimit;

    // Display
    doc["display"]["fontSize"] = config.fontSize;
    doc["display"]["startScreen"] = config.startScreen;

    // System
    doc["system"]["sleepMinutes"] = config.sleepMinutes;

    // WiFi AP
    doc["wifi"]["ap"]["ssid"] = config.apSsid;
    doc["wifi"]["ap"]["password"] = config.apPassword;

    // WiFi Station
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
    // M5Paper S3 battery voltage reading
    // Battery pin is typically GPIO10 on M5Paper S3
    // Full charge ~4.2V, Empty ~3.0V
    // Using voltage divider, ADC reads half of actual voltage

    int adcValue = analogRead(10);
    float voltage = (adcValue / 4095.0) * 3.3 * 2;  // Voltage divider factor

    // Convert voltage to percentage (3.0V = 0%, 4.2V = 100%)
    int percent = (int)((voltage - 3.0) / (4.2 - 3.0) * 100);
    percent = constrain(percent, 0, 100);

    return percent;
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

    // Draw tab bar background
    display.fillRect(0, tabY, SCREEN_WIDTH, TAB_BAR_HEIGHT, TFT_WHITE);

    // Draw top border line
    display.drawLine(0, tabY, SCREEN_WIDTH, tabY, TFT_BLACK);

    // Draw each tab
    display.setFont(&fonts::efontKR_24);
    display.setTextSize(0.8);  // Scale down for tab bar

    for (int i = 0; i < TAB_COUNT; i++) {
        int tabX = i * TAB_WIDTH;

        // Highlight current tab
        if (i == currentTab) {
            display.fillRect(tabX + 2, tabY + 2, TAB_WIDTH - 4, TAB_BAR_HEIGHT - 4, TFT_BLACK);
            display.setTextColor(TFT_WHITE);
        } else {
            display.setTextColor(TFT_BLACK);
        }

        // Draw tab label centered
        int labelWidth = display.textWidth(TAB_LABELS[i]);
        int labelX = tabX + (TAB_WIDTH - labelWidth) / 2;
        int labelY = tabY + (TAB_BAR_HEIGHT - display.fontHeight()) / 2;

        display.setCursor(labelX, labelY);
        display.print(TAB_LABELS[i]);

        // Draw vertical separator (except for last tab)
        if (i < TAB_COUNT - 1) {
            display.drawLine(tabX + TAB_WIDTH, tabY + 8, tabX + TAB_WIDTH, tabY + TAB_BAR_HEIGHT - 8, 0x7BEF);  // Gray color
        }
    }

    // Reset text color
    display.setTextColor(TFT_BLACK);

    // Draw battery indicator
    int battX = TAB_COUNT * TAB_WIDTH + 10;
    int battY = tabY + (TAB_BAR_HEIGHT - 24) / 2;

    // Battery icon outline
    display.drawRect(battX, battY, 40, 24, TFT_BLACK);
    display.fillRect(battX + 40, battY + 6, 4, 12, TFT_BLACK);  // Battery tip

    // Battery fill level
    int fillWidth = (36 * batteryPercent) / 100;
    if (fillWidth > 0) {
        display.fillRect(battX + 2, battY + 2, fillWidth, 20, TFT_BLACK);
    }

    // Battery percentage text
    display.setFont(&fonts::Font2);
    char battText[8];
    sprintf(battText, "%d%%", batteryPercent);
    display.setCursor(battX + 48, battY + 4);
    display.print(battText);

    // Reset font
    display.setFont(&fonts::efontKR_24);
    display.setTextSize(1.0);
}

int handleTabTouch(int touchX, int touchY) {
    int tabY = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Check if touch is in tab bar area
    if (touchY < tabY) {
        return -1;  // Not in tab bar
    }

    // Check which tab was touched
    for (int i = 0; i < TAB_COUNT; i++) {
        int tabX = i * TAB_WIDTH;
        if (touchX >= tabX && touchX < tabX + TAB_WIDTH) {
            return i;
        }
    }

    return -1;  // Touch in battery area or outside tabs
}

void switchTab(TabIndex newTab) {
    if (newTab != currentTab) {
        // Reset settings menu state when leaving settings tab
        if (currentTab == TAB_SETTINGS) {
            settingsState = SETTINGS_MAIN;
        }

        currentTab = newTab;
        needsFullRedraw = true;
        Serial.printf("Switched to tab: %s\n", TAB_LABELS[newTab]);
    }
}

// ============================================
// WiFi & Web Server Functions
// ============================================
void setupWebServer() {
    // Serve main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", HTML_PAGE);
    });

    // API: Get SD card info
    server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{\"cardSize\":" + String(SD.cardSize() / (1024 * 1024)) +
                      ",\"freeSpace\":" + String(getSDCardFreeSpace() / (1024 * 1024)) + "}";
        request->send(200, "application/json", json);
    });

    // API: List files in directory
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
                if (!file.isDirectory()) {
                    if (!first) json += ",";
                    json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
                    first = false;
                }
                file = root.openNextFile();
            }
        }
        json += "]";
        request->send(200, "application/json", json);
    });

    // API: Delete file
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

    // API: Upload file
    server.on("/api/upload", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "OK");
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (index == 0) {
                String dir = "/books";
                if (request->hasParam("dir", true)) {
                    dir = request->getParam("dir", true)->value();
                }
                currentUploadPath = dir + "/" + filename;
                Serial.printf("Upload Start: %s\n", currentUploadPath.c_str());
                uploadFile = SD.open(currentUploadPath, FILE_WRITE);
            }

            if (uploadFile && len) {
                uploadFile.write(data, len);
            }

            if (final) {
                if (uploadFile) {
                    uploadFile.close();
                    Serial.printf("Upload Complete: %s (%u bytes)\n", currentUploadPath.c_str(), index + len);
                }
            }
        }
    );

    // API: Exit WiFi mode
    server.on("/api/exit", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Restarting...");
        delay(1000);
        ESP.restart();
    });

    // API: Get config
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"apSsid\":\"" + config.apSsid + "\",";
        json += "\"apPass\":\"" + config.apPassword + "\",";
        json += "\"staSsid\":\"" + config.staSsid + "\",";
        json += "\"staPass\":\"" + config.staPassword + "\"";
        json += "}";
        request->send(200, "application/json", json);
    });

    // API: Save AP config
    server.on("/api/config/ap", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            config.apSsid = doc["ssid"] | config.apSsid;
            config.apPassword = doc["password"] | config.apPassword;

            if (saveConfig()) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed to save");
            }
        }
    );

    // API: Save Station config
    server.on("/api/config/sta", HTTP_POST, [](AsyncWebServerRequest *request) {},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            config.staSsid = doc["ssid"] | config.staSsid;
            config.staPassword = doc["password"] | config.staPassword;

            if (saveConfig()) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed to save");
            }
        }
    );

    server.begin();
    Serial.println("Web server started");
}

void startWiFiMode() {
    wifiMode = true;

    // Start AP mode using config values
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.apSsid.c_str(), config.apPassword.c_str());

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    setupWebServer();

    // Display WiFi info
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(1.0);

    int y = PAD_Y;
    display.setCursor(PAD_X, y);
    display.println("WiFi File Transfer Mode");
    y += 60;

    display.setTextSize(1.0);
    display.setCursor(PAD_X, y);
    display.println("1. Connect to WiFi:");
    y += 40;

    display.setTextSize(1.0);
    display.setCursor(PAD_X + 40, y);
    display.printf("SSID: %s", config.apSsid.c_str());
    y += 35;
    display.setCursor(PAD_X + 40, y);
    display.printf("Pass: %s", config.apPassword.c_str());
    y += 50;

    display.setTextSize(1.0);
    display.setCursor(PAD_X, y);
    display.println("2. Open browser:");
    y += 40;

    display.setTextSize(1.0);
    display.setCursor(PAD_X + 40, y);
    display.printf("http://%s", IP.toString().c_str());
    y += 60;

    display.setTextSize(1.0);
    display.setCursor(PAD_X, y);
    display.println("Touch screen to exit WiFi mode");

    display.display();
}

void stopWiFiMode() {
    wifiMode = false;
    server.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi mode stopped");
}

// ============================================
// Content Loading (Temporary - will be replaced with epub)
// ============================================
bool loadTodaySentences() {
    // Get date from M5Unified RTC
    auto dt = M5.Rtc.getDateTime();

    char target[8];
    sprintf(target, "#%02d%02d", dt.date.month, dt.date.date);
    todayDate = String(dt.date.month) + "月" + String(dt.date.date) + "日";

    // Try SD card first, then LittleFS as fallback
    File f;
    String filePath = String(DIR_BOOKS) + "/365.txt";

    if (SD.exists(filePath.c_str())) {
        f = SD.open(filePath.c_str(), FILE_READ);
    } else if (LittleFS.exists("/365.txt")) {
        f = LittleFS.open("/365.txt", "r");
    }

    if (!f) return false;

    bool found = false;
    sentences.clear();

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.startsWith("#")) {
            if (found) break;
            if (line == String(target)) found = true;
            continue;
        }
        if (!found) continue;
        if (line.startsWith("TITLE:")) {
            todayTitle = line.substring(6);
            todayTitle.trim();
        } else if (line.startsWith("AUTHOR:")) {
            todayAuthor = line.substring(7);
            todayAuthor.trim();
        } else if (line.length() > 0) {
            sentences.push_back(line);
        }
    }
    f.close();
    return sentences.size() > 0;
}

// ============================================
// Tab Content Drawing Functions
// ============================================
void drawPlaceholderContent(const char* tabName, const char* description) {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);

    display.setFont(&fonts::efontKR_24);
    display.setTextSize(1.0);
    display.setTextColor(TFT_BLACK);

    // Title - centered
    int titleWidth = display.textWidth(tabName);
    display.setCursor((SCREEN_WIDTH - titleWidth) / 2, CONTENT_HEIGHT / 2 - 40);
    display.print(tabName);

    // Description - centered
    int descWidth = display.textWidth(description);
    display.setCursor((SCREEN_WIDTH - descWidth) / 2, CONTENT_HEIGHT / 2 + 20);
    display.print(description);
}

void drawWordTab() {
    drawPlaceholderContent("단어 학습", "Coming soon - SRS 단어 학습");
}

void drawGrammarTab() {
    drawPlaceholderContent("문형 학습", "Coming soon - N2/N1 문법 패턴");
}

void drawStatsTab() {
    drawPlaceholderContent("학습 통계", "Coming soon - 학습 진행 현황");
}

// Settings menu item structure
struct SettingsMenuItem {
    const char* label;
    const char* value;
    int y;
    int height;
};

const int SETTINGS_ITEM_HEIGHT = 55;
const int SETTINGS_ITEMS_START_Y = 60;

void drawSettingsMainMenu() {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
    display.setFont(&fonts::efontKR_24);
    display.setTextColor(TFT_BLACK);

    // Title
    display.setTextSize(1.0);
    display.setCursor(PAD_X, PAD_Y);
    display.print("설정");

    // Divider
    display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    display.setTextSize(1.0);
    int y = SETTINGS_ITEMS_START_Y;

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
            display.fillRect(0, y, SCREEN_WIDTH, SETTINGS_ITEM_HEIGHT - 1, 0xF7BE);  // Light gray
        }

        // Draw item label
        display.setCursor(PAD_X, y + 15);
        display.print(menuItems[i]);

        // Draw arrow
        display.setCursor(SCREEN_WIDTH - 60, y + 15);
        display.print(">");

        // Draw bottom border
        display.drawLine(PAD_X, y + SETTINGS_ITEM_HEIGHT - 1, SCREEN_WIDTH - PAD_X, y + SETTINGS_ITEM_HEIGHT - 1, 0xDEDB);

        y += SETTINGS_ITEM_HEIGHT;
    }

    // SD Card info at bottom
    display.setFont(&fonts::Font2);
    display.setCursor(PAD_X, CONTENT_HEIGHT - 30);
    display.printf("SD: %llu MB free / %llu MB total",
        getSDCardFreeSpace() / (1024 * 1024),
        SD.totalBytes() / (1024 * 1024));

    display.setFont(&fonts::efontKR_24);
}

void drawWiFiAPSettings() {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
    display.setFont(&fonts::efontKR_24);
    display.setTextColor(TFT_BLACK);

    // Title with back button
    display.setTextSize(1.0);
    display.setCursor(PAD_X, PAD_Y);
    display.print("< WiFi 파일 전송");
    display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    display.setTextSize(1.0);
    int y = SETTINGS_ITEMS_START_Y + 10;

    // Current AP settings
    display.setCursor(PAD_X, y);
    display.print("현재 설정:");
    y += 40;

    display.setCursor(PAD_X + 20, y);
    display.printf("SSID: %s", config.apSsid.c_str());
    y += 35;

    display.setCursor(PAD_X + 20, y);
    display.printf("비밀번호: %s", config.apPassword.c_str());
    y += 50;

    // Start button
    int btnY = y + 20;
    int btnW = 300;
    int btnH = 60;
    int btnX = (SCREEN_WIDTH - btnW) / 2;

    display.fillRect(btnX, btnY, btnW, btnH, TFT_BLACK);
    display.setTextColor(TFT_WHITE);
    display.setTextSize(1.0);

    int textW = display.textWidth("파일 전송 시작");
    display.setCursor(btnX + (btnW - textW) / 2, btnY + 18);
    display.print("파일 전송 시작");

    display.setTextColor(TFT_BLACK);
    display.setTextSize(1.0);

    // Instructions
    y = btnY + btnH + 30;
    display.setFont(&fonts::Font2);
    display.setCursor(PAD_X, y);
    display.print("1. Start file transfer");
    y += 25;
    display.setCursor(PAD_X, y);
    display.printf("2. Connect to WiFi '%s'", config.apSsid.c_str());
    y += 25;
    display.setCursor(PAD_X, y);
    display.print("3. Open http://192.168.4.1 in browser");

    display.setFont(&fonts::efontKR_24);
}

void drawWiFiSTASettings() {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
    display.setFont(&fonts::efontKR_24);
    display.setTextColor(TFT_BLACK);

    // Title with back button
    display.setTextSize(1.0);
    display.setCursor(PAD_X, PAD_Y);
    display.print("< WiFi 연결 설정");
    display.drawLine(PAD_X, 50, SCREEN_WIDTH - PAD_X, 50, TFT_BLACK);

    display.setTextSize(1.0);
    int y = SETTINGS_ITEMS_START_Y + 10;

    // Current Station settings
    display.setCursor(PAD_X, y);
    display.print("외부 WiFi 연결:");
    y += 40;

    if (config.staSsid.length() > 0) {
        display.setCursor(PAD_X + 20, y);
        display.printf("SSID: %s", config.staSsid.c_str());
        y += 35;

        display.setCursor(PAD_X + 20, y);
        display.print("상태: 설정됨");
    } else {
        display.setCursor(PAD_X + 20, y);
        display.print("설정된 네트워크 없음");
    }
    y += 50;

    // Scan button
    int btnY = y + 20;
    int btnW = 250;
    int btnH = 50;
    int btnX = (SCREEN_WIDTH - btnW) / 2;

    display.drawRect(btnX, btnY, btnW, btnH, TFT_BLACK);
    display.setTextSize(1.1);

    int textW = display.textWidth("WiFi 스캔");
    display.setCursor(btnX + (btnW - textW) / 2, btnY + 12);
    display.print("WiFi 스캔");

    display.setTextSize(1.0);

    // Note
    y = btnY + btnH + 40;
    display.setFont(&fonts::Font2);
    display.setCursor(PAD_X, y);
    display.print("Note: WiFi scan feature coming soon.");
    y += 25;
    display.setCursor(PAD_X, y);
    display.print("Currently use AP mode for file transfer.");

    display.setFont(&fonts::efontKR_24);
}

void drawSettingsTab() {
    switch (settingsState) {
        case SETTINGS_MAIN:
            drawSettingsMainMenu();
            break;
        case SETTINGS_WIFI_AP:
            drawWiFiAPSettings();
            break;
        case SETTINGS_WIFI_STA:
            drawWiFiSTASettings();
            break;
        default:
            drawSettingsMainMenu();
            break;
    }
}

// ============================================
// Display Functions
// ============================================
int printWrapped(const String& text, int x, int y, int maxW) {
    int fontH = display.fontHeight();
    int bytePos = 0;
    int byteLen = text.length();

    while (bytePos < byteLen) {
        int lineEnd = bytePos;
        int lineWidth = 0;

        while (lineEnd < byteLen) {
            uint8_t c = (uint8_t)text[lineEnd];
            int charBytes = 1;
            if (c >= 0xF0) charBytes = 4;
            else if (c >= 0xE0) charBytes = 3;
            else if (c >= 0xC0) charBytes = 2;

            String ch = text.substring(lineEnd, lineEnd + charBytes);
            int chW = display.textWidth(ch.c_str());

            if (lineWidth + chW > maxW) break;
            lineWidth += chW;
            lineEnd += charBytes;
        }

        if (lineEnd == bytePos) lineEnd += 1;

        display.setCursor(x, y);
        display.print(text.substring(bytePos, lineEnd));
        bytePos = lineEnd;

        if (bytePos < byteLen) {
            y += fontH + LINE_SPACING;
        }
    }
    return y + fontH;
}

void drawCopyTitlePage() {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
    int contentW = SCREEN_WIDTH - PAD_X * 2;
    int y = PAD_Y;

    // Japanese font for Japanese content
    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.0);
    display.setTextColor(TFT_BLACK);

    int fontH = display.fontHeight();

    display.setCursor(PAD_X, y);
    display.println(todayDate);  // 1月1日 format
    y += fontH + 16;

    display.drawLine(PAD_X, y, SCREEN_WIDTH - PAD_X, y, TFT_BLACK);
    y += 20;

    y = printWrapped(todayTitle, PAD_X, y, contentW);
    y += 16;

    display.setCursor(PAD_X, y);
    display.println(todayAuthor);

    int totalPages = sentences.size() + 1;
    display.setCursor(SCREEN_WIDTH - 120, CONTENT_HEIGHT - PAD_Y - 20);
    display.printf("1 / %d", totalPages);
}

void drawCopyContentPage(int idx) {
    display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);

    // Japanese font for Japanese content
    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.0);
    display.setTextColor(TFT_BLACK);

    int contentW = SCREEN_WIDTH - PAD_X * 2;
    printWrapped(sentences[idx], PAD_X, PAD_Y, contentW);

    int totalPages = sentences.size() + 1;
    char info[20];
    sprintf(info, "%d / %d", idx + 2, totalPages);
    display.setCursor(SCREEN_WIDTH - 120, CONTENT_HEIGHT - PAD_Y - 20);
    display.println(info);
}

void drawCopyTab() {
    if (sentences.size() == 0) {
        // No content loaded - Korean UI text
        display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
        display.setFont(&fonts::efontKR_24);
        display.setTextSize(1.0);
        display.setTextColor(TFT_BLACK);

        int centerY = CONTENT_HEIGHT / 2;

        display.setCursor(PAD_X, centerY - 40);
        display.println("오늘의 필사 내용이 없습니다");

        display.setCursor(PAD_X, centerY + 10);
        display.println("설정 탭에서 WiFi 파일 전송으로 콘텐츠를 업로드하세요");
    } else {
        if (currentPage == -1) {
            drawCopyTitlePage();
        } else {
            drawCopyContentPage(currentPage);
        }
    }
}

void drawReadTab() {
    drawPlaceholderContent("읽기", "Coming soon - epub 리더");
}

void drawCurrentTabContent() {
    switch (currentTab) {
        case TAB_WORD:
            drawWordTab();
            break;
        case TAB_GRAMMAR:
            drawGrammarTab();
            break;
        case TAB_COPY:
            drawCopyTab();
            break;
        case TAB_READ:
            drawReadTab();
            break;
        case TAB_STATS:
            drawStatsTab();
            break;
        case TAB_SETTINGS:
            drawSettingsTab();
            break;
    }
}

// Track refreshes for periodic full clear
int refreshCount = 0;
const int FULL_CLEAR_INTERVAL = 5;  // Full clear every 5 refreshes

void refreshDisplay() {
    if (needsFullRedraw) {
        // Periodic full clear to reduce ghosting
        refreshCount++;
        if (refreshCount >= FULL_CLEAR_INTERVAL) {
            display.clearDisplay();
            display.waitDisplay();
            refreshCount = 0;
        }

        display.fillScreen(TFT_WHITE);
        drawCurrentTabContent();
        drawTabBar();
        display.display();
        display.waitDisplay();  // Wait for e-ink refresh to complete
        needsFullRedraw = false;
        needsTabRedraw = false;
    } else if (needsTabRedraw) {
        drawTabBar();
        display.display();
        display.waitDisplay();
        needsTabRedraw = false;
    }
}

// ============================================
// Setup & Loop
// ============================================
void setup() {
    // Initialize M5Unified (handles display, touch, RTC, etc.)
    auto cfg = M5.config();
    cfg.clear_display = true;  // Clear display on start
    M5.begin(cfg);

    Serial.println("Papers3 JP Learner Starting...");

    // Configure display
    Serial.println("Configuring display...");
    display.setRotation(1);  // Landscape mode
    display.setEpdMode(epd_mode_t::epd_quality);

    // E-ink full clear cycle (removes ghosting and vertical lines)
    Serial.println("Clearing e-ink display...");
    display.fillScreen(TFT_BLACK);
    display.display();
    display.waitDisplay();

    display.fillScreen(TFT_WHITE);
    display.display();
    display.waitDisplay();

    display.setTextColor(TFT_BLACK);
    display.setFont(&fonts::efontKR_24);
    display.setTextSize(1.0);
    Serial.println("Display configured OK");

    // Initialize LittleFS (for config and default font)
    Serial.println("Initializing LittleFS...");
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS mount failed, formatting...");
        if (!LittleFS.format()) {
            Serial.println("LittleFS format failed!");
        } else {
            Serial.println("LittleFS formatted OK");
            if (!LittleFS.begin(false)) {
                Serial.println("LittleFS mount after format failed!");
            } else {
                Serial.println("LittleFS mounted after format");
            }
        }
    } else {
        Serial.println("LittleFS mounted OK");
    }

    // Test LittleFS write capability
    Serial.println("Testing LittleFS write...");
    File testFile = LittleFS.open("/test.txt", "w");
    if (testFile) {
        testFile.println("test");
        testFile.close();
        LittleFS.remove("/test.txt");
        Serial.println("LittleFS write test OK");
    } else {
        Serial.println("LittleFS write test FAILED!");
    }

    // Load configuration
    Serial.println("Loading config...");
    loadConfig();

    // Initialize SD Card
    Serial.println("Initializing SD Card...");
    yield();  // Feed watchdog
    sdCardMounted = initSDCard();

    if (!sdCardMounted) {
        Serial.println("SD card mount failed!");
        showSDCardError("SD card not detected.");
        return;
    }
    Serial.println("SD Card mounted OK");

    // Ensure directory structure
    Serial.println("Creating directories...");
    yield();  // Feed watchdog
    if (!ensureDirectories()) {
        showSDCardError("Failed to create directories.");
        return;
    }

    Serial.printf("SD Card Free Space: %llu MB\n", getSDCardFreeSpace() / (1024 * 1024));

    // Initialize battery reading
    Serial.println("Initializing battery...");
    analogReadResolution(12);
    updateBattery();

    // Load 필사 content
    Serial.println("Loading content...");
    yield();  // Feed watchdog
    loadTodaySentences();

    // Draw initial screen with tab bar
    Serial.println("Drawing initial screen...");
    needsFullRedraw = true;
    refreshDisplay();
    Serial.println("Setup complete!");
}

void handleCopyTabTouch(int touchX, int touchY) {
    // Page navigation for 필사 tab
    if (sentences.size() > 0) {
        if (currentPage == -1) {
            currentPage = 0;
        } else {
            currentPage++;
            if (currentPage >= (int)sentences.size()) {
                currentPage = -1;
            }
        }
        needsFullRedraw = true;
    }
}

void handleSettingsTabTouch(int touchX, int touchY) {
    switch (settingsState) {
        case SETTINGS_MAIN: {
            // Check which menu item was touched
            if (touchY >= SETTINGS_ITEMS_START_Y && touchY < SETTINGS_ITEMS_START_Y + 6 * SETTINGS_ITEM_HEIGHT) {
                int itemIndex = (touchY - SETTINGS_ITEMS_START_Y) / SETTINGS_ITEM_HEIGHT;
                Serial.printf("Settings item touched: %d\n", itemIndex);

                switch (itemIndex) {
                    case 0:  // WiFi 파일 전송
                        settingsState = SETTINGS_WIFI_AP;
                        needsFullRedraw = true;
                        break;
                    case 1:  // WiFi 연결 설정
                        settingsState = SETTINGS_WIFI_STA;
                        needsFullRedraw = true;
                        break;
                    case 2:  // 화면 설정
                        // TODO: Implement display settings
                        break;
                    case 3:  // 학습 설정
                        // TODO: Implement learning settings
                        break;
                    case 4:  // 시스템 설정
                        // TODO: Implement system settings
                        break;
                    case 5:  // SD 카드 정보
                        // Just show info, no action needed
                        break;
                }
            }
            break;
        }

        case SETTINGS_WIFI_AP: {
            // Back button (top area)
            if (touchY < 50) {
                settingsState = SETTINGS_MAIN;
                needsFullRedraw = true;
                break;
            }

            // Start file transfer button
            int btnY = SETTINGS_ITEMS_START_Y + 10 + 40 + 35 + 50 + 20;  // Calculate button Y
            int btnH = 60;
            int btnW = 300;
            int btnX = (SCREEN_WIDTH - btnW) / 2;

            if (touchY >= btnY && touchY <= btnY + btnH &&
                touchX >= btnX && touchX <= btnX + btnW) {
                startWiFiMode();
            }
            break;
        }

        case SETTINGS_WIFI_STA: {
            // Back button (top area)
            if (touchY < 50) {
                settingsState = SETTINGS_MAIN;
                needsFullRedraw = true;
            }
            // WiFi scan button - TODO: implement
            break;
        }

        default:
            settingsState = SETTINGS_MAIN;
            needsFullRedraw = true;
            break;
    }
}

void handleContentTouch(int touchX, int touchY) {
    switch (currentTab) {
        case TAB_COPY:
            handleCopyTabTouch(touchX, touchY);
            break;
        case TAB_SETTINGS:
            handleSettingsTabTouch(touchX, touchY);
            break;
        default:
            // Other tabs - no action yet
            break;
    }
}

void loop() {
    M5.update();  // Update M5Unified state (touch, buttons, etc.)

    if (!sdCardMounted) {
        delay(1000);
        return;
    }

    // Update battery periodically
    updateBattery();

    // Get touch state
    auto touch = M5.Touch.getDetail();

    // WiFi mode handling
    if (wifiMode) {
        if (touch.wasPressed()) {
            delay(300);
            stopWiFiMode();

            // Reload content and return to tab view
            loadTodaySentences();
            needsFullRedraw = true;
        }
        delay(50);
        return;
    }

    // Normal mode - tab-based navigation
    if (touch.wasPressed()) {
        delay(200);  // Debounce

        int touchX = touch.x;
        int touchY = touch.y;

        // Check if touch is in tab bar
        int tabTouched = handleTabTouch(touchX, touchY);

        if (tabTouched >= 0) {
            // Tab was touched
            switchTab((TabIndex)tabTouched);
        } else {
            // Content area was touched
            handleContentTouch(touchX, touchY);
        }
    }

    // Refresh display if needed
    refreshDisplay();

    delay(50);
}
