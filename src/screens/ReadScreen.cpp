#include "screens/ReadScreen.h"
#include "Config.h"
#include <M5Unified.h>
#include <SD.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Progress file path
static const char* PROGRESS_FILE = "/userdata/reading_progress.json";

ReadScreen::ReadScreen()
    : BaseScreen(),
      _mode(Mode::BookSelection),
      _selectedBookIndex(-1),
      _gridScrollOffset(0),
      _currentChapter(0),
      _currentPage(0),
      _totalPages(0) {
}

void ReadScreen::onEnter() {
    BaseScreen::onEnter();

    if (_mode == Mode::BookSelection) {
        scanBooks();
        loadProgress();
    }

    requestRedraw();
}

void ReadScreen::onExit() {
    if (_mode == Mode::Reading) {
        saveProgress();
    }
    BaseScreen::onExit();
}

// ============================================
// Book Scanning
// ============================================
void ReadScreen::scanBooks() {
    _books.clear();

    File dir = SD.open(DIR_BOOKS);
    if (!dir) {
        Serial.println("ReadScreen: Cannot open books directory");
        return;
    }

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        entry.close();

        if (!isFile || !name.endsWith(".epub")) {
            continue;
        }

        BookInfo book;
        book.filename = name;
        book.currentChapter = 0;
        book.totalChapters = 0;
        book.progress = 0.0f;
        book.lastRead = "";

        // Try to get metadata from EPUB
        String fullPath = String(DIR_BOOKS) + "/" + name;
        EpubParser parser;
        if (parser.open(fullPath)) {
            const EpubMetadata& meta = parser.getMetadata();
            book.title = meta.title.length() > 0 ? meta.title : name;
            book.author = meta.author;
            book.totalChapters = parser.getChapterCount();
            parser.close();
        } else {
            book.title = name;
            book.title.replace(".epub", "");
        }

        _books.push_back(book);
        Serial.printf("ReadScreen: Found book: %s (%d chapters)\n",
                      book.title.c_str(), book.totalChapters);
    }
    dir.close();

    Serial.printf("ReadScreen: Found %d books\n", _books.size());
}

// ============================================
// Progress Management
// ============================================
void ReadScreen::loadProgress() {
    File f = LittleFS.open(PROGRESS_FILE, "r");
    if (!f) {
        Serial.println("ReadScreen: No progress file found");
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, f);
    f.close();

    if (error) {
        Serial.printf("ReadScreen: Failed to parse progress: %s\n", error.c_str());
        return;
    }

    JsonArray booksArray = doc["books"].as<JsonArray>();
    for (JsonObject bookObj : booksArray) {
        String filename = bookObj["file"] | "";

        // Find matching book in our list
        for (auto& book : _books) {
            if (book.filename == filename) {
                book.currentChapter = bookObj["currentChapter"] | 0;
                book.lastRead = bookObj["lastRead"] | "";

                // Calculate progress
                if (book.totalChapters > 0) {
                    book.progress = (float)book.currentChapter / book.totalChapters;
                }
                break;
            }
        }
    }

    Serial.println("ReadScreen: Progress loaded");
}

void ReadScreen::saveProgress() {
    // Ensure directory exists
    if (!LittleFS.exists("/userdata")) {
        LittleFS.mkdir("/userdata");
    }

    JsonDocument doc;
    JsonArray booksArray = doc["books"].to<JsonArray>();

    for (const auto& book : _books) {
        if (book.currentChapter > 0 || book.lastRead.length() > 0) {
            JsonObject bookObj = booksArray.add<JsonObject>();
            bookObj["file"] = book.filename;
            bookObj["currentChapter"] = book.currentChapter;
            bookObj["lastRead"] = book.lastRead;
        }
    }

    // Add current reading book if in reading mode
    if (_mode == Mode::Reading && _selectedBookIndex >= 0) {
        // Update the book info
        _books[_selectedBookIndex].currentChapter = _currentChapter;

        // Get current date
        auto dt = M5.Rtc.getDateTime();
        char dateStr[16];
        sprintf(dateStr, "%04d-%02d-%02d", dt.date.year, dt.date.month, dt.date.date);
        _books[_selectedBookIndex].lastRead = String(dateStr);
    }

    File f = LittleFS.open(PROGRESS_FILE, "w");
    if (!f) {
        Serial.println("ReadScreen: Failed to open progress file for writing");
        return;
    }

    serializeJson(doc, f);
    f.close();

    Serial.println("ReadScreen: Progress saved");
}

// ============================================
// Drawing - Main
// ============================================
void ReadScreen::draw() {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    M5.Display.startWrite();

    if (_mode == Mode::BookSelection) {
        drawBookSelection();
    } else {
        drawReading();
    }

    M5.Display.endWrite();
}

// ============================================
// Book Selection Mode
// ============================================
void ReadScreen::drawBookSelection() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Clear screen
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    // Header
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(PAD_X, 15);
    M5.Display.print("책 선택");

    // Book count
    M5.Display.setTextColor(TFT_DARKGRAY);
    String countStr = String(_books.size()) + "권";
    int countW = M5.Display.textWidth(countStr.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - countW, 15);
    M5.Display.print(countStr);

    // Separator
    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 1, TFT_BLACK);

    if (_books.empty()) {
        // Empty state
        M5.Display.setTextColor(TFT_BLACK);
        int centerY = availableHeight / 2;
        M5.Display.setCursor(PAD_X, centerY - 20);
        M5.Display.println("책이 없습니다");
        M5.Display.setCursor(PAD_X, centerY + 20);
        M5.Display.println("/books/ 폴더에 EPUB 파일을 추가하세요");
        return;
    }

    // Draw book grid
    M5.Display.setFont(&fonts::efontJA_24);

    int startX = (SCREEN_WIDTH - (GRID_COLS * BOOK_WIDTH + (GRID_COLS - 1) * BOOK_SPACING)) / 2;
    int startY = HEADER_HEIGHT + 20;

    for (int i = 0; i < _books.size() && i < GRID_COLS * GRID_ROWS; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;

        int x = startX + col * (BOOK_WIDTH + BOOK_SPACING);
        int y = startY + row * (BOOK_HEIGHT + BOOK_SPACING);

        const BookInfo& book = _books[i];

        // Book cover placeholder (rectangle)
        M5.Display.drawRect(x, y, BOOK_WIDTH, BOOK_HEIGHT - 40, TFT_BLACK);
        M5.Display.fillRect(x + 1, y + 1, BOOK_WIDTH - 2, BOOK_HEIGHT - 42, TFT_LIGHTGRAY);

        // Book icon in center
        int iconX = x + BOOK_WIDTH / 2 - 20;
        int iconY = y + (BOOK_HEIGHT - 40) / 2 - 15;
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(iconX, iconY);
        M5.Display.print("EPUB");

        // Title (truncated)
        M5.Display.setTextColor(TFT_BLACK);
        String displayTitle = book.title;
        int maxTitleWidth = BOOK_WIDTH - 10;
        while (M5.Display.textWidth(displayTitle.c_str()) > maxTitleWidth && displayTitle.length() > 6) {
            displayTitle = displayTitle.substring(0, displayTitle.length() - 4) + "..";
        }

        int titleY = y + BOOK_HEIGHT - 35;
        M5.Display.setCursor(x + 5, titleY);
        M5.Display.print(displayTitle);

        // Progress bar
        int barY = y + BOOK_HEIGHT - 10;
        int barW = BOOK_WIDTH - 10;
        M5.Display.drawRect(x + 5, barY, barW, 8, TFT_DARKGRAY);

        int fillW = (int)(barW * book.progress);
        if (fillW > 0) {
            M5.Display.fillRect(x + 6, barY + 1, fillW - 2, 6, TFT_BLACK);
        }
    }
}

bool ReadScreen::handleBookSelectionTouch(int x, int y) {
    if (_books.empty()) {
        return false;
    }

    int startX = (SCREEN_WIDTH - (GRID_COLS * BOOK_WIDTH + (GRID_COLS - 1) * BOOK_SPACING)) / 2;
    int startY = HEADER_HEIGHT + 20;

    for (int i = 0; i < _books.size() && i < GRID_COLS * GRID_ROWS; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;

        int bx = startX + col * (BOOK_WIDTH + BOOK_SPACING);
        int by = startY + row * (BOOK_HEIGHT + BOOK_SPACING);

        if (x >= bx && x < bx + BOOK_WIDTH && y >= by && y < by + BOOK_HEIGHT) {
            openBook(i);
            return true;
        }
    }

    return false;
}

void ReadScreen::openBook(int index) {
    if (index < 0 || index >= _books.size()) {
        return;
    }

    _selectedBookIndex = index;
    const BookInfo& book = _books[index];

    String fullPath = String(DIR_BOOKS) + "/" + book.filename;

    if (!_epubParser.open(fullPath)) {
        Serial.printf("ReadScreen: Failed to open book: %s\n", fullPath.c_str());
        return;
    }

    _currentBookPath = fullPath;
    _currentChapter = book.currentChapter;
    _currentPage = 0;

    loadChapter(_currentChapter);

    _mode = Mode::Reading;
    requestRedraw();

    Serial.printf("ReadScreen: Opened book: %s at chapter %d\n",
                  book.title.c_str(), _currentChapter);
}

// ============================================
// Reading Mode
// ============================================
void ReadScreen::loadChapter(int chapterIndex) {
    _paragraphs.clear();
    _pageBreaks.clear();
    _currentPage = 0;

    if (chapterIndex < 0 || chapterIndex >= _epubParser.getChapterCount()) {
        return;
    }

    _currentChapter = chapterIndex;
    String content = _epubParser.getChapterText(chapterIndex);

    // Split into paragraphs
    int start = 0;
    int len = content.length();

    while (start < len) {
        int end = content.indexOf('\n', start);
        if (end < 0) end = len;

        String para = content.substring(start, end);
        para.trim();

        if (para.length() > 0) {
            _paragraphs.push_back(para);
        }

        start = end + 1;
    }

    calculatePages();

    Serial.printf("ReadScreen: Loaded chapter %d: %d paragraphs, %d pages\n",
                  chapterIndex, _paragraphs.size(), _totalPages);
}

void ReadScreen::calculatePages() {
    _pageBreaks.clear();
    _pageBreaks.push_back(0);

    if (_paragraphs.empty()) {
        _totalPages = 0;
        return;
    }

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2;
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT - PAD_Y * 2;

    int currentHeight = 0;
    int paraIndex = 0;
    int lineSpacing = 8;
    int paraSpacing = 24;

    while (paraIndex < _paragraphs.size()) {
        const String& para = _paragraphs[paraIndex];

        int paraWidth = M5.Display.textWidth(para.c_str());
        int lines = (paraWidth / contentW) + 1;
        int paraHeight = lines * (fontH + lineSpacing) + paraSpacing;

        if (currentHeight + paraHeight > contentH && currentHeight > 0) {
            _pageBreaks.push_back(paraIndex);
            currentHeight = paraHeight;
        } else {
            currentHeight += paraHeight;
        }

        paraIndex++;
    }

    _totalPages = _pageBreaks.size();
}

void ReadScreen::drawReading() {
    drawReadingHeader();
    drawReadingContent();
    drawReadingNavigation();
}

void ReadScreen::drawReadingHeader() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    // Back button
    M5.Display.setCursor(PAD_X, 15);
    M5.Display.print("< 목록");

    // Chapter info
    if (_selectedBookIndex >= 0 && _selectedBookIndex < _books.size()) {
        const auto& chapters = _epubParser.getChapters();
        String chapterTitle = "";
        if (_currentChapter < chapters.size()) {
            chapterTitle = chapters[_currentChapter].title;
        }

        // Truncate if needed
        int maxWidth = 500;
        while (M5.Display.textWidth(chapterTitle.c_str()) > maxWidth && chapterTitle.length() > 10) {
            chapterTitle = chapterTitle.substring(0, chapterTitle.length() - 4) + "..";
        }

        int titleW = M5.Display.textWidth(chapterTitle.c_str());
        M5.Display.setCursor((SCREEN_WIDTH - titleW) / 2, 15);
        M5.Display.print(chapterTitle);
    }

    // Chapter navigation
    String chapterNav = String(_currentChapter + 1) + "/" + String(_epubParser.getChapterCount());
    int navW = M5.Display.textWidth(chapterNav.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - navW, 15);
    M5.Display.print(chapterNav);

    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 1, TFT_BLACK);
}

void ReadScreen::drawReadingContent() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT + PAD_Y;
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT;

    M5.Display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, contentH, TFT_WHITE);

    if (_currentPage >= _totalPages || _pageBreaks.empty()) {
        return;
    }

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();
    int contentW = SCREEN_WIDTH - PAD_X * 2;
    int maxY = availableHeight - NAV_HEIGHT - PAD_Y;
    int lineSpacing = 8;
    int paraSpacing = 24;

    int startPara = _pageBreaks[_currentPage];
    int endPara = (_currentPage + 1 < _totalPages) ? _pageBreaks[_currentPage + 1] : _paragraphs.size();

    int y = contentY;

    for (int i = startPara; i < endPara && y < maxY; i++) {
        const String& para = _paragraphs[i];

        int bytePos = 0;
        int byteLen = para.length();

        while (bytePos < byteLen && y < maxY) {
            int lineEnd = bytePos;

            while (lineEnd < byteLen) {
                int charLen = 1;
                uint8_t c = para[lineEnd];
                if (c >= 0xF0) charLen = 4;
                else if (c >= 0xE0) charLen = 3;
                else if (c >= 0xC0) charLen = 2;

                String sub = para.substring(bytePos, lineEnd + charLen);
                if (M5.Display.textWidth(sub.c_str()) > contentW) break;

                lineEnd += charLen;
            }

            if (lineEnd == bytePos) lineEnd += 1;

            M5.Display.setCursor(PAD_X, y);
            M5.Display.print(para.substring(bytePos, lineEnd));

            bytePos = lineEnd;
            y += fontH + lineSpacing;
        }

        y += paraSpacing - lineSpacing;
    }
}

void ReadScreen::drawReadingNavigation() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int navY = availableHeight - NAV_HEIGHT;

    M5.Display.fillRect(0, navY, SCREEN_WIDTH, NAV_HEIGHT, TFT_WHITE);
    M5.Display.drawLine(PAD_X, navY, SCREEN_WIDTH - PAD_X, navY, TFT_LIGHTGRAY);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);

    int buttonY = navY + (NAV_HEIGHT - 30) / 2;

    // Previous page/chapter
    bool canGoPrev = _currentPage > 0 || _currentChapter > 0;
    if (canGoPrev) {
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(PAD_X + 20, buttonY);
        if (_currentPage > 0) {
            M5.Display.print("< 이전");
        } else {
            M5.Display.print("<< 이전장");
        }
    }

    // Page indicator
    M5.Display.setTextColor(TFT_DARKGRAY);
    String pageInfo = String(_currentPage + 1) + " / " + String(_totalPages);
    int pageInfoW = M5.Display.textWidth(pageInfo.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - pageInfoW) / 2, buttonY);
    M5.Display.print(pageInfo);

    // Next page/chapter
    bool canGoNext = _currentPage < _totalPages - 1 || _currentChapter < _epubParser.getChapterCount() - 1;
    if (canGoNext) {
        M5.Display.setTextColor(TFT_BLACK);
        String nextText;
        if (_currentPage < _totalPages - 1) {
            nextText = "다음 >";
        } else {
            nextText = "다음장 >>";
        }
        int nextW = M5.Display.textWidth(nextText.c_str());
        M5.Display.setCursor(SCREEN_WIDTH - PAD_X - nextW - 20, buttonY);
        M5.Display.print(nextText);
    }
}

bool ReadScreen::handleReadingTouch(int x, int y) {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Header touch - back button
    if (y < HEADER_HEIGHT && x < 150) {
        closeBook();
        return true;
    }

    int navY = availableHeight - NAV_HEIGHT;

    // Navigation area
    if (y >= navY && y < availableHeight) {
        // Left - previous
        if (x < SCREEN_WIDTH / 3) {
            if (_currentPage > 0) {
                _currentPage--;
            } else if (_currentChapter > 0) {
                loadChapter(_currentChapter - 1);
                _currentPage = _totalPages - 1;  // Go to last page of previous chapter
            }

            M5.Display.startWrite();
            drawReadingContent();
            drawReadingNavigation();
            M5.Display.endWrite();
            return false;  // Prevent main loop refresh
        }
        // Right - next
        else if (x > SCREEN_WIDTH * 2 / 3) {
            if (_currentPage < _totalPages - 1) {
                _currentPage++;
            } else if (_currentChapter < _epubParser.getChapterCount() - 1) {
                loadChapter(_currentChapter + 1);
            }

            M5.Display.startWrite();
            drawReadingContent();
            drawReadingNavigation();
            M5.Display.endWrite();
            return false;
        }
    }

    return false;
}

void ReadScreen::closeBook() {
    saveProgress();

    // Update book info
    if (_selectedBookIndex >= 0 && _selectedBookIndex < _books.size()) {
        _books[_selectedBookIndex].currentChapter = _currentChapter;
        if (_epubParser.getChapterCount() > 0) {
            _books[_selectedBookIndex].progress =
                (float)_currentChapter / _epubParser.getChapterCount();
        }
    }

    _epubParser.close();
    _currentBookPath = "";
    _selectedBookIndex = -1;
    _paragraphs.clear();
    _pageBreaks.clear();

    _mode = Mode::BookSelection;
    requestRedraw();
}

// ============================================
// Touch Handling
// ============================================
bool ReadScreen::handleTouchStart(int x, int y) {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Ignore touches in tab bar area
    if (y >= availableHeight) {
        return false;
    }

    if (_mode == Mode::BookSelection) {
        return handleBookSelectionTouch(x, y);
    } else {
        return handleReadingTouch(x, y);
    }
}
