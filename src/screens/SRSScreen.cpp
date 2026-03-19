#include "screens/SRSScreen.h"
#include "Config.h"
#include "FontManager.h"
#include "UIHelpers.h"
#include <M5Unified.h>

SRSScreen::SRSScreen()
    : BaseScreen(),
      _cardState(CardState::FRONT),
      _currentCard(nullptr),
      _cardIndex(0),
      _reviewedCount(0),
      _totalDue(0),
      _totalNew(0),
      _cardType("") {
}

void SRSScreen::onEnter() {
    BaseScreen::onEnter();
    loadCards();
    _cardState = CardState::FRONT;
    _reviewedCount = 0;
    nextCard();
    requestRedraw();
}

void SRSScreen::loadCards() {
    SRSManager& srs = SRSManager::instance();

    _dueCards = srs.getDueCards(_cardType);
    _newCards = srs.getNewCards(_cardType, 20);  // Limit new cards per session

    _totalDue = _dueCards.size();
    _totalNew = _newCards.size();
    _cardIndex = 0;
}

void SRSScreen::nextCard() {
    // Priority: due cards first, then new cards
    if (_cardIndex < (int)_dueCards.size()) {
        _currentCard = _dueCards[_cardIndex];
    } else {
        int newIndex = _cardIndex - _dueCards.size();
        if (newIndex < (int)_newCards.size()) {
            _currentCard = _newCards[newIndex];
        } else {
            _currentCard = nullptr;  // No more cards
        }
    }

    _cardState = CardState::FRONT;
}

void SRSScreen::processResponse(SRSResponse response) {
    if (!_currentCard) return;

    // Check if this was a new card (due == 0 before processing)
    bool isNew = (_currentCard->due == 0);
    bool isCorrect = (response == SRSResponse::GOOD || response == SRSResponse::EASY);

    SRSManager& srs = SRSManager::instance();
    srs.processResponse(_currentCard->id, response);

    // Record stats
    srs.recordReview(isCorrect, isNew);

    _reviewedCount++;
    _cardIndex++;
    nextCard();
    requestRedraw();
}

void SRSScreen::draw() {
    M5.Display.setEpdMode(epd_mode_t::epd_fast);
    M5.Display.startWrite();

    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Clear content area
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    if (!_currentCard) {
        drawComplete();
    } else if (_cardState == CardState::FRONT) {
        drawFront();
    } else {
        drawBack();
    }

    drawStats();

    M5.Display.endWrite();
}

void SRSScreen::drawStats() {
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);

    int remaining = (_dueCards.size() + _newCards.size()) - _cardIndex;
    String statsText = "남은 카드: " + String(remaining);
    if (_totalNew > 0) {
        statsText += " (신규: " + String(_totalNew) + ")";
    }

    M5.Display.setCursor(PAD_X, 15);
    M5.Display.print(statsText);

    // Separator line
    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 5, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 5, TFT_LIGHTGRAY);
}

void SRSScreen::drawFront() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT;
    int contentH = availableHeight - HEADER_HEIGHT;

    // Card type indicator
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    String typeText = (_currentCard->type == "word") ? "[단어]" : "[문형]";
    int typeW = M5.Display.textWidth(typeText.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - typeW, 15);
    M5.Display.print(typeText);

    // Main content - the word/phrase
    FontManager& fm = FontManager::instance();
    String front = _currentCard->front;

    // Calculate text size based on content length
    int fontSize = 48;
    if (front.length() > 30) fontSize = 32;
    else if (front.length() > 15) fontSize = 36;

    // Draw centered
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(fontSize / 24.0);
    M5.Display.setTextColor(TFT_BLACK);

    int textW = M5.Display.textWidth(front.c_str());
    int textX = (SCREEN_WIDTH - textW) / 2;
    int textY = contentY + (contentH - BUTTON_HEIGHT - 80) / 2;

    M5.Display.setCursor(textX, textY);
    M5.Display.print(front);

    // Reset text size
    M5.Display.setTextSize(1.0);

    // "Show Answer" button
    int btnW = 200;
    int btnH = 50;
    int btnX = (SCREEN_WIDTH - btnW) / 2;
    int btnY = availableHeight - BUTTON_HEIGHT - 30;

    M5.Display.fillRoundRect(btnX, btnY, btnW, btnH, 8, TFT_WHITE);
    M5.Display.drawRoundRect(btnX, btnY, btnW, btnH, 8, TFT_BLACK);
    M5.Display.drawRoundRect(btnX + 1, btnY + 1, btnW - 2, btnH - 2, 7, TFT_BLACK);

    M5.Display.setFont(&fonts::efontKR_24);
    String btnText = "답 보기";
    int btnTextW = M5.Display.textWidth(btnText.c_str());
    M5.Display.setCursor(btnX + (btnW - btnTextW) / 2, btnY + (btnH - 24) / 2);
    M5.Display.print(btnText);
}

void SRSScreen::drawBack() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT + 20;

    // Card type indicator
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    String typeText = (_currentCard->type == "word") ? "[단어]" : "[문형]";
    int typeW = M5.Display.textWidth(typeText.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - typeW, 15);
    M5.Display.print(typeText);

    // Front text (question)
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.5);
    M5.Display.setTextColor(TFT_BLACK);

    String front = _currentCard->front;
    int frontW = M5.Display.textWidth(front.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - frontW) / 2, contentY);
    M5.Display.print(front);

    M5.Display.setTextSize(1.0);
    contentY += 60;

    // Back text (answer) if available
    if (_currentCard->back.length() > 0) {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextColor(TFT_DARKGRAY);

        String back = _currentCard->back;
        int backW = M5.Display.textWidth(back.c_str());
        M5.Display.setCursor((SCREEN_WIDTH - backW) / 2, contentY);
        M5.Display.print(back);

        contentY += 40;
    }

    // Separator
    M5.Display.drawLine(PAD_X + 50, contentY + 20, SCREEN_WIDTH - PAD_X - 50, contentY + 20, TFT_LIGHTGRAY);

    // Draw response buttons
    drawResponseButtons();
}

void SRSScreen::drawResponseButtons() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int btnY = availableHeight - BUTTON_HEIGHT - 40;

    // Calculate total width of buttons
    int totalWidth = 4 * BUTTON_WIDTH + 3 * BUTTON_SPACING;
    int startX = (SCREEN_WIDTH - totalWidth) / 2;

    struct ButtonInfo {
        const char* label;
        const char* interval;
        SRSResponse response;
    };

    ButtonInfo buttons[] = {
        {"Again", getIntervalText(SRSResponse::AGAIN).c_str(), SRSResponse::AGAIN},
        {"Hard", getIntervalText(SRSResponse::HARD).c_str(), SRSResponse::HARD},
        {"Good", getIntervalText(SRSResponse::GOOD).c_str(), SRSResponse::GOOD},
        {"Easy", getIntervalText(SRSResponse::EASY).c_str(), SRSResponse::EASY}
    };

    for (int i = 0; i < 4; i++) {
        int btnX = startX + i * (BUTTON_WIDTH + BUTTON_SPACING);

        // Button background
        M5.Display.fillRoundRect(btnX, btnY, BUTTON_WIDTH, BUTTON_HEIGHT, 6, TFT_WHITE);
        M5.Display.drawRoundRect(btnX, btnY, BUTTON_WIDTH, BUTTON_HEIGHT, 6, TFT_BLACK);

        // Button label
        M5.Display.setFont(&fonts::efontKR_16);
        M5.Display.setTextColor(TFT_BLACK);
        int labelW = M5.Display.textWidth(buttons[i].label);
        M5.Display.setCursor(btnX + (BUTTON_WIDTH - labelW) / 2, btnY + 10);
        M5.Display.print(buttons[i].label);

        // Interval text
        M5.Display.setFont(&fonts::efontKR_16);
        M5.Display.setTextColor(TFT_DARKGRAY);
        String interval = getIntervalText(buttons[i].response);
        int intervalW = M5.Display.textWidth(interval.c_str());
        M5.Display.setCursor(btnX + (BUTTON_WIDTH - intervalW) / 2, btnY + 35);
        M5.Display.print(interval);
    }
}

String SRSScreen::getIntervalText(SRSResponse response) {
    if (!_currentCard) return "";

    SRSManager& srs = SRSManager::instance();
    int interval = 0;

    // For new cards
    if (_currentCard->reps == 0) {
        switch (response) {
            case SRSResponse::AGAIN: return "1분";
            case SRSResponse::HARD: return "6분";
            case SRSResponse::GOOD: return "1일";
            case SRSResponse::EASY: return "4일";
        }
    }

    // For review cards
    int currentInterval = _currentCard->interval;
    if (currentInterval < 1) currentInterval = 1;

    switch (response) {
        case SRSResponse::AGAIN:
            return "1분";
        case SRSResponse::HARD:
            interval = (int)(currentInterval * 1.2);
            break;
        case SRSResponse::GOOD:
            interval = (int)(currentInterval * _currentCard->ef);
            break;
        case SRSResponse::EASY:
            interval = (int)(currentInterval * _currentCard->ef * 1.3);
            break;
    }

    if (interval < 1) interval = 1;

    if (interval == 1) return "1일";
    else if (interval < 30) return String(interval) + "일";
    else if (interval < 365) return String(interval / 30) + "개월";
    else return String(interval / 365) + "년";
}

void SRSScreen::drawComplete() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int centerY = availableHeight / 2;

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    String msg1 = "학습 완료!";
    int msg1W = M5.Display.textWidth(msg1.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - msg1W) / 2, centerY - 40);
    M5.Display.print(msg1);

    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);

    String msg2 = "오늘 " + String(_reviewedCount) + "장 복습 완료";
    int msg2W = M5.Display.textWidth(msg2.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - msg2W) / 2, centerY + 10);
    M5.Display.print(msg2);
}

bool SRSScreen::handleTouchStart(int x, int y) {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Ignore if no card
    if (!_currentCard) {
        return false;
    }

    // Front state - check "Show Answer" button
    if (_cardState == CardState::FRONT) {
        int btnW = 200;
        int btnH = 50;
        int btnX = (SCREEN_WIDTH - btnW) / 2;
        int btnY = availableHeight - BUTTON_HEIGHT - 30;

        if (x >= btnX && x < btnX + btnW && y >= btnY && y < btnY + btnH) {
            _cardState = CardState::BACK;
            requestRedraw();
            return true;
        }

        // Also allow tap anywhere to show answer
        if (y > HEADER_HEIGHT && y < availableHeight - BUTTON_HEIGHT) {
            _cardState = CardState::BACK;
            requestRedraw();
            return true;
        }
    }

    // Back state - check response buttons
    if (_cardState == CardState::BACK) {
        int btnY = availableHeight - BUTTON_HEIGHT - 40;
        int totalWidth = 4 * BUTTON_WIDTH + 3 * BUTTON_SPACING;
        int startX = (SCREEN_WIDTH - totalWidth) / 2;

        if (y >= btnY && y < btnY + BUTTON_HEIGHT) {
            for (int i = 0; i < 4; i++) {
                int btnX = startX + i * (BUTTON_WIDTH + BUTTON_SPACING);
                if (x >= btnX && x < btnX + BUTTON_WIDTH) {
                    processResponse((SRSResponse)i);
                    return true;
                }
            }
        }
    }

    return false;
}
