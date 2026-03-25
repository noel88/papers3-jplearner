#include "screens/SRSScreen.h"
#include "Config.h"
#include "FontManager.h"
#include "UIHelpers.h"
#include <M5Unified.h>

SRSScreen::SRSScreen()
    : BaseScreen(),
      _mode(ScreenMode::REVIEW),
      _cardState(CardState::FRONT),
      _currentCard(nullptr),
      _cardIndex(0),
      _reviewedCount(0),
      _totalDue(0),
      _totalNew(0),
      _cardType(""),
      _searchScrollOffset(0),
      _currentLevel("N5") {
}

void SRSScreen::onEnter() {
    BaseScreen::onEnter();
    loadCards();
    _cardState = CardState::FRONT;
    _reviewedCount = 0;
    _searchScrollOffset = 0;

    // If grammar mode and no cards, show search
    if (_cardType == "grammar" && _dueCards.empty() && _newCards.empty()) {
        _mode = ScreenMode::SEARCH;
        searchByLevel(_currentLevel);
    } else {
        _mode = ScreenMode::REVIEW;
        nextCard();
    }

    requestRedraw();
}

void SRSScreen::loadCards() {
    SRSManager& srs = SRSManager::instance();

    _dueCards = srs.getDueCards(_cardType);
    _newCards = srs.getNewCards(_cardType, 20);

    _totalDue = _dueCards.size();
    _totalNew = _newCards.size();
    _cardIndex = 0;
}

void SRSScreen::nextCard() {
    if (_cardIndex < (int)_dueCards.size()) {
        _currentCard = _dueCards[_cardIndex];
    } else {
        int newIndex = _cardIndex - _dueCards.size();
        if (newIndex < (int)_newCards.size()) {
            _currentCard = _newCards[newIndex];
        } else {
            _currentCard = nullptr;
        }
    }
    _cardState = CardState::FRONT;
}

void SRSScreen::processResponse(SRSResponse response) {
    if (!_currentCard) return;

    bool isNew = (_currentCard->due == 0);
    bool isCorrect = (response == SRSResponse::GOOD || response == SRSResponse::EASY);

    SRSManager& srs = SRSManager::instance();
    srs.processResponse(_currentCard->id, response);
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
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, availableHeight, TFT_WHITE);

    switch (_mode) {
        case ScreenMode::SEARCH:
            drawSearch();
            break;
        case ScreenMode::RESULTS:
            drawSearchResults();
            break;
        case ScreenMode::REVIEW:
        default:
            if (!_currentCard) {
                drawComplete();
            } else if (_cardState == CardState::FRONT) {
                drawFront();
            } else {
                drawBack();
            }
            drawStats();
            break;
    }

    M5.Display.endWrite();
}

// ============================================
// Grammar Search UI
// ============================================

void SRSScreen::drawSearch() {
    int y = PAD_Y;

    // Title
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(UI::SIZE_TITLE);
    M5.Display.setTextColor(TFT_BLACK);
    UI::drawBoldText("문형 검색", PAD_X, y);

    // Card count
    M5.Display.setTextSize(UI::SIZE_SMALL);
    SRSManager& srs = SRSManager::instance();
    int grammarCount = srs.getCardCount("grammar");
    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "등록: %d개", grammarCount);
    int countW = M5.Display.textWidth(countBuf);
    M5.Display.setCursor(SCREEN_WIDTH - PAD_X - countW, y + 5);
    M5.Display.print(countBuf);

    M5.Display.drawLine(PAD_X, y + 40, SCREEN_WIDTH - PAD_X, y + 40, TFT_BLACK);
    y += 55;

    // Level buttons
    M5.Display.setTextSize(UI::SIZE_CONTENT);
    const char* levels[] = {"N5", "N4", "N3", "N2", "N1"};
    int btnGap = 15;
    int totalW = 5 * LEVEL_BTN_WIDTH + 4 * btnGap;
    int startX = (SCREEN_WIDTH - totalW) / 2;

    GrammarManager& gm = GrammarManager::instance();

    for (int i = 0; i < 5; i++) {
        int btnX = startX + i * (LEVEL_BTN_WIDTH + btnGap);
        bool selected = (_currentLevel == levels[i]);

        if (selected) {
            M5.Display.fillRoundRect(btnX, y, LEVEL_BTN_WIDTH, LEVEL_BTN_HEIGHT, 6, TFT_BLACK);
            M5.Display.setTextColor(TFT_WHITE);
        } else {
            M5.Display.drawRoundRect(btnX, y, LEVEL_BTN_WIDTH, LEVEL_BTN_HEIGHT, 6, TFT_BLACK);
            M5.Display.setTextColor(TFT_BLACK);
        }

        M5.Display.setFont(&fonts::efontKR_24);
        int textW = M5.Display.textWidth(levels[i]);
        UI::drawBoldText(levels[i], btnX + (LEVEL_BTN_WIDTH - textW) / 2, y + 10);

        // Count below button
        M5.Display.setFont(&fonts::Font2);
        M5.Display.setTextColor(TFT_DARKGRAY);
        int count = gm.getCountByLevel(levels[i]);
        char numBuf[8];
        snprintf(numBuf, sizeof(numBuf), "%d", count);
        int numW = M5.Display.textWidth(numBuf);
        M5.Display.setCursor(btnX + (LEVEL_BTN_WIDTH - numW) / 2, y + LEVEL_BTN_HEIGHT + 5);
        M5.Display.print(numBuf);
    }

    y += LEVEL_BTN_HEIGHT + 30;

    // Instructions
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(UI::SIZE_BODY);
    M5.Display.setTextColor(TFT_DARKGRAY);
    UI::drawBoldTextCentered("레벨을 선택하면 문형 목록이 표시됩니다", y);
    y += 35;
    UI::drawBoldTextCentered("터치하여 SRS 카드로 추가하세요", y);

    // Review button if has cards
    if (_totalDue > 0 || _totalNew > 0) {
        y += 50;
        int btnW = 250;
        int btnH = 50;
        int btnX = (SCREEN_WIDTH - btnW) / 2;

        M5.Display.fillRoundRect(btnX, y, btnW, btnH, 8, TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextSize(UI::SIZE_CONTENT);

        char btnText[32];
        snprintf(btnText, sizeof(btnText), "복습하기 (%d)", _totalDue + _totalNew);
        int textW = M5.Display.textWidth(btnText);
        UI::drawBoldText(btnText, btnX + (btnW - textW) / 2, y + 12);
    }

    M5.Display.setTextColor(TFT_BLACK);
}

void SRSScreen::drawSearchResults() {
    int y = PAD_Y;

    // Header with back button
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(UI::SIZE_TITLE);
    M5.Display.setTextColor(TFT_BLACK);

    char title[32];
    snprintf(title, sizeof(title), "< %s 문형 (%d)", _currentLevel.c_str(), (int)_searchResults.size());
    UI::drawBoldText(title, PAD_X, y);

    M5.Display.drawLine(PAD_X, y + 40, SCREEN_WIDTH - PAD_X, y + 40, TFT_BLACK);
    y += 50;

    // Results list
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int maxVisible = (availableHeight - y - 60) / ITEM_HEIGHT;

    M5.Display.setTextSize(UI::SIZE_BODY);

    SRSManager& srs = SRSManager::instance();

    for (int i = _searchScrollOffset; i < _searchResults.size() && i < _searchScrollOffset + maxVisible; i++) {
        const GrammarItem& item = _searchResults[i];
        int itemY = y + (i - _searchScrollOffset) * ITEM_HEIGHT;

        // Alternating background
        if ((i - _searchScrollOffset) % 2 == 0) {
            M5.Display.fillRect(0, itemY, SCREEN_WIDTH, ITEM_HEIGHT - 1, 0xF7BE);
        }

        // Check if already added
        bool alreadyAdded = srs.hasCard(item.pattern, "grammar");
        if (alreadyAdded) {
            M5.Display.fillRect(0, itemY, 6, ITEM_HEIGHT - 1, TFT_DARKGREEN);
        }

        // Pattern
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(PAD_X + 10, itemY + 8);
        M5.Display.print(item.pattern);

        // Meaning (truncated)
        M5.Display.setFont(&fonts::Font2);
        M5.Display.setTextColor(TFT_DARKGRAY);
        String meaning = item.meaning;
        if (meaning.length() > 50) {
            meaning = meaning.substring(0, 47) + "...";
        }
        M5.Display.setCursor(PAD_X + 10, itemY + 40);
        M5.Display.print(meaning);

        // Bottom border
        M5.Display.drawLine(PAD_X, itemY + ITEM_HEIGHT - 1,
                           SCREEN_WIDTH - PAD_X, itemY + ITEM_HEIGHT - 1, 0xDEDB);
    }

    // Scroll indicators
    if (_searchScrollOffset > 0) {
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(SCREEN_WIDTH - 50, y);
        M5.Display.print("▲");
    }
    if (_searchScrollOffset + maxVisible < _searchResults.size()) {
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(SCREEN_WIDTH - 50, availableHeight - 70);
        M5.Display.print("▼");
    }
}

void SRSScreen::searchByLevel(const String& level) {
    _currentLevel = level;
    _searchScrollOffset = 0;

    GrammarManager& gm = GrammarManager::instance();
    _searchResults = gm.getByLevel(level);

    _mode = ScreenMode::RESULTS;
    requestRedraw();
}

void SRSScreen::addGrammarToSRS(const GrammarItem& item) {
    SRSManager& srs = SRSManager::instance();

    // Check if already exists
    if (srs.hasCard(item.pattern, "grammar")) {
        Serial.printf("Grammar already exists: %s\n", item.pattern.c_str());
        return;
    }

    // Show progress indicator
    UI::showProgressIndicator("저장 중...");

    // Format back text for display
    // Format: [LEVEL] meaning
    //         例: example (exampleEn)
    String back = "[" + item.level + "] " + item.meaning;
    if (item.example.length() > 0) {
        back += "\n\n例: " + item.example;
        if (item.exampleEn.length() > 0) {
            back += "\n(" + item.exampleEn + ")";
        }
    }

    srs.addCard("grammar", item.pattern, back);
    srs.saveCards();

    Serial.printf("Added grammar: %s\n", item.pattern.c_str());

    // Reload cards count
    loadCards();
    requestRedraw();
}

// ============================================
// Review Mode UI (existing code)
// ============================================

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

    // Search button for grammar mode
    if (_cardType == "grammar") {
        M5.Display.setFont(&fonts::efontKR_16);
        M5.Display.setTextColor(TFT_BLACK);
        int btnX = SCREEN_WIDTH - PAD_X - 80;
        M5.Display.drawRect(btnX, 8, 70, 30, TFT_BLACK);
        M5.Display.setCursor(btnX + 10, 13);
        M5.Display.print("검색");
    }

    M5.Display.drawLine(PAD_X, HEADER_HEIGHT - 5, SCREEN_WIDTH - PAD_X, HEADER_HEIGHT - 5, TFT_LIGHTGRAY);
}

void SRSScreen::drawFront() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int contentY = HEADER_HEIGHT;
    int contentH = availableHeight - HEADER_HEIGHT;
    int margin = 40;

    // Card type indicator
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    String typeText = (_currentCard->type == "word") ? "[단어]" : "[문형]";
    int typeW = M5.Display.textWidth(typeText.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - margin - typeW, 15);
    M5.Display.print(typeText);

    // Main content
    String front = _currentCard->front;

    int fontSize = 48;
    if (front.length() > 30) fontSize = 32;
    else if (front.length() > 15) fontSize = 36;

    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(fontSize / 24.0);
    M5.Display.setTextColor(TFT_BLACK);

    int textW = M5.Display.textWidth(front.c_str());
    int textX = (SCREEN_WIDTH - textW) / 2;
    if (textX < margin) textX = margin;

    int textY = contentY + (contentH - BUTTON_HEIGHT - 80) / 2;
    M5.Display.setCursor(textX, textY);
    M5.Display.print(front);

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
    int margin = 40;
    int contentWidth = SCREEN_WIDTH - margin * 2;

    // Card type indicator
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);
    String typeText = (_currentCard->type == "word") ? "[단어]" : "[문형]";
    int typeW = M5.Display.textWidth(typeText.c_str());
    M5.Display.setCursor(SCREEN_WIDTH - margin - typeW, 15);
    M5.Display.print(typeText);

    // Front text
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.5);
    M5.Display.setTextColor(TFT_BLACK);

    String front = _currentCard->front;
    int frontW = M5.Display.textWidth(front.c_str());
    int frontX = (SCREEN_WIDTH - frontW) / 2;
    if (frontX < margin) frontX = margin;
    M5.Display.setCursor(frontX, contentY);
    M5.Display.print(front);

    M5.Display.setTextSize(1.0);
    contentY += 60;

    // Back text with word wrapping
    if (_currentCard->back.length() > 0) {
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextColor(TFT_BLACK);

        String back = _currentCard->back;
        int lineHeight = 32;
        int totalWidth = M5.Display.textWidth(back.c_str());

        if (totalWidth > contentWidth) {
            int pos = 0;
            int len = back.length();

            while (pos < len && contentY < availableHeight - BUTTON_HEIGHT - 80) {
                int lo = pos, hi = len;
                while (lo < hi) {
                    int mid = (lo + hi + 1) / 2;
                    while (mid < len && (back[mid] & 0xC0) == 0x80) mid++;

                    String testLine = back.substring(pos, mid);
                    if (M5.Display.textWidth(testLine.c_str()) <= contentWidth) {
                        lo = mid;
                    } else {
                        hi = mid - 1;
                    }
                }

                int lineEnd = lo;
                if (lineEnd == pos) {
                    lineEnd = pos + 1;
                    while (lineEnd < len && (back[lineEnd] & 0xC0) == 0x80) lineEnd++;
                }

                String line = back.substring(pos, lineEnd);
                int lineW = M5.Display.textWidth(line.c_str());
                int lineX = (SCREEN_WIDTH - lineW) / 2;
                if (lineX < margin) lineX = margin;

                M5.Display.setCursor(lineX, contentY);
                M5.Display.print(line);

                contentY += lineHeight;
                pos = lineEnd;
            }
        } else {
            int backX = (SCREEN_WIDTH - totalWidth) / 2;
            if (backX < margin) backX = margin;
            M5.Display.setCursor(backX, contentY);
            M5.Display.print(back);
            contentY += lineHeight;
        }

        contentY += 10;
    }

    M5.Display.drawLine(margin + 30, contentY + 10, SCREEN_WIDTH - margin - 30, contentY + 10, TFT_LIGHTGRAY);

    drawResponseButtons();
}

void SRSScreen::drawResponseButtons() {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int btnY = availableHeight - BUTTON_HEIGHT - 40;

    int totalWidth = 4 * BUTTON_WIDTH + 3 * BUTTON_SPACING;
    int startX = (SCREEN_WIDTH - totalWidth) / 2;

    struct ButtonInfo {
        const char* label;
        SRSResponse response;
    };

    ButtonInfo buttons[] = {
        {"Again", SRSResponse::AGAIN},
        {"Hard", SRSResponse::HARD},
        {"Good", SRSResponse::GOOD},
        {"Easy", SRSResponse::EASY}
    };

    for (int i = 0; i < 4; i++) {
        int btnX = startX + i * (BUTTON_WIDTH + BUTTON_SPACING);

        M5.Display.fillRoundRect(btnX, btnY, BUTTON_WIDTH, BUTTON_HEIGHT, 6, TFT_WHITE);
        M5.Display.drawRoundRect(btnX, btnY, BUTTON_WIDTH, BUTTON_HEIGHT, 6, TFT_BLACK);

        M5.Display.setFont(&fonts::efontKR_16);
        M5.Display.setTextColor(TFT_BLACK);
        int labelW = M5.Display.textWidth(buttons[i].label);
        M5.Display.setCursor(btnX + (BUTTON_WIDTH - labelW) / 2, btnY + 10);
        M5.Display.print(buttons[i].label);

        M5.Display.setFont(&fonts::efontKR_16);
        M5.Display.setTextColor(TFT_BLACK);
        String interval = getIntervalText(buttons[i].response);
        int intervalW = M5.Display.textWidth(interval.c_str());
        M5.Display.setCursor(btnX + (BUTTON_WIDTH - intervalW) / 2, btnY + 35);
        M5.Display.print(interval);
    }
}

String SRSScreen::getIntervalText(SRSResponse response) {
    if (!_currentCard) return "";

    if (_currentCard->reps == 0) {
        switch (response) {
            case SRSResponse::AGAIN: return "1분";
            case SRSResponse::HARD: return "6분";
            case SRSResponse::GOOD: return "1일";
            case SRSResponse::EASY: return "4일";
        }
    }

    int currentInterval = _currentCard->interval;
    if (currentInterval < 1) currentInterval = 1;
    int interval = 0;

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
    M5.Display.setCursor((SCREEN_WIDTH - msg1W) / 2, centerY - 60);
    M5.Display.print(msg1);

    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_DARKGRAY);

    String msg2 = "오늘 " + String(_reviewedCount) + "장 복습 완료";
    int msg2W = M5.Display.textWidth(msg2.c_str());
    M5.Display.setCursor((SCREEN_WIDTH - msg2W) / 2, centerY - 20);
    M5.Display.print(msg2);

    // Show "Add Grammar" button for grammar mode
    if (_cardType == "grammar") {
        int btnW = 200;
        int btnH = 50;
        int btnX = (SCREEN_WIDTH - btnW) / 2;
        int btnY = centerY + 30;

        M5.Display.fillRoundRect(btnX, btnY, btnW, btnH, 8, TFT_BLACK);
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setFont(&fonts::efontKR_24);

        String btnText = "문형 추가";
        int btnTextW = M5.Display.textWidth(btnText.c_str());
        M5.Display.setCursor(btnX + (btnW - btnTextW) / 2, btnY + 13);
        M5.Display.print(btnText);
    }
}

// ============================================
// Touch Handling
// ============================================

bool SRSScreen::handleTouchStart(int x, int y) {
    switch (_mode) {
        case ScreenMode::SEARCH:
            return handleSearchTouch(x, y);
        case ScreenMode::RESULTS:
            return handleResultsTouch(x, y);
        case ScreenMode::REVIEW:
        default:
            return handleReviewTouch(x, y);
    }
}

bool SRSScreen::handleSearchTouch(int x, int y) {
    int btnY = PAD_Y + 55;

    // Level buttons
    if (y >= btnY && y < btnY + LEVEL_BTN_HEIGHT) {
        const char* levels[] = {"N5", "N4", "N3", "N2", "N1"};
        int btnGap = 15;
        int totalW = 5 * LEVEL_BTN_WIDTH + 4 * btnGap;
        int startX = (SCREEN_WIDTH - totalW) / 2;

        for (int i = 0; i < 5; i++) {
            int btnX = startX + i * (LEVEL_BTN_WIDTH + btnGap);
            if (x >= btnX && x < btnX + LEVEL_BTN_WIDTH) {
                searchByLevel(levels[i]);
                return true;
            }
        }
    }

    // Review button
    if (_totalDue > 0 || _totalNew > 0) {
        int reviewBtnY = btnY + LEVEL_BTN_HEIGHT + 30 + 35 + 35 + 50;
        int btnW = 250;
        int btnH = 50;
        int btnX = (SCREEN_WIDTH - btnW) / 2;

        if (y >= reviewBtnY && y < reviewBtnY + btnH &&
            x >= btnX && x < btnX + btnW) {
            _mode = ScreenMode::REVIEW;
            nextCard();
            requestRedraw();
            return true;
        }
    }

    return false;
}

bool SRSScreen::handleResultsTouch(int x, int y) {
    // Back button (header area)
    if (y < PAD_Y + 50) {
        _mode = ScreenMode::SEARCH;
        requestRedraw();
        return true;
    }

    // Scroll up
    int listY = PAD_Y + 50;
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;
    int maxVisible = (availableHeight - listY - 60) / ITEM_HEIGHT;

    // Scroll buttons
    if (x > SCREEN_WIDTH - 80) {
        if (y < listY + 50 && _searchScrollOffset > 0) {
            _searchScrollOffset -= maxVisible;
            if (_searchScrollOffset < 0) _searchScrollOffset = 0;
            requestRedraw();
            return true;
        }
        if (y > availableHeight - 100 && _searchScrollOffset + maxVisible < _searchResults.size()) {
            _searchScrollOffset += maxVisible;
            requestRedraw();
            return true;
        }
    }

    // Item selection
    if (y >= listY && y < listY + maxVisible * ITEM_HEIGHT) {
        int itemIndex = (y - listY) / ITEM_HEIGHT + _searchScrollOffset;
        if (itemIndex >= 0 && itemIndex < _searchResults.size()) {
            addGrammarToSRS(_searchResults[itemIndex]);
            return true;
        }
    }

    return false;
}

bool SRSScreen::handleReviewTouch(int x, int y) {
    int availableHeight = SCREEN_HEIGHT - TAB_BAR_HEIGHT;

    // Search button (top right) for grammar mode
    if (_cardType == "grammar") {
        int btnX = SCREEN_WIDTH - PAD_X - 80;
        if (x >= btnX && x < btnX + 70 && y >= 8 && y < 38) {
            _mode = ScreenMode::SEARCH;
            searchByLevel(_currentLevel);
            return true;
        }
    }

    // No card - check "Add Grammar" button
    if (!_currentCard) {
        if (_cardType == "grammar") {
            int btnW = 200;
            int btnH = 50;
            int btnX = (SCREEN_WIDTH - btnW) / 2;
            int btnY = availableHeight / 2 + 30;

            if (x >= btnX && x < btnX + btnW && y >= btnY && y < btnY + btnH) {
                _mode = ScreenMode::SEARCH;
                searchByLevel(_currentLevel);
                return true;
            }
        }
        return false;
    }

    // Front state - show answer
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

        if (y > HEADER_HEIGHT && y < availableHeight - BUTTON_HEIGHT) {
            _cardState = CardState::BACK;
            requestRedraw();
            return true;
        }
    }

    // Back state - response buttons
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
