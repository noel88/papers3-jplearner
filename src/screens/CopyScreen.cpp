#include "screens/CopyScreen.h"
#include "Config.h"
#include "FontManager.h"
#include "UIHelpers.h"
#include "SRSManager.h"
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
      _totalPages(0),
      _chapterOffset(-1),
      _readingMode(ReadingMode::DAILY),
      _currentChapter(0),
      _touchStartX(0),
      _touchStartY(0),
      _touchStartTime(0),
      _touchInContentArea(false),
      _inDragSelection(false),
      _showingDictionary(false) {
}

void CopyScreen::onEnter() {
    BaseScreen::onEnter();

    // Reload font settings
    FontManager& fm = FontManager::instance();
    if (config.primaryFont.length() > 0) {
        fm.setPrimaryFont(config.primaryFont);
        fm.setFontSize(config.fontSizePt);
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

    // Try EPUB first, then fall back to text file
    bool loaded = loadFromEpub(dayOfYear);
    if (!loaded) {
        loaded = loadFromTextFile(dayOfYear);
    }

    if (loaded) {
        calculatePages();
    }

    requestRedraw();
    return loaded;
}

String CopyScreen::findEpubFile() {
    File dir = SD.open(DIR_BOOKS);
    if (!dir) {
        return "";
    }

    String configMatch = "";
    String firstEpub = "";
    String bestMatch = "";

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        entry.close();

        if (!isFile || !name.endsWith(".epub")) {
            continue;
        }

        String fullPath = String(DIR_BOOKS) + "/" + name;

        // Check if this matches config.dailyEpub
        if (config.dailyEpub.length() > 0 && name == config.dailyEpub) {
            configMatch = fullPath;
            break;
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
        }
    }
    dir.close();

    // Priority: config match > auto-detect > first EPUB
    if (configMatch.length() > 0) {
        return configMatch;
    } else if (bestMatch.length() > 0) {
        return bestMatch;
    }
    return firstEpub;
}

int CopyScreen::detectChapterOffset() {
    // Search first 15 chapters to find where Day 1 content starts
    // Verify by checking consecutive chapters and cross-reference with known date

    int totalChapters = _epubParser.getChapterCount();
    int searchLimit = min(15, totalChapters);
    const auto& chapters = _epubParser.getChapters();

    // Helper: check if date pattern exists after newline (proper heading position)
    auto hasDateAfterNewline = [](const String& text, const char* datePattern) -> bool {
        int pos = text.indexOf(datePattern);
        if (pos < 0) return false;
        if (pos == 0) return true;
        for (int j = pos - 1; j >= 0 && j >= pos - 5; j--) {
            if (text.charAt(j) == '\n') return true;
        }
        char prevChar = text.charAt(pos - 1);
        return (prevChar == ' ' || prevChar == '\t' || prevChar == '\r');
    };

    for (int i = 0; i < searchLimit; i++) {
        String text = _epubParser.getChapterText(i);
        if (text.length() == 0) continue;

        String header = text.substring(0, min(300, (int)text.length()));
        bool foundDay1 = false;

        // Japanese date patterns for Jan 1
        if (hasDateAfterNewline(header, "1月1日") ||
            hasDateAfterNewline(header, "１月１日")) {
            foundDay1 = true;
        }

        // Day number patterns
        if (!foundDay1 && (hasDateAfterNewline(header, "第1日") ||
                          hasDateAfterNewline(header, "第１日") ||
                          hasDateAfterNewline(header, "Day 1") ||
                          hasDateAfterNewline(header, "DAY 1"))) {
            foundDay1 = true;
        }

        if (foundDay1) {
            // Verify: next chapter should have Day 2 pattern
            if (i + 1 < totalChapters) {
                String nextText = _epubParser.getChapterText(i + 1);
                String nextHeader = nextText.substring(0, min(300, (int)nextText.length()));

                bool hasDay2 = (hasDateAfterNewline(nextHeader, "1月2日") ||
                               hasDateAfterNewline(nextHeader, "１月２日") ||
                               hasDateAfterNewline(nextHeader, "第2日") ||
                               hasDateAfterNewline(nextHeader, "第２日") ||
                               hasDateAfterNewline(nextHeader, "Day 2") ||
                               hasDateAfterNewline(nextHeader, "DAY 2"));

                if (hasDay2) {
                    // Cross-verify with a known chapter to calculate correct offset
                    int testChapter = i + 75;
                    int correctOffset = i;

                    if (testChapter < totalChapters) {
                        String testText = _epubParser.getChapterText(testChapter);
                        String testHeader = testText.substring(0, min(200, (int)testText.length()));

                        // Normalize full-width digits to half-width
                        String normalized = testHeader;
                        normalized.replace("０", "0"); normalized.replace("１", "1");
                        normalized.replace("２", "2"); normalized.replace("３", "3");
                        normalized.replace("４", "4"); normalized.replace("５", "5");
                        normalized.replace("６", "6"); normalized.replace("７", "7");
                        normalized.replace("８", "8"); normalized.replace("９", "9");

                        // Extract actual date to calculate correct offset
                        int actualDayOfYear = -1;
                        for (int m = 1; m <= 12 && actualDayOfYear < 0; m++) {
                            for (int d = 1; d <= 31; d++) {
                                String pattern = String(m) + "月" + String(d) + "日";
                                if (normalized.indexOf(pattern) >= 0) {
                                    actualDayOfYear = getDayOfYear(m, d);
                                    break;
                                }
                            }
                        }

                        if (actualDayOfYear > 0) {
                            correctOffset = testChapter - (actualDayOfYear - 1);
                        }
                    }

                    _readingMode = ReadingMode::DAILY;
                    return correctOffset;
                } else {
                    continue;
                }
            } else {
                _readingMode = ReadingMode::DAILY;
                return i;
            }
        }
    }

    // Check file naming pattern as fallback (skip TOC/cover files)
    for (int i = 0; i < searchLimit; i++) {
        String href = chapters[i].href;
        href.toLowerCase();

        // Skip TOC, cover, and other non-content files
        if (href.indexOf("toc") >= 0 || href.indexOf("cover") >= 0 ||
            href.indexOf("title") >= 0 || href.indexOf("nav") >= 0) {
            continue;
        }

        // Check for "001" pattern and verify "002" in next
        if ((href.indexOf("001.") >= 0 || href.indexOf("_001") >= 0 ||
             href.indexOf("-001") >= 0 || href.indexOf("p-001") >= 0) && i + 1 < totalChapters) {
            String nextHref = chapters[i + 1].href;
            nextHref.toLowerCase();
            if (nextHref.indexOf("002.") >= 0 || nextHref.indexOf("_002") >= 0 ||
                nextHref.indexOf("-002") >= 0 || nextHref.indexOf("p-002") >= 0) {
                _readingMode = ReadingMode::DAILY;
                return i;
            }
        }
    }

    // No daily pattern found - switch to Sequential mode
    _readingMode = ReadingMode::SEQUENTIAL;
    return 0;  // Start from chapter 0
}

bool CopyScreen::loadFromEpub(int dayOfYear) {
    // Find the EPUB file that should be used
    String targetPath = findEpubFile();
    if (targetPath.length() == 0) {
        return false;
    }

    // If a different EPUB is requested, close current and reset offset
    if (_epubParser.isOpen() && _epubPath != targetPath) {
        _epubParser.close();
        _chapterOffset = -1;
    }

    // Open EPUB if not already open
    if (!_epubParser.isOpen()) {
        _epubPath = targetPath;

        if (!_epubParser.open(_epubPath)) {
            return false;
        }

        // Auto-detect chapter offset and reading mode
        _chapterOffset = detectChapterOffset();

        // Load saved progress for Sequential mode
        if (_readingMode == ReadingMode::SEQUENTIAL) {
            loadReadingProgress();
        }
    }

    // Calculate chapter index based on reading mode
    int chapterIndex;
    if (_readingMode == ReadingMode::DAILY) {
        chapterIndex = _chapterOffset + (dayOfYear - 1);
    } else {
        chapterIndex = _currentChapter;
    }

    // Bounds check
    if (chapterIndex < 0 || chapterIndex >= _epubParser.getChapterCount()) {
        return false;
    }

    // Get chapter text
    const auto& chapters = _epubParser.getChapters();
    String chapterText = _epubParser.getChapterText(chapterIndex);
    if (chapterText.length() == 0) {
        return false;
    }

    // Parse the chapter content
    parseChapterContent(chapterText);

    // If no title extracted from content, use chapter info
    if (_title.length() == 0) {
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

    File f;
    String filePath = String(DIR_BOOKS) + "/365.txt";

    if (SD.exists(filePath.c_str())) {
        f = SD.open(filePath.c_str(), FILE_READ);
    } else if (LittleFS.exists("/365.txt")) {
        f = LittleFS.open("/365.txt", "r");
    }

    if (!f) {
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
}

void CopyScreen::drawHeader() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int headerY = (HEADER_HEIGHT - fontH) / 2;

    // Right side: Date (Daily mode) or Chapter info (Sequential mode)
    String rightInfo;
    if (_readingMode == ReadingMode::DAILY) {
        rightInfo = _date;
    } else {
        // Show chapter progress: "Ch. 5/120"
        int totalChapters = _epubParser.getChapterCount() - _chapterOffset;
        int currentChNum = _currentChapter - _chapterOffset + 1;
        rightInfo = "Ch." + String(currentChNum) + "/" + String(totalChapters);
    }

    int rightWidth = M5.Display.textWidth(rightInfo.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - rightWidth, headerY);
    M5.Display.print(rightInfo);

    // Author before right info
    if (_author.length() > 0) {
        int authorWidth = M5.Display.textWidth(_author.c_str());
        int authorX = SCREEN_WIDTH - PAD_X - rightWidth - 40 - authorWidth;
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

    // Clear text layout for new page
    _textLayout.clear();

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

            String lineText = para.substring(bytePos, lineEnd);
            int lineWidth = useCustomFont ? fm.getTextWidth(lineText) : M5.Display.textWidth(lineText.c_str());

            // Record line position for text selection
            _textLayout.addLine(i, bytePos, lineEnd, PAD_X, y, lineWidth, fontH + LINE_SPACING, lineText);

            // Draw this line
            if (useCustomFont) {
                fm.drawString(lineText, PAD_X, y);
            } else {
                M5.Display.setCursor(PAD_X, y);
                M5.Display.print(lineText);
            }

            bytePos = lineEnd;
            y += fontH + LINE_SPACING;
        }

        y += PARAGRAPH_SPACING - LINE_SPACING;  // Extra space between paragraphs
    }

    // Draw selection highlight on top of text (inverted style)
    if (_textLayout.hasSelection()) {
        _textLayout.drawHighlight();
    }

    // Draw popup menu if visible (on top of everything)
    if (_popupMenu.isVisible()) {
        _popupMenu.draw();
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
    M5.Display.setTextSize(UI::SIZE_CONTENT);

    int buttonY = navY + (NAV_HEIGHT - 30) / 2;
    int buttonW = 120;

    // Previous button (left, bold)
    if (_currentPage > 0) {
        M5.Display.setTextColor(TFT_BLACK);
        UI::drawBoldText("< 이전", PAD_X + 20, buttonY);
    }

    // Page indicator (center, bold)
    M5.Display.setTextColor(TFT_DARKGRAY);
    String pageInfo = String(_currentPage + 1) + " / " + String(_totalPages);
    int pageInfoW = M5.Display.textWidth(pageInfo.c_str());
    UI::drawBoldText(pageInfo.c_str(), (SCREEN_WIDTH - pageInfoW) / 2, buttonY);

    // Next button (right, bold)
    if (_currentPage < _totalPages - 1) {
        M5.Display.setTextColor(TFT_BLACK);
        String nextText = "다음 >";
        int nextW = M5.Display.textWidth(nextText.c_str());
        UI::drawBoldText(nextText.c_str(), SCREEN_WIDTH - PAD_X - nextW - 20, buttonY);
    }
}

void CopyScreen::drawEmptyState() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(UI::SIZE_CONTENT);
    M5.Display.setTextColor(TFT_BLACK);

    auto dt = M5.Rtc.getDateTime();
    int dayOfYear = getDayOfYear(dt.date.month, dt.date.date);

    int centerY = availableHeight / 2;

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%d月%d日 (Day %d)", dt.date.month, dt.date.date, dayOfYear);
    UI::drawBoldText(dateBuf, PAD_X, centerY - 80);

    UI::drawBoldText("오늘의 필사 내용이 없습니다", PAD_X, centerY - 40);

    M5.Display.setTextSize(UI::SIZE_BODY);
    UI::drawBoldText("필요한 파일:", PAD_X, centerY + 10);

    UI::drawBoldText("- /books/365*.epub (EPUB 형식)", PAD_X + 20, centerY + 45);

    UI::drawBoldText("- /books/365.txt (텍스트 형식)", PAD_X + 20, centerY + 75);

    UI::drawBoldText("설정 > WiFi 파일 전송으로 업로드하세요", PAD_X, centerY + 115);
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
    int contentY = HEADER_HEIGHT;

    // Handle dictionary popup touch - any touch dismisses it
    if (_showingDictionary) {
        _showingDictionary = false;
        _dictResults.clear();
        _textLayout.clearSelection();
        _popupMenu.hide();

        // Redraw page
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
        M5.Display.startWrite();
        drawPageContent();
        M5.Display.endWrite();
        return true;
    }

    // Handle popup menu touch first
    if (_popupMenu.isVisible()) {
        PopupMenu::Action action = _popupMenu.handleTouch(x, y);
        // Any touch while popup is visible dismisses or acts on it
        handlePopupAction(action);
        // Reset touch state to prevent further processing
        _touchInContentArea = false;
        return true;
    }

    // Record touch start for long press detection
    _touchStartX = x;
    _touchStartY = y;
    _touchStartTime = millis();
    _touchInContentArea = (y >= contentY && y < navY);

    // Navigation area touch - handle immediately
    if (y >= navY && y < availableHeight) {
        // Clear any selection when navigating
        if (_textLayout.hasSelection()) {
            _textLayout.clearSelection();
            _popupMenu.hide();
        }

        bool pageChanged = false;
        bool chapterChanged = false;

        // Left side - previous page/chapter
        if (x < SCREEN_WIDTH / 3) {
            if (_currentPage > 0) {
                _currentPage--;
                pageChanged = true;
            } else if (_readingMode == ReadingMode::SEQUENTIAL && _currentChapter > _chapterOffset) {
                // Go to previous chapter
                _currentChapter--;
                chapterChanged = true;
            }
        }
        // Right side - next page/chapter
        else if (x > SCREEN_WIDTH * 2 / 3) {
            if (_currentPage < _totalPages - 1) {
                _currentPage++;
                pageChanged = true;
            } else if (_readingMode == ReadingMode::SEQUENTIAL &&
                       _currentChapter < _epubParser.getChapterCount() - 1) {
                // Go to next chapter
                _currentChapter++;
                _currentPage = 0;
                chapterChanged = true;
            }
        }

        if (chapterChanged) {
            // Reload content for new chapter
            loadTodayContent();
            saveReadingProgress();
            return true;
        }

        if (pageChanged) {
            // Save progress when page changes in Sequential mode
            if (_readingMode == ReadingMode::SEQUENTIAL) {
                saveReadingProgress();
            }
            // Direct draw without explicit display() - avoids flicker and ghosting
            M5.Display.startWrite();
            drawPageContent();
            drawNavigation();
            M5.Display.endWrite();
            return false;
        }
    }

    // Content area touch - don't select immediately, wait for long press
    return false;
}

bool CopyScreen::handleTouchMove(int x, int y) {
    if (!_touchInContentArea) {
        return false;
    }

    unsigned long pressDuration = millis() - _touchStartTime;
    int dx = abs(x - _touchStartX);
    int dy = abs(y - _touchStartY);

    // Before long press threshold
    if (pressDuration < LONG_PRESS_MS) {
        // If moved too much, cancel long press
        if (dx > TOUCH_MOVE_THRESHOLD || dy > TOUCH_MOVE_THRESHOLD) {
            _touchInContentArea = false;
        }
        return false;
    }

    // Long press threshold reached
    // First, select word at start position if not done yet
    if (!_inDragSelection) {
        WordInfo startWord;
        if (_textLayout.findWordAt(_touchStartX, _touchStartY, startWord)) {
            _dragStartWord = startWord;
            _textLayout.setSelection(startWord);
            _inDragSelection = true;

            // Draw initial highlight
            M5.Display.setEpdMode(epd_mode_t::epd_fastest);
            M5.Display.startWrite();
            drawPageContent();
            M5.Display.endWrite();
        }
        return false;
    }

    // Already in drag mode - extend selection if moved enough
    if (dx > 10 || dy > 10) {
        WordInfo currentWord;
        if (_textLayout.findWordAt(x, y, currentWord)) {
            // Only update if different from current selection
            if (currentWord.x != _dragStartWord.x || currentWord.y != _dragStartWord.y) {
                _textLayout.setRangeSelection(_dragStartWord, currentWord);

                // Redraw with updated selection
                M5.Display.setEpdMode(epd_mode_t::epd_fastest);
                M5.Display.startWrite();
                drawPageContent();
                M5.Display.endWrite();
            }
        }
    }

    return false;
}

bool CopyScreen::handleTouchEnd() {
    // Finalize drag selection if active
    if (_inDragSelection) {
        _inDragSelection = false;

        // Show popup with selected text
        if (_textLayout.hasSelection()) {
            int selX, selY, selW, selH;
            _textLayout.getSelectionBounds(selX, selY, selW, selH);
            _popupMenu.show(selX + selW / 2, selY, _textLayout.getSelectedText());

            M5.Display.setEpdMode(epd_mode_t::epd_fastest);
            M5.Display.startWrite();
            drawPageContent();
            M5.Display.endWrite();
        }
        return true;
    }

    if (!_touchInContentArea) {
        return false;
    }

    unsigned long pressDuration = millis() - _touchStartTime;

    // Long press without drag - single word selection
    if (pressDuration >= LONG_PRESS_MS) {
        handleWordSelection(_touchStartX, _touchStartY);
        return true;
    }

    // Short tap in content area - clear selection if any
    if (_textLayout.hasSelection()) {
        _textLayout.clearSelection();
        _popupMenu.hide();
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        M5.Display.startWrite();
        drawPageContent();
        M5.Display.endWrite();
        return true;
    }

    return false;
}

void CopyScreen::handleWordSelection(int x, int y) {
    WordInfo word;

    if (_textLayout.findWordAt(x, y, word)) {
        // Set selection
        _textLayout.setSelection(word);

        // Show popup menu above the word
        int popupX = word.x + word.width / 2;
        int popupY = word.y;
        _popupMenu.show(popupX, popupY, word.text);

        // Use fastest mode for minimal flicker
        // Redraw full content to ensure highlight shows correctly on e-ink
        M5.Display.setEpdMode(epd_mode_t::epd_fastest);
        M5.Display.startWrite();
        drawPageContent();  // This includes highlight and popup
        M5.Display.endWrite();
    } else {
        // Touch on empty area - clear selection
        if (_textLayout.hasSelection() || _popupMenu.isVisible()) {
            _textLayout.clearSelection();
            _popupMenu.hide();

            // Redraw content to remove highlight - need full redraw here
            M5.Display.setEpdMode(epd_mode_t::epd_fastest);
            M5.Display.startWrite();
            drawPageContent();
            M5.Display.endWrite();
        }
    }
}

void CopyScreen::handlePopupAction(PopupMenu::Action action) {
    // Handle NONE case (touch inside popup but not on button)
    if (action == PopupMenu::NONE) {
        return;  // Do nothing, keep popup open
    }

    String selectedText = _popupMenu.getSelectedText();

    switch (action) {
        case PopupMenu::SEARCH:
            showDictionaryPopup(selectedText);
            return;  // Don't clear selection yet

        case PopupMenu::SAVE:
            saveToVocabulary(selectedText);
            break;

        case PopupMenu::GRAMMAR:
            saveToGrammar(selectedText);
            break;

        case PopupMenu::CANCEL:
        default:
            // Just dismiss, no action needed
            break;
    }

    // Clear selection and hide popup
    _textLayout.clearSelection();
    _popupMenu.hide();

    // Redraw with fastest mode for minimal flicker
    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    M5.Display.startWrite();
    drawPageContent();
    M5.Display.endWrite();
}

void CopyScreen::saveToVocabulary(const String& word) {
    SRSManager& srs = SRSManager::instance();

    // Check if already saved
    if (srs.hasCard(word, "word")) {
        showToast("이미 저장됨");
        return;
    }

    // Add as SRS card
    String cardId = srs.addCard("word", word, "");  // Back will be filled later or by user
    if (cardId.length() > 0) {
        showToast("단어 저장됨");
    } else {
        showToast("저장 실패");
    }
}

void CopyScreen::saveToGrammar(const String& text) {
    SRSManager& srs = SRSManager::instance();

    // Check if already saved
    if (srs.hasCard(text, "grammar")) {
        showToast("이미 저장됨");
        return;
    }

    // Add as SRS card
    String cardId = srs.addCard("grammar", text, "");  // Back will be filled later
    if (cardId.length() > 0) {
        showToast("문형 저장됨");
    } else {
        showToast("저장 실패");
    }
}

void CopyScreen::showToast(const char* message) {
    // Draw toast message at center of screen
    int toastW = 200;
    int toastH = 50;
    int toastX = (SCREEN_WIDTH - toastW) / 2;
    int toastY = (SCREEN_HEIGHT - TAB_BAR_HEIGHT) / 2 - toastH / 2;

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    M5.Display.startWrite();

    // Draw toast background with border
    M5.Display.fillRect(toastX, toastY, toastW, toastH, TFT_WHITE);
    M5.Display.drawRect(toastX, toastY, toastW, toastH, TFT_BLACK);
    M5.Display.drawRect(toastX + 1, toastY + 1, toastW - 2, toastH - 2, TFT_BLACK);

    // Draw message centered
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int textW = M5.Display.textWidth(message);
    int textX = toastX + (toastW - textW) / 2;
    int textY = toastY + (toastH - 24) / 2;
    M5.Display.setCursor(textX, textY);
    M5.Display.print(message);

    M5.Display.endWrite();

    // Brief delay to show the toast
    delay(800);

    // Redraw page to remove toast
    M5.Display.startWrite();
    drawPageContent();
    M5.Display.endWrite();
}

void CopyScreen::loadReadingProgress() {
    // Load saved reading progress from LittleFS
    // Format: <epub_filename>:<chapter>:<page>
    File file = LittleFS.open("/userdata/reading_progress.txt", FILE_READ);
    if (!file) {
        _currentChapter = _chapterOffset;
        _currentPage = 0;
        return;
    }

    String line = file.readStringUntil('\n');
    file.close();

    // Parse: filename:chapter:page
    int sep1 = line.indexOf(':');
    int sep2 = line.indexOf(':', sep1 + 1);

    if (sep1 > 0 && sep2 > sep1) {
        String savedEpub = line.substring(0, sep1);
        String epubName = _epubPath.substring(_epubPath.lastIndexOf('/') + 1);

        if (savedEpub == epubName) {
            _currentChapter = line.substring(sep1 + 1, sep2).toInt();
            _currentPage = line.substring(sep2 + 1).toInt();
        } else {
            _currentChapter = _chapterOffset;
            _currentPage = 0;
        }
    } else {
        _currentChapter = _chapterOffset;
        _currentPage = 0;
    }
}

void CopyScreen::saveReadingProgress() {
    // Only save in Sequential mode
    if (_readingMode != ReadingMode::SEQUENTIAL) {
        return;
    }

    // Ensure directory exists
    if (!LittleFS.exists("/userdata")) {
        LittleFS.mkdir("/userdata");
    }

    File file = LittleFS.open("/userdata/reading_progress.txt", FILE_WRITE);
    if (file) {
        String epubName = _epubPath.substring(_epubPath.lastIndexOf('/') + 1);
        file.printf("%s:%d:%d\n", epubName.c_str(), _currentChapter, _currentPage);
        file.close();
    }
}

void CopyScreen::showDictionaryPopup(const String& word) {
    DictionaryManager& dict = DictionaryManager::instance();

    if (!dict.isAvailable()) {
        showToast("사전 없음");
        return;
    }

    // Look up the word
    _dictResults = dict.lookupByWord(word);

    if (_dictResults.empty()) {
        // Try partial search
        _dictResults = dict.search(word, 5);
    }

    if (_dictResults.empty()) {
        showToast("검색 결과 없음");
        return;
    }

    _showingDictionary = true;

    // Draw dictionary popup
    int popupW = 450;
    int popupH = 300;
    int popupX = (SCREEN_WIDTH - popupW) / 2;
    int popupY = (SCREEN_HEIGHT - TAB_BAR_HEIGHT - popupH) / 2;

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.startWrite();

    // Draw popup background with border
    M5.Display.fillRect(popupX, popupY, popupW, popupH, TFT_WHITE);
    M5.Display.drawRect(popupX, popupY, popupW, popupH, TFT_BLACK);
    M5.Display.drawRect(popupX + 1, popupY + 1, popupW - 2, popupH - 2, TFT_BLACK);

    // Draw header with word
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(popupX + 15, popupY + 15);
    M5.Display.print(word);

    // Draw separator
    M5.Display.drawLine(popupX + 10, popupY + 50, popupX + popupW - 10, popupY + 50, TFT_DARKGRAY);

    // Draw entries (max 3)
    int entryY = popupY + 60;
    int maxEntries = min(3, (int)_dictResults.size());

    for (int i = 0; i < maxEntries; i++) {
        drawDictionaryEntry(_dictResults[i], entryY);
        entryY += 70;
    }

    // Draw close button
    int btnW = 100;
    int btnH = 40;
    int btnX = popupX + (popupW - btnW) / 2;
    int btnY = popupY + popupH - btnH - 15;

    M5.Display.fillRoundRect(btnX, btnY, btnW, btnH, 5, TFT_DARKGRAY);
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_WHITE);
    String closeText = "닫기";
    int textW = M5.Display.textWidth(closeText.c_str());
    M5.Display.setCursor(btnX + (btnW - textW) / 2, btnY + (btnH - 16) / 2);
    M5.Display.print(closeText);

    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.endWrite();
}

void CopyScreen::drawDictionaryEntry(const DictEntry& entry, int y) {
    int popupW = 450;
    int popupX = (SCREEN_WIDTH - popupW) / 2;
    int contentX = popupX + 15;
    int contentW = popupW - 30;

    // Reading (ひらがな)
    if (entry.reading.length() > 0 && entry.reading != entry.word) {
        M5.Display.setFont(&fonts::efontJA_16);
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(contentX, y);
        M5.Display.print("[");
        M5.Display.print(entry.reading);
        M5.Display.print("]");
    }

    // Part of speech
    if (entry.partOfSpeech.length() > 0) {
        M5.Display.setFont(&fonts::efontKR_12);
        M5.Display.setTextColor(TFT_DARKGRAY);
        int posX = contentX + 150;
        M5.Display.setCursor(posX, y + 4);
        M5.Display.print(entry.partOfSpeech);
    }

    // Meaning
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(contentX, y + 25);

    // Truncate if too long
    String meaning = entry.meanings;
    if (M5.Display.textWidth(meaning.c_str()) > contentW) {
        while (meaning.length() > 0 && M5.Display.textWidth((meaning + "...").c_str()) > contentW) {
            meaning = meaning.substring(0, meaning.length() - 1);
        }
        meaning += "...";
    }
    M5.Display.print(meaning);

    // Draw separator line
    M5.Display.drawLine(contentX, y + 55, contentX + contentW, y + 55, TFT_LIGHTGREY);
}
