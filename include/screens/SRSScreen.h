#pragma once

#include "screens/BaseScreen.h"
#include "SRSManager.h"
#include <vector>

/**
 * SRSScreen - Spaced Repetition Learning Screen
 *
 * Shows flashcards for review with SM-2 algorithm.
 * Supports both word and grammar cards.
 */
class SRSScreen : public BaseScreen {
public:
    SRSScreen();

    const char* getName() const override { return "SRS"; }

    void onEnter() override;
    void draw() override;
    bool handleTouchStart(int x, int y) override;

private:
    // Card state
    enum class CardState {
        FRONT,      // Showing question
        BACK        // Showing answer with response buttons
    };

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

    // UI methods
    void drawFront();
    void drawBack();
    void drawComplete();
    void drawStats();
    void drawResponseButtons();

    // Card management
    void loadCards();
    void nextCard();
    void processResponse(SRSResponse response);

    // Get interval text for display
    String getIntervalText(SRSResponse response);

    // Layout constants
    static constexpr int HEADER_HEIGHT = 50;
    static constexpr int BUTTON_HEIGHT = 60;
    static constexpr int BUTTON_WIDTH = 110;
    static constexpr int BUTTON_SPACING = 15;
    static constexpr int TAB_BAR_HEIGHT = 60;
};
