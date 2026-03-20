#pragma once

#include "BaseScreen.h"
#include "SRSManager.h"

/**
 * StatsScreen - Learning Statistics Dashboard
 *
 * Displays:
 * - Study streak (consecutive days)
 * - Today's progress (reviews, new, accuracy)
 * - Card overview (word/grammar counts, mastery rate)
 * - Weekly chart (last 7 days)
 * - Upcoming reviews (tomorrow, this week)
 */
class StatsScreen : public BaseScreen {
public:
    StatsScreen();

    const char* getName() const override { return "Stats"; }

    void onEnter() override;
    void draw() override;
    bool handleTouchStart(int x, int y) override;

private:
    void drawStreakCard(int x, int y, int w, int h);
    void drawTodayCard(int x, int y, int w, int h);
    void drawOverviewCard(int x, int y, int w, int h);
    void drawWeeklyChart(int x, int y, int w, int h);
    void drawUpcomingCard(int x, int y, int w, int h);

    // Helper to draw progress bar
    void drawProgressBar(int x, int y, int w, int h, float progress);

    // Cached stats
    SRSStats _stats;
    int _wordCount;
    int _grammarCount;
    int _wordMastered;
    int _grammarMastered;
    int _tomorrowDue;
    int _weekDue;
};
