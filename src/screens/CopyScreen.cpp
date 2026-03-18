#include "CopyScreen.h"
#include "Config.h"
#include <M5Unified.h>
#include <SD.h>
#include <LittleFS.h>

// Header configuration
const int COPY_HEADER_HEIGHT = 50;
const int SCROLL_BAR_WIDTH = 8;
const int SCROLL_THRESHOLD = 10;  // Minimum drag distance to trigger scroll

CopyScreen::CopyScreen()
    : scrollY(0)
    , contentHeight(0)
    , visibleHeight(CONTENT_HEIGHT - COPY_HEADER_HEIGHT)
    , lastTouchY(0)
    , isDragging(false) {
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
    date = String(dt.date.month) + "月" + String(dt.date.date) + "日";

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
    paragraphs.clear();
    title = "";
    author = "";

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
            title = line.substring(6);
            title.trim();
        } else if (line.startsWith("AUTHOR:")) {
            author = line.substring(7);
            author.trim();
        } else if (line.length() > 0) {
            paragraphs.push_back(line);
        }
    }
    f.close();

    // Reset scroll on new content
    scrollY = 0;
    contentHeight = calculateContentHeight();

    Serial.printf("CopyScreen: Loaded '%s' by %s, %d paragraphs, height=%d\n",
                  title.c_str(), author.c_str(), paragraphs.size(), contentHeight);

    return paragraphs.size() > 0;
}

int CopyScreen::calculateContentHeight() {
    if (paragraphs.empty()) return 0;

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2 - SCROLL_BAR_WIDTH;
    int totalHeight = PAD_Y;  // Top padding

    // Title section
    totalHeight += fontH * 1.5 + 30;  // Title + author + spacing

    // Each paragraph
    for (const auto& para : paragraphs) {
        // Estimate wrapped height
        int paraWidth = M5.Display.textWidth(para.c_str());
        int lines = (paraWidth / contentW) + 1;
        totalHeight += lines * (fontH + LINE_SPACING) + 20;  // Extra spacing between paragraphs
    }

    totalHeight += PAD_Y;  // Bottom padding
    return totalHeight;
}

void CopyScreen::drawHeader() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, COPY_HEADER_HEIGHT, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int headerY = (COPY_HEADER_HEIGHT - fontH) / 2;

    // Date on the right
    int dateWidth = M5.Display.textWidth(date.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - dateWidth, headerY);
    M5.Display.print(date);

    // Author before date
    if (author.length() > 0) {
        int authorWidth = M5.Display.textWidth(author.c_str());
        int authorX = SCREEN_WIDTH - PAD_X - dateWidth - 40 - authorWidth;
        if (authorX > 450) {
            M5.Display.setCursor(authorX, headerY);
            M5.Display.print(author);
        }
    }

    // Title on the left (truncate if needed)
    if (title.length() > 0) {
        String displayTitle = title;
        int maxTitleWidth = 400;
        while (M5.Display.textWidth(displayTitle.c_str()) > maxTitleWidth && displayTitle.length() > 6) {
            displayTitle = displayTitle.substring(0, displayTitle.length() - 4) + "...";
        }
        M5.Display.setCursor(PAD_X, headerY);
        M5.Display.print(displayTitle);
    }

    // Separator line
    M5.Display.drawLine(PAD_X, COPY_HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, COPY_HEADER_HEIGHT - 1, TFT_BLACK);
}

int CopyScreen::printWrappedAt(const String& text, int x, int y, int maxWidth) {
    int fontH = M5.Display.fontHeight();
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
        if (currentY >= COPY_HEADER_HEIGHT - fontH && currentY < CONTENT_HEIGHT) {
            M5.Display.setCursor(x, currentY);
            M5.Display.print(text.substring(bytePos, lineEnd));
        }

        bytePos = lineEnd;
        currentY += fontH + LINE_SPACING;
    }

    return currentY;
}

void CopyScreen::drawContent() {
    // Clear content area (below header)
    M5.Display.fillRect(0, COPY_HEADER_HEIGHT, SCREEN_WIDTH - SCROLL_BAR_WIDTH, visibleHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2 - SCROLL_BAR_WIDTH;

    // Apply scroll offset
    int y = COPY_HEADER_HEIGHT + PAD_Y - scrollY;

    // Title (larger)
    M5.Display.setTextSize(1.2);
    if (y > COPY_HEADER_HEIGHT - 40 && y < CONTENT_HEIGHT) {
        M5.Display.setCursor(PAD_X, y);
        M5.Display.print(title);
    }
    y += fontH * 1.5;

    // Author
    M5.Display.setTextSize(1.0);
    if (y > COPY_HEADER_HEIGHT - 30 && y < CONTENT_HEIGHT) {
        M5.Display.setCursor(PAD_X, y);
        M5.Display.print("― ");
        M5.Display.print(author);
    }
    y += fontH + 30;

    // Paragraphs
    for (const auto& para : paragraphs) {
        y = printWrappedAt(para, PAD_X, y, contentW);
        y += 20;  // Paragraph spacing
    }
}

void CopyScreen::drawScrollIndicator() {
    if (contentHeight <= visibleHeight) return;

    int scrollBarX = SCREEN_WIDTH - SCROLL_BAR_WIDTH;
    int scrollBarHeight = visibleHeight;
    int scrollBarY = COPY_HEADER_HEIGHT;

    // Background
    M5.Display.fillRect(scrollBarX, scrollBarY, SCROLL_BAR_WIDTH, scrollBarHeight, 0xDEDB);  // Light gray

    // Calculate thumb
    float visibleRatio = (float)visibleHeight / contentHeight;
    int thumbHeight = max(30, (int)(scrollBarHeight * visibleRatio));

    float scrollRatio = (float)scrollY / (contentHeight - visibleHeight);
    int thumbY = scrollBarY + (int)((scrollBarHeight - thumbHeight) * scrollRatio);

    // Draw thumb
    M5.Display.fillRect(scrollBarX + 1, thumbY, SCROLL_BAR_WIDTH - 2, thumbHeight, 0x7BEF);  // Gray
}

void CopyScreen::draw() {
    if (paragraphs.empty()) {
        // No content message
        M5.Display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
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
        return;
    }

    drawHeader();
    drawContent();
    drawScrollIndicator();
}

bool CopyScreen::handleTouch(int x, int y) {
    if (paragraphs.empty()) return false;
    if (contentHeight <= visibleHeight) return false;  // No scroll needed

    // Simple scroll: touch top half = scroll up, bottom half = scroll down
    int scrollAmount = visibleHeight / 3;  // Scroll 1/3 of visible area

    if (y < CONTENT_HEIGHT / 2) {
        // Scroll up (show earlier content)
        scrollY -= scrollAmount;
        if (scrollY < 0) scrollY = 0;
    } else {
        // Scroll down (show later content)
        scrollY += scrollAmount;
        int maxScroll = contentHeight - visibleHeight;
        if (scrollY > maxScroll) scrollY = maxScroll;
    }

    Serial.printf("CopyScreen: scroll=%d/%d\n", scrollY, contentHeight - visibleHeight);
    return true;
}

void CopyScreen::resetScroll() {
    scrollY = 0;
}
