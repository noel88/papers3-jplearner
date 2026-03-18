#pragma once

#include "BaseScreen.h"
#include "EpubParser.h"
#include <vector>

/**
 * Read Screen - EPUB book reader
 *
 * Two modes:
 * 1. Book Selection: Grid view showing available EPUBs with covers and progress
 * 2. Reading: Chapter content with page navigation
 */

// Book metadata for grid display
struct BookInfo {
    String filename;
    String title;
    String author;
    int currentChapter;
    int totalChapters;
    float progress;  // 0.0 - 1.0
    String lastRead; // Date string
};

// Reading progress storage
struct ReadingProgress {
    String filename;
    int currentChapter;
    int currentPage;
    String lastRead;
};

class ReadScreen : public BaseScreen {
public:
    ReadScreen();

    // ============================================
    // IScreen Implementation
    // ============================================
    const char* getName() const override { return "Read"; }

    void onEnter() override;
    void onExit() override;
    void draw() override;
    bool handleTouchStart(int x, int y) override;

private:
    // ============================================
    // State
    // ============================================
    enum class Mode {
        BookSelection,
        Reading
    };

    Mode _mode;

    // Book selection state
    std::vector<BookInfo> _books;
    int _selectedBookIndex;
    int _gridScrollOffset;

    // Reading state
    EpubParser _epubParser;
    String _currentBookPath;
    int _currentChapter;
    int _currentPage;
    int _totalPages;
    std::vector<String> _paragraphs;
    std::vector<int> _pageBreaks;

    // ============================================
    // Book Selection Mode
    // ============================================
    void scanBooks();
    void drawBookSelection();
    bool handleBookSelectionTouch(int x, int y);
    void openBook(int index);

    // ============================================
    // Reading Mode
    // ============================================
    void loadChapter(int chapterIndex);
    void calculatePages();
    void drawReading();
    void drawReadingHeader();
    void drawReadingContent();
    void drawReadingNavigation();
    bool handleReadingTouch(int x, int y);
    void closeBook();

    // ============================================
    // Progress Management
    // ============================================
    void loadProgress();
    void saveProgress();
    ReadingProgress getProgressForBook(const String& filename);
    void updateProgressForBook(const String& filename, int chapter, int page);

    // ============================================
    // Layout Constants
    // ============================================
    static constexpr int GRID_COLS = 4;
    static constexpr int GRID_ROWS = 2;
    static constexpr int BOOK_WIDTH = 200;
    static constexpr int BOOK_HEIGHT = 180;
    static constexpr int BOOK_SPACING = 20;
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int NAV_HEIGHT = 50;
    static constexpr int TAB_BAR_HEIGHT = 60;
};
