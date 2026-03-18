#include <Arduino.h>
#include <M5GFX.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <I2C_BM8563.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// ============================================
// Hardware Configuration
// ============================================
static M5GFX display;
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);
AsyncWebServer server(80);

// M5Paper S3 SD Card pins
#define SD_CS   4
#define SD_SCK  36
#define SD_MISO 35
#define SD_MOSI 37

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

// WiFi AP Settings
const char* AP_SSID = "Papers3-JP";
const char* AP_PASS = "12345678";

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

        loadInfo();
        loadFiles();
    </script>
</body>
</html>
)rawliteral";

// ============================================
// SD Card Functions
// ============================================
bool initSDCard() {
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS, SPI, 25000000)) {
        Serial.println("SD Card mount failed");
        return false;
    }

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
    display.setTextSize(1.5);
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
    display.setFont(&fonts::efontJA_16);
    display.setTextSize(1.0);

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
    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.4);
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

    server.begin();
    Serial.println("Web server started");
}

void startWiFiMode() {
    wifiMode = true;

    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    setupWebServer();

    // Display WiFi info
    display.fillScreen(TFT_WHITE);
    display.setTextColor(TFT_BLACK);
    display.setTextSize(1.5);

    int y = PAD_Y;
    display.setCursor(PAD_X, y);
    display.println("WiFi File Transfer Mode");
    y += 60;

    display.setTextSize(1.2);
    display.setCursor(PAD_X, y);
    display.println("1. Connect to WiFi:");
    y += 40;

    display.setTextSize(1.4);
    display.setCursor(PAD_X + 40, y);
    display.printf("SSID: %s", AP_SSID);
    y += 35;
    display.setCursor(PAD_X + 40, y);
    display.printf("Pass: %s", AP_PASS);
    y += 50;

    display.setTextSize(1.2);
    display.setCursor(PAD_X, y);
    display.println("2. Open browser:");
    y += 40;

    display.setTextSize(1.4);
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
    Wire.begin(41, 42);
    rtc.begin();

    I2C_BM8563_DateTypeDef date;
    rtc.getDate(&date);

    char target[8];
    sprintf(target, "#%02d%02d", date.month, date.date);
    todayDate = String(date.month) + "月" + String(date.date) + "日";

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

    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.5);
    display.setTextColor(TFT_BLACK);

    // Title
    int titleWidth = display.textWidth(tabName);
    display.setCursor((SCREEN_WIDTH - titleWidth) / 2, CONTENT_HEIGHT / 2 - 50);
    display.print(tabName);

    // Description
    display.setTextSize(1.0);
    int descWidth = display.textWidth(description);
    display.setCursor((SCREEN_WIDTH - descWidth) / 2, CONTENT_HEIGHT / 2 + 10);
    display.print(description);

    display.setTextSize(1.4);
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

void drawSettingsTab() {
    drawPlaceholderContent("설정", "Touch LEFT: WiFi 파일 전송\nTouch RIGHT: Coming soon");
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
    int fontH = display.fontHeight();

    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.4);
    display.setTextColor(TFT_BLACK);

    display.setCursor(PAD_X, y);
    display.println(todayDate);
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

    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.4);
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
        // No content loaded
        display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
        display.setFont(&fonts::efontJA_24);
        display.setTextSize(1.2);
        display.setTextColor(TFT_BLACK);

        int centerY = CONTENT_HEIGHT / 2;

        display.setCursor(PAD_X, centerY - 40);
        display.println("오늘의 필사 내용이 없습니다");

        display.setCursor(PAD_X, centerY + 10);
        display.println("설정 탭에서 WiFi 파일 전송으로 콘텐츠를 업로드하세요");

        display.setTextSize(1.4);
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

void refreshDisplay() {
    if (needsFullRedraw) {
        display.fillScreen(TFT_WHITE);
        drawCurrentTabContent();
        drawTabBar();
        display.display();
        needsFullRedraw = false;
        needsTabRedraw = false;
    } else if (needsTabRedraw) {
        drawTabBar();
        display.display();
        needsTabRedraw = false;
    }
}

// ============================================
// Setup & Loop
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.println("Papers3 JP Learner Starting...");

    // Initialize display
    display.begin();
    display.waitDisplay();
    display.setRotation(1);  // Landscape mode
    display.setEpdMode(epd_mode_t::epd_text);
    display.setTextColor(TFT_BLACK);
    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.4);

    // Initialize LittleFS (for config and default font)
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
    }

    // Initialize SD Card
    sdCardMounted = initSDCard();

    if (!sdCardMounted) {
        showSDCardError("SD card not detected.");
        return;
    }

    // Ensure directory structure
    if (!ensureDirectories()) {
        showSDCardError("Failed to create directories.");
        return;
    }

    Serial.printf("SD Card Free Space: %llu MB\n", getSDCardFreeSpace() / (1024 * 1024));

    // Initialize battery reading
    analogReadResolution(12);
    updateBattery();

    // Load 필사 content
    loadTodaySentences();

    // Draw initial screen with tab bar
    needsFullRedraw = true;
    refreshDisplay();
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
    // Left side - WiFi mode
    if (touchX < SCREEN_WIDTH / 2) {
        startWiFiMode();
    }
    // Right side - future settings menu
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
    if (!sdCardMounted) {
        delay(1000);
        return;
    }

    // Update battery periodically
    updateBattery();

    lgfx::touch_point_t tp;

    // WiFi mode handling
    if (wifiMode) {
        if (display.getTouch(&tp, 1)) {
            delay(300);
            stopWiFiMode();

            // Reload content and return to tab view
            loadTodaySentences();
            needsFullRedraw = true;
            while (display.getTouch(&tp, 1)) delay(10);
        }
        delay(50);
        return;
    }

    // Normal mode - tab-based navigation
    if (display.getTouch(&tp, 1)) {
        delay(200);  // Debounce

        // Check if touch is in tab bar
        int tabTouched = handleTabTouch(tp.x, tp.y);

        if (tabTouched >= 0) {
            // Tab was touched
            switchTab((TabIndex)tabTouched);
        } else {
            // Content area was touched
            handleContentTouch(tp.x, tp.y);
        }

        // Wait for touch release
        while (display.getTouch(&tp, 1)) delay(10);
    }

    // Refresh display if needed
    refreshDisplay();

    delay(50);
}
