#pragma once

#include "BaseScreen.h"
#include "Config.h"
#include "EpubParser.h"
#include <vector>

/**
 * 필사 (Copy/Transcription) Screen
 *
 * Displays daily content from EPUB or 365.txt file.
 * Uses page-based navigation instead of scroll for e-ink optimization.
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
};
