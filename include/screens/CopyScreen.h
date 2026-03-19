#pragma once

#include "BaseScreen.h"
#include "Config.h"
#include "EpubParser.h"
#include "TextLayout.h"
#include "PopupMenu.h"
#include <vector>

/**
 * Reading mode for EPUB content
 */
enum class ReadingMode {
    DAILY,       // Date-based: Day N = Chapter offset + N - 1
    SEQUENTIAL   // Progress-based: Continue from last read chapter
};

/**
 * 필사 (Copy/Transcription) Screen
 *
 * Displays daily content from EPUB or 365.txt file.
 * Uses page-based navigation instead of scroll for e-ink optimization.
 *
 * Reading Modes:
 * - DAILY: Auto-detected when chapters have date patterns (1月1日, Day 1)
 * - SEQUENTIAL: Fallback when no date pattern found, tracks reading progress
 *
 * Content Source Priority:
 * 1. EPUB file matching config.dailyEpub in /books/
 * 2. Auto-detect EPUB with "365" in name
 * 3. Plain text 365.txt file
 */
class CopyScreen : public BaseScreen {
public:
    CopyScreen();

    // ============================================
    // IScreen Implementation
    // ============================================

    const char* getName() const override { return "Copy"; }

    void onEnter() override;
    void draw() override;
    bool handleTouchStart(int x, int y) override;
    bool handleTouchMove(int x, int y) override;
    bool handleTouchEnd() override;

    // ============================================
    // Content Loading
    // ============================================

    /**
     * Load content for today based on RTC date
     * @return true if content was loaded successfully
     */
    bool loadTodayContent();

private:
    // Content data
    String _date;
    String _title;
    String _author;
    std::vector<String> _paragraphs;

    // Page navigation
    int _currentPage;
    int _totalPages;
    std::vector<int> _pageBreaks;  // Index into _paragraphs for each page start

    // EPUB support
    EpubParser _epubParser;
    String _epubPath;
    int _chapterOffset;       // Offset for first content chapter
    ReadingMode _readingMode; // Daily or Sequential
    int _currentChapter;      // Current chapter index (for Sequential mode)

    // Text selection
    TextLayout _textLayout;
    PopupMenu _popupMenu;

    // Touch tracking for long press and drag selection
    int _touchStartX;
    int _touchStartY;
    unsigned long _touchStartTime;
    bool _touchInContentArea;
    bool _inDragSelection;      // True when dragging to extend selection
    WordInfo _dragStartWord;    // First word selected when drag started
    static constexpr unsigned long LONG_PRESS_MS = 400;  // 400ms for long press
    static constexpr int TOUCH_MOVE_THRESHOLD = 20;      // Max movement for long press

    // Drawing methods
    void drawHeader();
    void drawPageContent();
    void drawNavigation();
    void drawEmptyState();

    // Helper functions
    int getDayOfYear(int month, int day);
    void calculatePages();

    // Content loading helpers
    bool loadFromEpub(int dayOfYear);
    bool loadFromTextFile(int dayOfYear);
    String findEpubFile();
    void parseChapterContent(const String& text);
    int detectChapterOffset();   // Find offset, sets _readingMode
    void loadReadingProgress();  // Load saved chapter/page for Sequential
    void saveReadingProgress();  // Save current chapter/page

    // Text selection helpers
    void handleWordSelection(int x, int y);
    void handlePopupAction(PopupMenu::Action action);
    void saveToVocabulary(const String& word);
    void saveToGrammar(const String& text);

    // UI feedback
    void showToast(const char* message);
};
