#include "screens/CopyScreen.h"
#include "Config.h"
#include "FontManager.h"
#include <M5Unified.h>
#include <SD.h>
#include <LittleFS.h>

// Layout constants
constexpr int LINE_SPACING = 8;
constexpr int PARAGRAPH_SPACING = 24;
constexpr int NAV_HEIGHT = 50;
constexpr int HEADER_HEIGHT = 50;
constexpr int TAB_BAR_HEIGHT = 60;  // Tab bar at bottom of screen

CopyScreen::CopyScreen()
    : BaseScreen(),
      _currentPage(0),
      _totalPages(0) {
}

void CopyScreen::onEnter() {
    BaseScreen::onEnter();

    // Reload font settings
    FontManager& fm = FontManager::instance();
    Serial.println("=== CopyScreen::onEnter ===");
    Serial.printf("CopyScreen: primaryFont='%s', size=%d\n",
                  config.primaryFont.c_str(), config.fontSizePt);
    Serial.flush();

    if (config.primaryFont.length() > 0) {
        Serial.println("CopyScreen: Loading custom font...");
        Serial.flush();
        bool loaded = fm.setPrimaryFont(config.primaryFont);
        Serial.printf("CopyScreen: Font load result=%d, hasCustomFont=%d, err=%d\n",
                      loaded, fm.hasCustomFont(), fm.getLastError());
        Serial.flush();
        fm.setFontSize(config.fontSizePt);
    } else {
        Serial.println("CopyScreen: Using built-in font");
        Serial.flush();
    }

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

    _date = String(dt.date.month) + "月" + String(dt.date.date) + "日";
    _paragraphs.clear();
    _title = "";
    _author = "";
    _currentPage = 0;
    _totalPages = 0;
    _pageBreaks.clear();

    Serial.printf("CopyScreen: Loading day %d\n", dayOfYear);

    // Try EPUB first, then fall back to text file
    bool loaded = loadFromEpub(dayOfYear);
    if (!loaded) {
        loaded = loadFromTextFile(dayOfYear);
    }

    if (loaded) {
        calculatePages();
        Serial.printf("CopyScreen: Loaded '%s' by %s, %d paragraphs, %d pages\n",
                      _title.c_str(), _author.c_str(), _paragraphs.size(), _totalPages);
    }

    requestRedraw();
    return loaded;
}

String CopyScreen::findEpubFile() {
    File dir = SD.open(DIR_BOOKS);
    if (!dir) {
        Serial.println("CopyScreen: Cannot open books directory");
        return "";
    }

    String configMatch = "";  // Match for config.dailyEpub
    String firstEpub = "";    // First EPUB found
    String bestMatch = "";    // Best auto-detect match

    Serial.printf("CopyScreen: Looking for EPUB (config: '%s')\n", config.dailyEpub.c_str());

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        entry.close();

        if (!isFile || !name.endsWith(".epub")) {
            continue;
        }

        Serial.printf("CopyScreen: Found EPUB: %s\n", name.c_str());

        String fullPath = String(DIR_BOOKS) + "/" + name;

        // Check if this matches config.dailyEpub
        if (config.dailyEpub.length() > 0 && name == config.dailyEpub) {
            configMatch = fullPath;
            Serial.printf("CopyScreen: Config match found!\n");
            break;  // Found exact match, no need to continue
        }

        // Remember first EPUB as fallback
        if (firstEpub.length() == 0) {
            firstEpub = fullPath;
        }

        // Auto-detect: prefer files with "365" or "1日1話" in name
        if (bestMatch.length() == 0 &&
            (name.indexOf("365") >= 0 || name.indexOf("1日") >= 0 ||
             name.indexOf("daily") >= 0 || name.indexOf("Daily") >= 0)) {
            bestMatch = fullPath;
            Serial.printf("CopyScreen: Auto-detect match: %s\n", name.c_str());
        }
    }
    dir.close();

    // Priority: config match > auto-detect > first EPUB
    String result;
    if (configMatch.length() > 0) {
        result = configMatch;
    } else if (bestMatch.length() > 0) {
        result = bestMatch;
    } else {
        result = firstEpub;
    }

    if (result.length() > 0) {
        Serial.printf("CopyScreen: Selected EPUB: %s\n", result.c_str());
    }
    return result;
}

bool CopyScreen::loadFromEpub(int dayOfYear) {
    // Find the EPUB file that should be used
    String targetPath = findEpubFile();
    if (targetPath.length() == 0) {
        Serial.println("CopyScreen: No EPUB found");
        return false;
    }

    // If a different EPUB is requested, close current and open new
    if (_epubParser.isOpen() && _epubPath != targetPath) {
        Serial.printf("CopyScreen: Switching EPUB from %s to %s\n",
                      _epubPath.c_str(), targetPath.c_str());
        _epubParser.close();
    }

    // Open EPUB if not already open
    if (!_epubParser.isOpen()) {
        _epubPath = targetPath;

        if (!_epubParser.open(_epubPath)) {
            Serial.println("CopyScreen: Failed to open EPUB");
            return false;
        }

        // Get metadata
        const EpubMetadata& meta = _epubParser.getMetadata();
        Serial.printf("CopyScreen: EPUB opened - %s by %s, %d chapters\n",
                      meta.title.c_str(), meta.author.c_str(), _epubParser.getChapterCount());
    }

    // Chapter index = dayOfYear - 1 (0-indexed)
    // Some books may have intro chapters, so we might need to adjust
    int chapterIndex = dayOfYear - 1;

    // Bounds check
    if (chapterIndex < 0 || chapterIndex >= _epubParser.getChapterCount()) {
        Serial.printf("CopyScreen: Chapter %d out of range (total: %d)\n",
                      chapterIndex, _epubParser.getChapterCount());
        return false;
    }

    // Get chapter text
    String chapterText = _epubParser.getChapterText(chapterIndex);
    if (chapterText.length() == 0) {
        Serial.println("CopyScreen: Empty chapter content");
        return false;
    }

    // Parse the chapter content
    parseChapterContent(chapterText);

    // If no title extracted from content, use chapter info
    if (_title.length() == 0) {
        const auto& chapters = _epubParser.getChapters();
        _title = chapters[chapterIndex].title;
    }

    // Use book author if not extracted from chapter
    if (_author.length() == 0) {
        _author = _epubParser.getMetadata().author;
    }

    return _paragraphs.size() > 0;
}

void CopyScreen::parseChapterContent(const String& text) {
    // Split text into paragraphs by newlines
    int start = 0;
    int len = text.length();

    // Try to extract title from first line (often formatted differently)
    int firstNewline = text.indexOf('\n');
    if (firstNewline > 0 && firstNewline < 100) {
        String firstLine = text.substring(0, firstNewline);
        firstLine.trim();

        // If first line is short, it might be a title
        if (firstLine.length() > 0 && firstLine.length() < 50) {
            _title = firstLine;
            start = firstNewline + 1;
        }
    }

    // Parse remaining content into paragraphs
    while (start < len) {
        int end = text.indexOf('\n', start);
        if (end < 0) end = len;

        String para = text.substring(start, end);
        para.trim();

        if (para.length() > 0) {
            // Check for author line (often starts with special characters)
            if (_author.length() == 0 && _paragraphs.empty() &&
                (para.startsWith("―") || para.startsWith("—") ||
                 para.startsWith("─") || para.startsWith("【"))) {
                // This might be an author attribution
                _author = para;
                _author.replace("―", "");
                _author.replace("—", "");
                _author.replace("─", "");
                _author.replace("【", "");
                _author.replace("】", "");
                _author.trim();
            } else {
                _paragraphs.push_back(para);
            }
        }

        start = end + 1;
    }
}

bool CopyScreen::loadFromTextFile(int dayOfYear) {
    char target[8];
    sprintf(target, "#%03d", dayOfYear);

    Serial.printf("CopyScreen: Trying text file for %s\n", target);

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

    return _paragraphs.size() > 0;
}

void CopyScreen::calculatePages() {
    _pageBreaks.clear();
    _pageBreaks.push_back(0);  // First page starts at paragraph 0

    if (_paragraphs.empty()) {
        _totalPages = 0;
        return;
    }

    FontManager& fm = FontManager::instance();
    int fontH;

    if (fm.hasCustomFont()) {
        fontH = config.fontSizePt + 4;  // Custom font height
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        fontH = M5.Display.fontHeight();
    }

    int contentW = SCREEN_WIDTH - PAD_X * 2;
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;  // Area above tab bar
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT - PAD_Y * 3;  // Extra padding

    int currentHeight = 0;
    int paraIndex = 0;

    while (paraIndex < _paragraphs.size()) {
        const String& para = _paragraphs[paraIndex];

        // Calculate wrapped height for this paragraph
        int paraWidth = M5.Display.textWidth(para.c_str());
        int lines = (paraWidth / contentW) + 1;
        int paraHeight = lines * (fontH + LINE_SPACING) + PARAGRAPH_SPACING;

        if (currentHeight + paraHeight > contentH && currentHeight > 0) {
            // Start a new page
            _pageBreaks.push_back(paraIndex);
            currentHeight = paraHeight;
        } else {
            currentHeight += paraHeight;
        }

        paraIndex++;
    }

    _totalPages = _pageBreaks.size();
    Serial.printf("CopyScreen: Calculated %d pages\n", _totalPages);
}

void CopyScreen::drawHeader() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int headerY = (HEADER_HEIGHT - fontH) / 2;

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
    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 1, TFT_BLACK);
}

void CopyScreen::drawPageContent() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT + PAD_Y;
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT;

    // Clear content area
    M5.Display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, contentH, TFT_WHITE);

    if (_currentPage >= _totalPages || _pageBreaks.empty()) {
        return;
    }

    FontManager& fm = FontManager::instance();
    bool useCustomFont = fm.hasCustomFont();

    int fontH;
    int contentW = SCREEN_WIDTH - PAD_X * 2;

    if (useCustomFont) {
        fm.setFontSize(config.fontSizePt);
        fontH = config.fontSizePt + 4;  // Approximate height
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        fontH = M5.Display.fontHeight();
    }
    M5.Display.setTextColor(TFT_BLACK);

    int maxY = availableHeight - NAV_HEIGHT - PAD_Y * 2;  // Extra padding above nav buttons

    int startPara = _pageBreaks[_currentPage];
    int endPara = (_currentPage + 1 < _totalPages) ? _pageBreaks[_currentPage + 1] : _paragraphs.size();

    int y = contentY;

    for (int i = startPara; i < endPara && y < maxY; i++) {
        const String& para = _paragraphs[i];

        // Draw wrapped text
        int bytePos = 0;
        int byteLen = para.length();

        while (bytePos < byteLen && y < maxY) {
            int lineEnd = bytePos;

            // Find line break point
            while (lineEnd < byteLen) {
                int charLen = 1;
                uint8_t c = para[lineEnd];
                if (c >= 0xF0) charLen = 4;
                else if (c >= 0xE0) charLen = 3;
                else if (c >= 0xC0) charLen = 2;

                String sub = para.substring(bytePos, lineEnd + charLen);
                int textW = useCustomFont ? fm.getTextWidth(sub) : M5.Display.textWidth(sub.c_str());
                if (textW > contentW) break;

                lineEnd += charLen;
            }

            if (lineEnd == bytePos) lineEnd += 1;

            // Draw this line
            if (useCustomFont) {
                fm.drawString(para.substring(bytePos, lineEnd), PAD_X, y);
            } else {
                M5.Display.setCursor(PAD_X, y);
                M5.Display.print(para.substring(bytePos, lineEnd));
            }

            bytePos = lineEnd;
            y += fontH + LINE_SPACING;
        }

        y += PARAGRAPH_SPACING - LINE_SPACING;  // Extra space between paragraphs
    }
}

void CopyScreen::drawNavigation() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int navY = availableHeight - NAV_HEIGHT;

    // Clear navigation area
    M5.Display.fillRect(0, navY, SCREEN_WIDTH, NAV_HEIGHT, TFT_WHITE);

    // Separator line
    M5.Display.drawLine(PAD_X, navY, SCREEN_WIDTH - PAD_X, navY, TFT_LIGHTGRAY);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);

    int buttonY = navY + (NAV_HEIGHT - 30) / 2;
    int buttonW = 120;

    // Previous button (left)
    if (_currentPage > 0) {
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(PAD_X + 20, buttonY);
        M5.Display.print("< 이전");
    }

    // Page indicator (center)
    M5.Display.setTextColor(TFT_DARKGRAY);
    String pageInfo = String(_currentPage + 1) + " / " + String(_totalPages);
    int pageInfoW = M5.Display.textWidth(pageInfo.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - pageInfoW) / 2, buttonY);
    M5.Display.print(pageInfo);

    // Next button (right)
    if (_currentPage < _totalPages - 1) {
        M5.Display.setTextColor(TFT_BLACK);
        String nextText = "다음 >";
        int nextW = M5.Display.textWidth(nextText.c_str());
        M5.Display.setCursor(SCREEN_WIDTH - PAD_X - nextW - 20, buttonY);
        M5.Display.print(nextText);
    }
}

void CopyScreen::drawEmptyState() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    auto dt = M5.Rtc.getDateTime();
    int dayOfYear = getDayOfYear(dt.date.month, dt.date.date);

    int centerY = availableHeight / 2;

    M5.Display.setCursor(PAD_X, centerY - 80);
    M5.Display.printf("%d月%d日 (Day %d)", dt.date.month, dt.date.date, dayOfYear);

    M5.Display.setCursor(PAD_X, centerY - 40);
    M5.Display.println("오늘의 필사 내용이 없습니다");

    M5.Display.setTextSize(0.9);
    M5.Display.setCursor(PAD_X, centerY + 10);
    M5.Display.println("필요한 파일:");

    M5.Display.setCursor(PAD_X + 20, centerY + 45);
    M5.Display.println("- /books/365*.epub (EPUB 형식)");

    M5.Display.setCursor(PAD_X + 20, centerY + 75);
    M5.Display.println("- /books/365.txt (텍스트 형식)");

    M5.Display.setCursor(PAD_X, centerY + 115);
    M5.Display.println("설정 > WiFi 파일 전송으로 업로드하세요");
}

void CopyScreen::draw() {
    // Use fast refresh mode for less flickering
    M5.Display.setEpdMode(epd_mode_t::epd_fast);

    // Batch all drawing operations - no screen update until endWrite()
    M5.Display.startWrite();

    if (_paragraphs.empty()) {
        drawEmptyState();
    } else {
        drawHeader();
        drawPageContent();
        drawNavigation();
    }

    // Now update the screen once
    M5.Display.endWrite();
}

bool CopyScreen::handleTouchStart(int x, int y) {
    if (_paragraphs.empty() || _totalPages == 0) {
        return false;
    }

    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int navY = availableHeight - NAV_HEIGHT;

    // Only handle touches in navigation area (above tab bar)
    if (y < navY || y >= availableHeight) {
        return false;
    }

    bool pageChanged = false;

    // Left side - previous page
    if (x < SCREEN_WIDTH / 3 && _currentPage > 0) {
        _currentPage--;
        pageChanged = true;
    }
    // Right side - next page
    else if (x > SCREEN_WIDTH * 2 / 3 && _currentPage < _totalPages - 1) {
        _currentPage++;
        pageChanged = true;
    }

    if (pageChanged) {
        // Direct draw without explicit display() - avoids flicker and ghosting
        M5.Display.startWrite();
        drawPageContent();
        drawNavigation();
        M5.Display.endWrite();
        // Return false to prevent main loop from triggering aggressive refresh
        return false;
    }

    return false;
}
