#pragma once

#include "screens/BaseScreen.h"
#include "SRSManager.h"
#include "GrammarManager.h"
#include <vector>

/**
 * SRSScreen - Spaced Repetition Learning Screen
 *
 * Shows flashcards for review with SM-2 algorithm.
 * Supports both word and grammar cards.
 * Grammar mode includes search and add functionality.
 */
class SRSScreen : public BaseScreen {
public:
    SRSScreen();

    const char* getName() const override { return "SRS"; }

    void onEnter() override;
    void draw() override;
    bool handleTouchStart(int x, int y) override;

    // Set card type filter
    void setCardType(const String& type) { _cardType = type; }

private:
    // Screen mode
    enum class ScreenMode {
        REVIEW,     // Card review mode
        SEARCH,     // Grammar search mode
        RESULTS     // Search results
    };

    // Card state
    enum class CardState {
        FRONT,      // Showing question
        BACK        // Showing answer with response buttons
    };

    ScreenMode _mode;
    CardState _cardState;
    SRSCard* _currentCard;
    std::vector<SRSCard*> _dueCards;
    std::vector<SRSCard*> _newCards;
    int _cardIndex;

    // Session stats
    int _reviewedCount;
    int _totalDue;
    int _totalNew;

    // Card type filter ("word", "grammar", or "" for all)
    String _cardType;

    // Grammar search state
    std::vector<GrammarItem> _searchResults;
    int _searchScrollOffset;
    String _currentLevel;  // Filter by level ("N5", "N4", etc. or "" for all)

    // UI methods - Review mode
    void drawFront();
    void drawBack();
    void drawComplete();
    void drawStats();
    void drawResponseButtons();

    // UI methods - Grammar search mode
    void drawSearch();
    void drawSearchResults();
    void drawLevelButtons();

    // Card management
    void loadCards();
    void nextCard();
    void processResponse(SRSResponse response);

    // Grammar search
    void searchByLevel(const String& level);
    void addGrammarToSRS(const GrammarItem& item);

    // Get interval text for display
    String getIntervalText(SRSResponse response);

    // Touch handlers
    bool handleReviewTouch(int x, int y);
    bool handleSearchTouch(int x, int y);
    bool handleResultsTouch(int x, int y);

    // Layout constants
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int BUTTON_HEIGHT = 60;
    static constexpr int BUTTON_WIDTH = 110;
    static constexpr int BUTTON_SPACING = 15;
    static constexpr int TAB_BAR_HEIGHT = 60;
    static constexpr int ITEM_HEIGHT = 70;
    static constexpr int LEVEL_BTN_WIDTH = 80;
    static constexpr int LEVEL_BTN_HEIGHT = 45;
};
