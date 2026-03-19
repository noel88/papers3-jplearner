#include "screens/StatsScreen.h"
#include <M5Unified.h>

// Layout constants
static constexpr int CARD_PAD = 15;
static constexpr int CARD_RADIUS = 8;

StatsScreen::StatsScreen()
    : BaseScreen(),
      _wordCount(0),
      _grammarCount(0),
      _wordMastered(0),
      _grammarMastered(0),
      _tomorrowDue(0),
      _weekDue(0) {
}

void StatsScreen::onEnter() {
    BaseScreen::onEnter();

    // Refresh stats
    SRSManager& srs = SRSManager::instance();
    _stats = srs.getStats();

    _wordCount = srs.getCardCount("word");
    _grammarCount = srs.getCardCount("grammar");
    _wordMastered = srs.getMasteredCount("word");
    _grammarMastered = srs.getMasteredCount("grammar");

    _tomorrowDue = srs.getDueCountForDay(1);

    // Calculate week due (days 1-7)
    _weekDue = 0;
    for (int i = 1; i <= 7; i++) {
        _weekDue += srs.getDueCountForDay(i);
    }
}

void StatsScreen::draw() {
    int y = HEADER_HEIGHT + CARD_PAD;
    int contentWidth = SCREEN_WIDTH - 2 * PAD_X;

    // Title
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(PAD_X, y);
    M5.Display.print("학습 통계");
    y += 40;

    // Row 1: Streak | Today | Overview (3 columns)
    int row1H = 120;
    int col3W = (contentWidth - 2 * CARD_PAD) / 3;

    drawStreakCard(PAD_X, y, col3W, row1H);
    drawTodayCard(PAD_X + col3W + CARD_PAD, y, col3W, row1H);
    drawOverviewCard(PAD_X + 2 * (col3W + CARD_PAD), y, col3W, row1H);

    y += row1H + CARD_PAD;

    // Row 2: Weekly Chart | Upcoming (2 columns)
    int row2H = 150;
    int col2W = (contentWidth - CARD_PAD) / 2;

    drawWeeklyChart(PAD_X, y, col2W + 80, row2H);
    drawUpcomingCard(PAD_X + col2W + CARD_PAD + 80, y, col2W - 80, row2H);
}

void StatsScreen::drawStreakCard(int x, int y, int w, int h) {
    // Background
    M5.Display.fillRoundRect(x, y, w, h, CARD_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(x, y, w, h, CARD_RADIUS, TFT_DARKGRAY);

    // Fire emoji and streak count
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor(x + 12, y + 12);
    M5.Display.print("연속 학습");

    // Big streak number
    M5.Display.setFont(&fonts::Font7);
    M5.Display.setTextColor(TFT_BLACK);
    String streakStr = String(_stats.streak);
    int textW = M5.Display.textWidth(streakStr.c_str());
    M5.Display.setCursor(x + (w - textW) / 2, y + 40);
    M5.Display.print(streakStr);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setCursor(x + (w - textW) / 2 + textW + 5, y + 50);
    M5.Display.print("일");
}

void StatsScreen::drawTodayCard(int x, int y, int w, int h) {
    // Background
    M5.Display.fillRoundRect(x, y, w, h, CARD_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(x, y, w, h, CARD_RADIUS, TFT_DARKGRAY);

    // Title
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor(x + 12, y + 12);
    M5.Display.print("오늘 학습");

    // Stats
    M5.Display.setFont(&fonts::efontKR_14);
    M5.Display.setTextColor(TFT_BLACK);

    int lineY = y + 45;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("복습: %d", _stats.todayReviews);

    lineY += 22;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("새카드: %d", _stats.todayNew);

    // Accuracy
    lineY += 22;
    int accuracy = 0;
    if (_stats.todayReviews > 0) {
        accuracy = (_stats.todayCorrect * 100) / _stats.todayReviews;
    }
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("정답률: %d%%", accuracy);
}

void StatsScreen::drawOverviewCard(int x, int y, int w, int h) {
    // Background
    M5.Display.fillRoundRect(x, y, w, h, CARD_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(x, y, w, h, CARD_RADIUS, TFT_DARKGRAY);

    // Title
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor(x + 12, y + 12);
    M5.Display.print("전체 카드");

    // Stats
    M5.Display.setFont(&fonts::efontKR_14);
    M5.Display.setTextColor(TFT_BLACK);

    int lineY = y + 45;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("단어: %d", _wordCount);

    lineY += 22;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("문형: %d", _grammarCount);

    // Mastery progress bar
    lineY += 28;
    int totalCards = _wordCount + _grammarCount;
    int totalMastered = _wordMastered + _grammarMastered;
    float masteryRate = totalCards > 0 ? (float)totalMastered / totalCards : 0;

    drawProgressBar(x + 12, lineY, w - 24, 12, masteryRate);

    M5.Display.setFont(&fonts::efontKR_12);
    M5.Display.setCursor(x + 12, lineY + 16);
    M5.Display.printf("숙지율: %d%%", (int)(masteryRate * 100));
}

void StatsScreen::drawWeeklyChart(int x, int y, int w, int h) {
    // Background
    M5.Display.fillRoundRect(x, y, w, h, CARD_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(x, y, w, h, CARD_RADIUS, TFT_DARKGRAY);

    // Title
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor(x + 12, y + 12);
    M5.Display.print("주간 학습량");

    // Find max for scaling
    int maxReviews = 1;
    for (int i = 0; i < 7; i++) {
        if (_stats.weeklyReviews[i] > maxReviews) {
            maxReviews = _stats.weeklyReviews[i];
        }
    }

    // Draw bars
    int chartX = x + 20;
    int chartY = y + 45;
    int chartW = w - 40;
    int chartH = h - 80;
    int barW = chartW / 7 - 8;

    const char* days[] = {"월", "화", "수", "목", "금", "토", "일"};

    for (int i = 0; i < 7; i++) {
        int barX = chartX + i * (chartW / 7) + 4;
        float ratio = (float)_stats.weeklyReviews[i] / maxReviews;
        int barH = (int)(chartH * ratio);
        if (barH < 2 && _stats.weeklyReviews[i] > 0) barH = 2;

        // Bar
        int barY = chartY + chartH - barH;
        if (_stats.weeklyReviews[i] > 0) {
            M5.Display.fillRect(barX, barY, barW, barH, TFT_DARKGRAY);
        } else {
            M5.Display.drawRect(barX, barY + barH - 2, barW, 2, TFT_LIGHTGREY);
        }

        // Day label
        M5.Display.setFont(&fonts::efontKR_12);
        M5.Display.setTextColor(TFT_DARKGRAY);
        int labelW = M5.Display.textWidth(days[i]);
        M5.Display.setCursor(barX + (barW - labelW) / 2, chartY + chartH + 5);
        M5.Display.print(days[i]);
    }
}

void StatsScreen::drawUpcomingCard(int x, int y, int w, int h) {
    // Background
    M5.Display.fillRoundRect(x, y, w, h, CARD_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(x, y, w, h, CARD_RADIUS, TFT_DARKGRAY);

    // Title
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    M5.Display.setCursor(x + 12, y + 12);
    M5.Display.print("예정 복습");

    // Tomorrow
    M5.Display.setFont(&fonts::efontKR_14);
    M5.Display.setTextColor(TFT_BLACK);

    int lineY = y + 50;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("내일: %d개", _tomorrowDue);

    lineY += 30;
    M5.Display.setCursor(x + 12, lineY);
    M5.Display.printf("이번주: %d개", _weekDue);

    // Today's due count
    int todayDue = SRSManager::instance().getDueCount();
    lineY += 30;
    if (todayDue > 0) {
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(x + 12, lineY);
        M5.Display.printf("오늘 남은: %d개", todayDue);
    } else {
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(x + 12, lineY);
        M5.Display.print("오늘 완료!");
    }
}

void StatsScreen::drawProgressBar(int x, int y, int w, int h, float progress) {
    // Background
    M5.Display.fillRect(x, y, w, h, TFT_LIGHTGREY);

    // Progress
    int progressW = (int)(w * progress);
    if (progressW > 0) {
        M5.Display.fillRect(x, y, progressW, h, TFT_DARKGRAY);
    }

    // Border
    M5.Display.drawRect(x, y, w, h, TFT_DARKGRAY);
}

bool StatsScreen::handleTouchStart(int x, int y) {
    // Refresh on any touch
    onEnter();
    return true;
}
