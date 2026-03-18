#include <Arduino.h>
#include <M5GFX.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <I2C_BM8563.h>

// ============================================
// Hardware Configuration
// ============================================
static M5GFX display;
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);

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
const int TAB_BAR_HEIGHT = 70;
const int CONTENT_HEIGHT = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

const int PAD_X = 30;
const int PAD_Y = 30;
const int LINE_SPACING = 16;

// SD Card directories
const char* DIR_BOOKS = "/books";
const char* DIR_DICT = "/dict";
const char* DIR_FONTS = "/fonts";
const char* DIR_USERDATA = "/userdata";

// ============================================
// State
// ============================================
bool sdCardMounted = false;
std::vector<String> sentences;
String todayTitle = "";
String todayAuthor = "";
String todayDate = "";
int currentPage = -1;

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

void showTitlePage() {
    display.fillScreen(TFT_WHITE);
    int contentW = display.width() - PAD_X * 2;
    int y = PAD_Y;
    int fontH = display.fontHeight();

    display.setCursor(PAD_X, y);
    display.println(todayDate);
    y += fontH + 16;

    display.drawLine(PAD_X, y, display.width() - PAD_X, y, TFT_BLACK);
    y += 20;

    y = printWrapped(todayTitle, PAD_X, y, contentW);
    y += 16;

    display.setCursor(PAD_X, y);
    display.println(todayAuthor);

    int totalPages = sentences.size() + 1;
    display.setCursor(display.width() - 120, CONTENT_HEIGHT - PAD_Y);
    display.printf("1 / %d", totalPages);
    display.display();
}

void showPage(int idx) {
    display.fillScreen(TFT_WHITE);

    int contentW = display.width() - PAD_X * 2;
    printWrapped(sentences[idx], PAD_X, PAD_Y, contentW);

    int totalPages = sentences.size() + 1;
    char info[20];
    sprintf(info, "%d / %d", idx + 2, totalPages);
    display.setCursor(display.width() - 120, CONTENT_HEIGHT - PAD_Y);
    display.println(info);
    display.display();
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

    // Load content
    if (loadTodaySentences()) {
        showTitlePage();
    } else {
        display.fillScreen(TFT_WHITE);
        display.setCursor(PAD_X, PAD_Y);
        display.println("No data for today");
        display.setCursor(PAD_X, PAD_Y + 40);
        display.println("Place 365.txt in /books/ on SD card");
        display.display();
    }
}

void loop() {
    if (!sdCardMounted) {
        delay(1000);
        return;
    }

    lgfx::touch_point_t tp;
    if (display.getTouch(&tp, 1)) {
        delay(300);
        if (currentPage == -1) {
            currentPage = 0;
            showPage(currentPage);
        } else {
            currentPage++;
            if (currentPage >= (int)sentences.size()) {
                currentPage = -1;
                showTitlePage();
            } else {
                showPage(currentPage);
            }
        }
        while (display.getTouch(&tp, 1)) delay(10);
    }
    delay(50);
}
