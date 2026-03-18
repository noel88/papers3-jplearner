#include "screens/CopyScreen.h"
#include <M5Unified.h>
#include <SD.h>
#include <LittleFS.h>

// Line spacing constant
constexpr int LINE_SPACING = 16;

CopyScreen::CopyScreen()
    : ScrollableScreen() {
}

void CopyScreen::onEnter() {
    ScrollableScreen::onEnter();
    // Reload content when entering screen
    loadTodayContent();
}

int CopyScreen::getDayOfYear(int month, int day) {
    const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dayOfYear = 0;
    for (int m = 1; m < month; m++) {
        dayOfYear += daysInMonth[m];
    }
    dayOfYear += day;
    return dayOfYear;
}

bool CopyScreen::loadTodayContent() {
    auto dt = M5.Rtc.getDateTime();
    int dayOfYear = getDayOfYear(dt.date.month, dt.date.date);

    char target[8];
    sprintf(target, "#%03d", dayOfYear);
    _date = String(dt.date.month) + "月" + String(dt.date.date) + "日";

    Serial.printf("CopyScreen: Loading day %d (%s)\n", dayOfYear, target);

    File f;
    String filePath = String(DIR_BOOKS) + "/365.txt";

    if (SD.exists(filePath.c_str())) {
        f = SD.open(filePath.c_str(), FILE_READ);
    } else if (LittleFS.exists("/365.txt")) {
        f = LittleFS.open("/365.txt", "r");
    }

    if (!f) {
        Serial.println("CopyScreen: 365.txt not found");
        return false;
    }

    bool found = false;
    _paragraphs.clear();
    _title = "";
    _author = "";

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();

        if (line.startsWith("#") && line.length() <= 5) {
            if (found) break;
            if (line == String(target)) {
                found = true;
            }
            continue;
        }

        if (!found) continue;

        if (line.startsWith("TITLE:")) {
            _title = line.substring(6);
            _title.trim();
        } else if (line.startsWith("AUTHOR:")) {
            _author = line.substring(7);
            _author.trim();
        } else if (line.length() > 0) {
            _paragraphs.push_back(line);
        }
    }
    f.close();

    // Reset scroll on new content
    resetScroll();
    _contentHeight = calculateContentHeight();

    Serial.printf("CopyScreen: Loaded '%s' by %s, %d paragraphs, height=%d\n",
                  _title.c_str(), _author.c_str(), _paragraphs.size(), _contentHeight);

    requestRedraw();
    return _paragraphs.size() > 0;
}

int CopyScreen::calculateContentHeight() {
    if (_paragraphs.empty()) return 0;

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2 - getScrollBarWidth();
    int totalHeight = PAD_Y;  // Top padding

    // Title section
    totalHeight += fontH * 1.5 + 30;  // Title + author + spacing

    // Each paragraph
    for (const auto& para : _paragraphs) {
        // Estimate wrapped height
        int paraWidth = M5.Display.textWidth(para.c_str());
        int lines = (paraWidth / contentW) + 1;
        totalHeight += lines * (fontH + LINE_SPACING) + 20;  // Extra spacing between paragraphs
    }

    totalHeight += PAD_Y;  // Bottom padding
    return totalHeight;
}

void CopyScreen::drawHeader() {
    int headerHeight = getHeaderHeight();
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, headerHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int headerY = (headerHeight - fontH) / 2;

    // Date on the right
    int dateWidth = M5.Display.textWidth(_date.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - dateWidth, headerY);
    M5.Display.print(_date);

    // Author before date
    if (_author.length() > 0) {
        int authorWidth = M5.Display.textWidth(_author.c_str());
        int authorX = SCREEN_WIDTH - PAD_X - dateWidth - 40 - authorWidth;
        if (authorX > 450) {
            M5.Display.setCursor(authorX, headerY);
            M5.Display.print(_author);
        }
    }

    // Title on the left (truncate if needed)
    if (_title.length() > 0) {
        String displayTitle = _title;
        int maxTitleWidth = 400;
        while (M5.Display.textWidth(displayTitle.c_str()) > maxTitleWidth && displayTitle.length() > 6) {
            displayTitle = displayTitle.substring(0, displayTitle.length() - 4) + "...";
        }
        M5.Display.setCursor(PAD_X, headerY);
        M5.Display.print(displayTitle);
    }

    // Separator line
    M5.Display.drawLine(PAD_X, headerHeight - 1, SCREEN_WIDTH - PAD_X, headerHeight - 1, TFT_BLACK);
}

int CopyScreen::printWrappedAt(const String& text, int x, int y, int maxWidth) {
    int fontH = M5.Display.fontHeight();
    int headerHeight = getHeaderHeight();
    int currentY = y;
    int bytePos = 0;
    int byteLen = text.length();

    while (bytePos < byteLen) {
        int lineEnd = bytePos;

        // Find line break point
        while (lineEnd < byteLen) {
            int charLen = 1;
            uint8_t c = text[lineEnd];
            if (c >= 0xF0) charLen = 4;
            else if (c >= 0xE0) charLen = 3;
            else if (c >= 0xC0) charLen = 2;

            String sub = text.substring(bytePos, lineEnd + charLen);
            if (M5.Display.textWidth(sub.c_str()) > maxWidth) break;

            lineEnd += charLen;
        }

        if (lineEnd == bytePos) lineEnd += 1;

        // Only draw if visible
        if (currentY >= headerHeight - fontH && currentY < CONTENT_HEIGHT) {
            M5.Display.setCursor(x, currentY);
            M5.Display.print(text.substring(bytePos, lineEnd));
        }

        bytePos = lineEnd;
        currentY += fontH + LINE_SPACING;
    }

    return currentY;
}

void CopyScreen::drawContent() {
    int headerHeight = getHeaderHeight();
    int visibleHeight = getVisibleHeight();

    // Clear content area (below header)
    M5.Display.fillRect(0, headerHeight, SCREEN_WIDTH - getScrollBarWidth(), visibleHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2 - getScrollBarWidth();

    // Apply scroll offset
    int y = headerHeight + PAD_Y - _scrollY;

    // Title (larger)
    M5.Display.setTextSize(1.2);
    if (y > headerHeight - 40 && y < CONTENT_HEIGHT) {
        M5.Display.setCursor(PAD_X, y);
        M5.Display.print(_title);
    }
    y += fontH * 1.5;

    // Author
    M5.Display.setTextSize(1.0);
    if (y > headerHeight - 30 && y < CONTENT_HEIGHT) {
        M5.Display.setCursor(PAD_X, y);
        M5.Display.print("― ");
        M5.Display.print(_author);
    }
    y += fontH + 30;

    // Paragraphs
    for (const auto& para : _paragraphs) {
        y = printWrappedAt(para, PAD_X, y, contentW);
        y += 20;  // Paragraph spacing
    }
}

void CopyScreen::drawEmptyState() {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    auto dt = M5.Rtc.getDateTime();
    int dayOfYear = getDayOfYear(dt.date.month, dt.date.date);

    int centerY = CONTENT_HEIGHT / 2;

    M5.Display.setCursor(PAD_X, centerY - 60);
    M5.Display.printf("%d月%d日 (Day %d)", dt.date.month, dt.date.date, dayOfYear);

    M5.Display.setCursor(PAD_X, centerY - 20);
    M5.Display.println("오늘의 필사 내용이 없습니다");

    M5.Display.setTextSize(0.9);
    M5.Display.setCursor(PAD_X, centerY + 30);
    M5.Display.println("SD 카드의 /books/365.txt 파일이 필요합니다");

    M5.Display.setCursor(PAD_X, centerY + 70);
    M5.Display.println("설정 > WiFi 파일 전송으로 업로드하세요");
}

void CopyScreen::draw() {
    if (_paragraphs.empty()) {
        drawEmptyState();
        return;
    }

    // Use ScrollableScreen's template method
    ScrollableScreen::draw();
}
