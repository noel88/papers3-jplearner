#include <Arduino.h>
#include <M5GFX.h>
#include <LittleFS.h>
#include <I2C_BM8563.h>

static M5GFX display;
I2C_BM8563 rtc(I2C_BM8563_DEFAULT_ADDRESS, Wire);

std::vector<String> sentences;
String todayTitle = "";
String todayAuthor = "";
String todayDate = "";
int currentPage = -1;

const int PAD_X = 30;
const int PAD_Y = 30;
const int LINE_SPACING = 16;

bool loadTodaySentences() {
    Wire.begin(41, 42);
    rtc.begin();

    I2C_BM8563_DateTypeDef date;
    rtc.getDate(&date);

    char target[8];
    sprintf(target, "#%02d%02d", date.month, date.date);
    todayDate = String(date.month) + "月" + String(date.date) + "日";

    File f = LittleFS.open("/365.txt", "r");
    if (!f) return false;

    bool found = false;
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
        } else if (line.startsWith("AUTHOR:")) {
            todayAuthor = line.substring(7);
        } else if (line.length() > 0) {
            sentences.push_back(line);
        }
    }
    f.close();
    return sentences.size() > 0;
}

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
    display.setCursor(display.width() - 120, display.height() - PAD_Y);
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
    display.setCursor(display.width() - 120, display.height() - PAD_Y);
    display.println(info);
    display.display();
}

void setup() {
    display.begin();
    display.waitDisplay();
    display.setRotation(1);
    display.setEpdMode(epd_mode_t::epd_text);
    display.setTextColor(TFT_BLACK);
    display.setFont(&fonts::efontJA_24);
    display.setTextSize(1.4);

    LittleFS.begin();

    if (loadTodaySentences()) {
        showTitlePage();
    } else {
        display.fillScreen(TFT_WHITE);
        display.setCursor(PAD_X, PAD_Y);
        display.println("No data for today");
        display.display();
    }
}

void loop() {
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