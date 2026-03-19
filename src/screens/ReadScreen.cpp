#include "screens/ReadScreen.h"
#include "Config.h"
#include "FontManager.h"
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
      _chapterListScroll(0),
      _currentChapter(0),
      _currentPage(0),
      _totalPages(0) {
}

void ReadScreen::onEnter() {
    BaseScreen::onEnter();

    // Load custom font settings
    FontManager& fm = FontManager::instance();
    if (config.primaryFont.length() > 0) {
        fm.setPrimaryFont(config.primaryFont);
        fm.setFontSize(config.fontSizePt);
    }

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

    switch (_mode) {
        case Mode::BookSelection:
            drawBookSelection();
            break;
        case Mode::ChapterList:
            drawChapterList();
            break;
        case Mode::Reading:
            drawReading();
            break;
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

    FontManager& fm = FontManager::instance();
    int fontH;
    int contentW = SCREEN_WIDTH - PAD_X * 2;

    if (fm.hasCustomFont()) {
        // Use custom font size for calculation
        fontH = config.fontSizePt + 4;  // Approximate line height
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        fontH = M5.Display.fontHeight();
    }

    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT - PAD_Y * 2;

    int currentHeight = 0;
    int paraIndex = 0;
    int lineSpacing = 8;
    int paraSpacing = 24;

    while (paraIndex < _paragraphs.size()) {
        const String& para = _paragraphs[paraIndex];

        int paraWidth = fm.hasCustomFont() ? fm.getTextWidth(para) : M5.Display.textWidth(para.c_str());
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

    // Back button (left)
    M5.Display.setCursor(PAD_X, 15);
    M5.Display.print("< 목록");

    // TOC button (right side, before chapter nav)
    String tocBtn = "목차";
    int tocW = M5.Display.textWidth(tocBtn.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - 150, 15);
    M5.Display.print(tocBtn);

    // Chapter navigation (far right)
    String chapterNav = String(_currentChapter + 1) + "/" + String(_epubParser.getChapterCount());
    int navW = M5.Display.textWidth(chapterNav.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - navW, 15);
    M5.Display.print(chapterNav);

    // Chapter title (center)
    if (_selectedBookIndex >= 0 && _selectedBookIndex < (int)_books.size()) {
        const auto& chapters = _epubParser.getChapters();
        String chapterTitle = "";
        if (_currentChapter < (int)chapters.size()) {
            chapterTitle = chapters[_currentChapter].title;
        }

        // Truncate if needed
        int maxWidth = 400;
        while (M5.Display.textWidth(chapterTitle.c_str()) > maxWidth && chapterTitle.length() > 10) {
            chapterTitle = chapterTitle.substring(0, chapterTitle.length() - 4) + "..";
        }

        int titleW = M5.Display.textWidth(chapterTitle.c_str());
        M5.Display.setCursor((SCREEN_WIDTH - titleW) / 2, 15);
        M5.Display.print(chapterTitle);
    }

    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 1, TFT_BLACK);
}

void ReadScreen::drawReadingContent() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT + PAD_Y;
    int contentH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT;

    M5.Display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, contentH, TFT_WHITE);

    // Clear text layout for new page
    _textLayout.clear();

    if (_currentPage >= _totalPages || _pageBreaks.empty()) {
        return;
    }

    FontManager& fm = FontManager::instance();
    bool useCustomFont = fm.hasCustomFont();

    int fontH;
    if (useCustomFont) {
        fontH = config.fontSizePt + 4;
    } else {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        M5.Display.setTextColor(TFT_BLACK);
        fontH = M5.Display.fontHeight();
    }

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
                int subWidth = useCustomFont ? fm.getTextWidth(sub) : M5.Display.textWidth(sub.c_str());
                if (subWidth > contentW) break;

                lineEnd += charLen;
            }

            if (lineEnd == bytePos) lineEnd += 1;

            String lineText = para.substring(bytePos, lineEnd);
            int lineWidth = useCustomFont ? fm.getTextWidth(lineText) : M5.Display.textWidth(lineText.c_str());

            // Record line position for text selection
            _textLayout.addLine(i, bytePos, lineEnd, PAD_X, y, lineWidth, fontH + lineSpacing, lineText);

            // Draw highlight if this line contains selection
            if (_textLayout.hasSelection()) {
                _textLayout.drawHighlight();
            }

            if (useCustomFont) {
                fm.drawString(lineText, PAD_X, y);
            } else {
                M5.Display.setCursor(PAD_X, y);
                M5.Display.print(lineText);
            }

            bytePos = lineEnd;
            y += fontH + lineSpacing;
        }

        y += paraSpacing - lineSpacing;
    }

    // Draw popup menu if visible
    if (_popupMenu.isVisible()) {
        _popupMenu.draw();
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

    // Header touch
    if (y < HEADER_HEIGHT) {
        // Back button (left side)
        if (x < 150) {
            closeBook();
            return true;
        }
        // TOC button (right side, around x=750-850)
        if (x > SCREEN_WIDTH - 200 && x < SCREEN_WIDTH - 80) {
            _chapterListScroll = (_currentChapter / CHAPTERS_PER_PAGE) * CHAPTERS_PER_PAGE;
            _mode = Mode::ChapterList;
            requestRedraw();
            return true;
        }
    }

    // Content area touch - text selection
    if (y >= contentY && y < navY) {
        handleWordSelection(x, y);
        return true;
    }

    // Navigation area
    if (y >= navY && y < availableHeight) {
        // Clear any selection when navigating
        if (_textLayout.hasSelection()) {
            _textLayout.clearSelection();
            _popupMenu.hide();
        }

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

void ReadScreen::handleWordSelection(int x, int y) {
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
        drawReadingContent();
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
            drawReadingContent();
            M5.Display.endWrite();
        }
    }
}

void ReadScreen::handlePopupAction(PopupMenu::Action action) {
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
    drawReadingContent();
    M5.Display.endWrite();
}

void ReadScreen::saveToVocabulary(const String& word) {
    // Save word to vocabulary file in LittleFS
    File file = LittleFS.open("/userdata/vocabulary.txt", FILE_APPEND);
    if (file) {
        file.println(word);
        file.close();
    }
}

void ReadScreen::saveToGrammar(const String& text) {
    // Save text to grammar patterns file in LittleFS
    File file = LittleFS.open("/userdata/grammar.txt", FILE_APPEND);
    if (file) {
        file.println(text);
        file.close();
    }
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

    switch (_mode) {
        case Mode::BookSelection:
            return handleBookSelectionTouch(x, y);
        case Mode::ChapterList:
            return handleChapterListTouch(x, y);
        case Mode::Reading:
            return handleReadingTouch(x, y);
    }
    return false;
}

// ============================================
// Chapter List Mode
// ============================================
void ReadScreen::drawChapterList() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Clear screen
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    int fontH = M5.Display.fontHeight();

    // Header with book title and back button
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, TFT_WHITE);
    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 1, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 1, TFT_BLACK);

    // Back button (left)
    M5.Display.setCursor(PAD_X, (HEADER_HEIGHT - fontH) / 2);
    M5.Display.print("< 뒤로");

    // Title (center)
    String title = "목차";
    if (_epubParser.isOpen()) {
        const EpubMetadata& meta = _epubParser.getMetadata();
        if (meta.title.length() > 0) {
            title = meta.title;
            // Truncate if too long
            if (M5.Display.textWidth(title.c_str()) > 600) {
                while (M5.Display.textWidth(title.c_str()) > 550 && title.length() > 10) {
                    title = title.substring(0, title.length() - 4) + "...";
                }
            }
        }
    }
    int titleW = M5.Display.textWidth(title.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - titleW) / 2, (HEADER_HEIGHT - fontH) / 2);
    M5.Display.print(title);

    // Chapter list area
    int listY = HEADER_HEIGHT + 10;
    int listH = availableHeight - HEADER_HEIGHT - NAV_HEIGHT - 20;
    int itemH = 60;  // Height per chapter item

    int totalChapters = _epubParser.getChapterCount();
    int maxScroll = max(0, totalChapters - CHAPTERS_PER_PAGE);
    _chapterListScroll = constrain(_chapterListScroll, 0, maxScroll);

    const auto& chapters = _epubParser.getChapters();

    for (int i = 0; i < CHAPTERS_PER_PAGE; i++) {
        int chapterIdx = _chapterListScroll + i;
        if (chapterIdx >= totalChapters) break;

        int itemY = listY + i * itemH;

        // Highlight current chapter
        if (chapterIdx == _currentChapter) {
            M5.Display.fillRect(PAD_X - 5, itemY, SCREEN_WIDTH - PAD_X * 2 + 10, itemH - 5, TFT_LIGHTGRAY);
        }

        // Chapter number
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(PAD_X, itemY + 10);
        M5.Display.printf("%d.", chapterIdx + 1);

        // Chapter title
        M5.Display.setTextColor(TFT_BLACK);
        String chTitle = chapters[chapterIdx].title;
        if (chTitle.length() == 0 || chTitle.startsWith("Chapter ")) {
            // Try to get first line of content as title
            String text = _epubParser.getChapterText(chapterIdx);
            if (text.length() > 0) {
                int newline = text.indexOf('\n');
                if (newline > 0 && newline < 80) {
                    chTitle = text.substring(0, newline);
                    chTitle.trim();
                } else if (text.length() < 80) {
                    chTitle = text;
                    chTitle.trim();
                }
            }
            if (chTitle.length() == 0) {
                chTitle = "Chapter " + String(chapterIdx + 1);
            }
        }

        // Truncate long titles
        int maxTitleW = SCREEN_WIDTH - PAD_X * 2 - 80;
        while (M5.Display.textWidth(chTitle.c_str()) > maxTitleW && chTitle.length() > 10) {
            chTitle = chTitle.substring(0, chTitle.length() - 4) + "...";
        }

        M5.Display.setCursor(PAD_X + 60, itemY + 10);
        M5.Display.print(chTitle);

        // Separator line
        M5.Display.drawLine(PAD_X, itemY + itemH - 8, SCREEN_WIDTH - PAD_X, itemY + itemH - 8, TFT_LIGHTGRAY);
    }

    // Navigation (scroll indicators)
    int navY = availableHeight - NAV_HEIGHT;
    M5.Display.fillRect(0, navY, SCREEN_WIDTH, NAV_HEIGHT, TFT_WHITE);
    M5.Display.drawLine(PAD_X, navY, SCREEN_WIDTH - PAD_X, navY, TFT_LIGHTGRAY);

    M5.Display.setTextColor(TFT_BLACK);
    int navTextY = navY + (NAV_HEIGHT - fontH) / 2;

    // Previous page indicator
    if (_chapterListScroll > 0) {
        M5.Display.setCursor(PAD_X + 20, navTextY);
        M5.Display.print("< 이전");
    }

    // Page info
    int startNum = _chapterListScroll + 1;
    int endNum = min(_chapterListScroll + CHAPTERS_PER_PAGE, totalChapters);
    String pageInfo = String(startNum) + "-" + String(endNum) + " / " + String(totalChapters);
    int pageInfoW = M5.Display.textWidth(pageInfo.c_str());
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor((SCREEN_WIDTH - pageInfoW) / 2, navTextY);
    M5.Display.print(pageInfo);

    // Next page indicator
    if (_chapterListScroll + CHAPTERS_PER_PAGE < totalChapters) {
        M5.Display.setTextColor(TFT_BLACK);
        String nextText = "다음 >";
        int nextW = M5.Display.textWidth(nextText.c_str());
        M5.Display.setCursor(SCREEN_WIDTH - PAD_X - nextW - 20, navTextY);
        M5.Display.print(nextText);
    }
}

bool ReadScreen::handleChapterListTouch(int x, int y) {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int navY = availableHeight - NAV_HEIGHT;
    int listY = HEADER_HEIGHT + 10;
    int itemH = 60;

    // Back button (header area, left side)
    if (y < HEADER_HEIGHT && x < 150) {
        _mode = Mode::Reading;
        requestRedraw();
        return true;
    }

    // Navigation area
    if (y >= navY) {
        int totalChapters = _epubParser.getChapterCount();

        // Previous
        if (x < SCREEN_WIDTH / 3 && _chapterListScroll > 0) {
            _chapterListScroll -= CHAPTERS_PER_PAGE;
            if (_chapterListScroll < 0) _chapterListScroll = 0;
            requestRedraw();
            return true;
        }
        // Next
        else if (x > SCREEN_WIDTH * 2 / 3 && _chapterListScroll + CHAPTERS_PER_PAGE < totalChapters) {
            _chapterListScroll += CHAPTERS_PER_PAGE;
            requestRedraw();
            return true;
        }
        return false;
    }

    // Chapter item selection
    if (y >= listY && y < navY) {
        int itemIndex = (y - listY) / itemH;
        int chapterIdx = _chapterListScroll + itemIndex;

        if (chapterIdx >= 0 && chapterIdx < _epubParser.getChapterCount()) {
            goToChapter(chapterIdx);
            return true;
        }
    }

    return false;
}

void ReadScreen::goToChapter(int chapterIndex) {
    if (chapterIndex < 0 || chapterIndex >= _epubParser.getChapterCount()) {
        return;
    }

    _currentChapter = chapterIndex;
    _currentPage = 0;
    loadChapter(_currentChapter);
    saveProgress();

    _mode = Mode::Reading;
    requestRedraw();

    Serial.printf("ReadScreen: Jumped to chapter %d\n", chapterIndex);
}
