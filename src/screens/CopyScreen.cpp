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
      _totalPages(0),
      _chapterOffset(-1),
      _readingMode(ReadingMode::DAILY),
      _currentChapter(0) {
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

int CopyScreen::detectChapterOffset() {
    // Search first 15 chapters to find where Day 1 content starts
    // Must verify by checking consecutive chapters have sequential dates

    int totalChapters = _epubParser.getChapterCount();
    int searchLimit = min(15, totalChapters);

    Serial.println("CopyScreen: Auto-detecting chapter offset...");

    for (int i = 0; i < searchLimit; i++) {
        String text = _epubParser.getChapterText(i);
        if (text.length() == 0) continue;

        // Only check header area (first 150 chars) - date should be at top
        String header = text.substring(0, min(150, (int)text.length()));

        bool foundDay1 = false;

        // Japanese date patterns for Jan 1 (must be in header position)
        if (header.indexOf("1月1日") >= 0 ||
            header.indexOf("１月１日") >= 0) {
            foundDay1 = true;
        }

        // Day number patterns
        if (header.indexOf("第1日") >= 0 ||
            header.indexOf("第１日") >= 0 ||
            header.indexOf("Day 1") >= 0 ||
            header.indexOf("DAY 1") >= 0 ||
            header.indexOf("001") >= 0) {
            foundDay1 = true;
        }

        if (foundDay1) {
            // Verify: next chapter should have Day 2 pattern
            if (i + 1 < totalChapters) {
                String nextText = _epubParser.getChapterText(i + 1);
                String nextHeader = nextText.substring(0, min(150, (int)nextText.length()));

                bool hasDay2 = (nextHeader.indexOf("1月2日") >= 0 ||
                               nextHeader.indexOf("１月２日") >= 0 ||
                               nextHeader.indexOf("第2日") >= 0 ||
                               nextHeader.indexOf("第２日") >= 0 ||
                               nextHeader.indexOf("Day 2") >= 0 ||
                               nextHeader.indexOf("DAY 2") >= 0 ||
                               nextHeader.indexOf("002") >= 0);

                if (hasDay2) {
                    Serial.printf("CopyScreen: Verified Day 1 at chapter %d (Day 2 at %d)\n", i, i + 1);
                    _readingMode = ReadingMode::DAILY;
                    return i;
                } else {
                    Serial.printf("CopyScreen: Found Day 1 pattern at %d but no Day 2 at %d, skipping\n", i, i + 1);
                    continue;
                }
            } else {
                // Can't verify, but found pattern - use it
                Serial.printf("CopyScreen: Found Day 1 at chapter %d (no verification)\n", i);
                _readingMode = ReadingMode::DAILY;
                return i;
            }
        }
    }

    // Check file naming pattern as fallback
    const auto& chapters = _epubParser.getChapters();
    for (int i = 0; i < searchLimit; i++) {
        String href = chapters[i].href;
        href.toLowerCase();

        // Check for "001" pattern and verify "002" in next
        if ((href.indexOf("001.") >= 0 || href.indexOf("_001") >= 0) && i + 1 < totalChapters) {
            String nextHref = chapters[i + 1].href;
            nextHref.toLowerCase();
            if (nextHref.indexOf("002.") >= 0 || nextHref.indexOf("_002") >= 0) {
                Serial.printf("CopyScreen: Found sequential numbering starting at chapter %d\n", i);
                _readingMode = ReadingMode::DAILY;
                return i;
            }
        }
    }

    // No daily pattern found - switch to Sequential mode
    Serial.println("CopyScreen: No daily pattern found, using Sequential mode");
    _readingMode = ReadingMode::SEQUENTIAL;
    return 0;  // Start from chapter 0
}

bool CopyScreen::loadFromEpub(int dayOfYear) {
    // Find the EPUB file that should be used
    String targetPath = findEpubFile();
    if (targetPath.length() == 0) {
        Serial.println("CopyScreen: No EPUB found");
        return false;
    }

    // If a different EPUB is requested, close current and reset offset
    if (_epubParser.isOpen() && _epubPath != targetPath) {
        Serial.printf("CopyScreen: Switching EPUB from %s to %s\n",
                      _epubPath.c_str(), targetPath.c_str());
        _epubParser.close();
        _chapterOffset = -1;  // Reset offset for new book
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

        // Auto-detect chapter offset and reading mode
        _chapterOffset = detectChapterOffset();
        Serial.printf("CopyScreen: offset=%d, mode=%s\n",
                      _chapterOffset,
                      _readingMode == ReadingMode::DAILY ? "DAILY" : "SEQUENTIAL");

        // Load saved progress for Sequential mode
        if (_readingMode == ReadingMode::SEQUENTIAL) {
            loadReadingProgress();
        }
    }

    // Calculate chapter index based on reading mode
    int chapterIndex;
    if (_readingMode == ReadingMode::DAILY) {
        // Daily mode: Day N = offset + (N - 1)
        chapterIndex = _chapterOffset + (dayOfYear - 1);
        Serial.printf("CopyScreen: DAILY mode - dayOfYear=%d, chapterIndex=%d\n",
                      dayOfYear, chapterIndex);
    } else {
        // Sequential mode: use saved chapter
        chapterIndex = _currentChapter;
        Serial.printf("CopyScreen: SEQUENTIAL mode - currentChapter=%d\n", chapterIndex);
    }

    // Bounds check
    if (chapterIndex < 0 || chapterIndex >= _epubParser.getChapterCount()) {
        Serial.printf("CopyScreen: Chapter %d out of range (total: %d)\n",
                      chapterIndex, _epubParser.getChapterCount());
        return false;
    }

    // Get chapter text
    const auto& chapters = _epubParser.getChapters();
    Serial.printf("CopyScreen: Loading chapter[%d]: %s\n",
                  chapterIndex, chapters[chapterIndex].href.c_str());

    String chapterText = _epubParser.getChapterText(chapterIndex);
    if (chapterText.length() == 0) {
        Serial.println("CopyScreen: Empty chapter content");
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

    Serial.printf("CopyScreen: Final title='%s', author='%s'\n",
                  _title.c_str(), _author.c_str());

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

            // Draw highlight if this line contains selection
            if (_textLayout.hasSelection()) {
                _textLayout.drawHighlight();
            }

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

    // Draw popup menu if visible
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
    int contentY = HEADER_HEIGHT;

    // Handle popup menu touch first
    if (_popupMenu.isVisible()) {
        PopupMenu::Action action = _popupMenu.handleTouch(x, y);
        if (action != PopupMenu::NONE) {
            handlePopupAction(action);
            return true;
        }
    }

    // Content area touch - text selection
    if (y >= contentY && y < navY) {
        handleWordSelection(x, y);
        return true;
    }

    // Navigation area touch
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

        // Redraw to show highlight and popup
        M5.Display.setEpdMode(epd_mode_t::epd_fast);
        M5.Display.startWrite();
        drawPageContent();
        M5.Display.endWrite();

        Serial.printf("Selected word: '%s'\n", word.text.c_str());
    } else {
        // Touch on empty area - clear selection
        if (_textLayout.hasSelection() || _popupMenu.isVisible()) {
            _textLayout.clearSelection();
            _popupMenu.hide();

            // Redraw to remove highlight
            M5.Display.setEpdMode(epd_mode_t::epd_fast);
            M5.Display.startWrite();
            drawPageContent();
            M5.Display.endWrite();
        }
    }
}

void CopyScreen::handlePopupAction(PopupMenu::Action action) {
    String selectedText = _popupMenu.getSelectedText();

    switch (action) {
        case PopupMenu::SEARCH:
            // TODO: Phase 4 - Dictionary search
            Serial.printf("Search: %s\n", selectedText.c_str());
            break;

        case PopupMenu::SAVE:
            saveToVocabulary(selectedText);
            Serial.printf("Saved to vocabulary: %s\n", selectedText.c_str());
            break;

        case PopupMenu::GRAMMAR:
            saveToGrammar(selectedText);
            Serial.printf("Saved to grammar: %s\n", selectedText.c_str());
            break;

        case PopupMenu::CANCEL:
        default:
            break;
    }

    // Clear selection and hide popup
    _textLayout.clearSelection();
    _popupMenu.hide();

    // Redraw
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    M5.Display.startWrite();
    drawPageContent();
    M5.Display.endWrite();
}

void CopyScreen::saveToVocabulary(const String& word) {
    // Save word to vocabulary file in LittleFS
    File file = LittleFS.open("/userdata/vocabulary.txt", FILE_APPEND);
    if (file) {
        file.println(word);
        file.close();
    }
}

void CopyScreen::saveToGrammar(const String& text) {
    // Save text to grammar patterns file in LittleFS
    File file = LittleFS.open("/userdata/grammar.txt", FILE_APPEND);
    if (file) {
        file.println(text);
        file.close();
    }
}

void CopyScreen::loadReadingProgress() {
    // Load saved reading progress from LittleFS
    // Format: <epub_filename>:<chapter>:<page>
    File file = LittleFS.open("/userdata/reading_progress.txt", FILE_READ);
    if (!file) {
        Serial.println("CopyScreen: No saved reading progress");
        _currentChapter = _chapterOffset;  // Start from first content chapter
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
            Serial.printf("CopyScreen: Loaded progress - chapter=%d, page=%d\n",
                          _currentChapter, _currentPage);
        } else {
            Serial.printf("CopyScreen: Different book, starting fresh (saved=%s, current=%s)\n",
                          savedEpub.c_str(), epubName.c_str());
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
        Serial.printf("CopyScreen: Saved progress - chapter=%d, page=%d\n",
                      _currentChapter, _currentPage);
    }
}
