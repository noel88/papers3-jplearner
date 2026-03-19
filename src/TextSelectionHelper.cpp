#include "TextSelectionHelper.h"

bool TextSelectionHelper::handleAction(PopupMenu::Action action, const String& selectedText,
                                       std::function<void()> onRedraw) {
    _redrawCallback = onRedraw;

    switch (action) {
        case PopupMenu::SEARCH:
            showDictionaryPopup(selectedText);
            return true;  // Dictionary shown, don't clear selection

        case PopupMenu::SAVE:
            if (saveToVocabulary(selectedText)) {
                showToast("단어 저장됨", onRedraw);
            }
            return false;

        case PopupMenu::GRAMMAR:
            if (saveToGrammar(selectedText)) {
                showToast("문형 저장됨", onRedraw);
            }
            return false;

        case PopupMenu::CANCEL:
        default:
            return false;
    }
}

void TextSelectionHelper::closeDictionary() {
    _showingDictionary = false;
    _dictResults.clear();
}

bool TextSelectionHelper::handleDictionaryTouch() {
    if (_showingDictionary) {
        closeDictionary();
        return true;
    }
    return false;
}

void TextSelectionHelper::showToast(const char* message, std::function<void()> onRedraw) {
    int toastW = 200;
    int toastH = 50;
    int toastX = (SCREEN_WIDTH - toastW) / 2;
    int toastY = (SCREEN_HEIGHT - 60 - toastH) / 2;  // 60 = TAB_BAR_HEIGHT approx

    M5.Display.setEpdMode(epd_mode_t::epd_fastest);
    M5.Display.startWrite();

    // Draw toast background
    M5.Display.fillRect(toastX, toastY, toastW, toastH, TFT_WHITE);
    M5.Display.drawRect(toastX, toastY, toastW, toastH, TFT_BLACK);
    M5.Display.drawRect(toastX + 1, toastY + 1, toastW - 2, toastH - 2, TFT_BLACK);

    // Draw message centered
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(message, toastX + toastW / 2, toastY + toastH / 2);

    M5.Display.endWrite();

    // Brief delay then redraw
    delay(800);

    if (onRedraw) {
        onRedraw();
    }
}

bool TextSelectionHelper::saveToVocabulary(const String& word) {
    SRSManager& srs = SRSManager::instance();

    // Check if already saved
    if (srs.hasCard(word, "word")) {
        showToast("이미 저장됨", _redrawCallback);
        return false;
    }

    // Add as SRS card
    String cardId = srs.addCard("word", word, "");
    return cardId.length() > 0;
}

bool TextSelectionHelper::saveToGrammar(const String& text) {
    SRSManager& srs = SRSManager::instance();

    // Check if already saved
    if (srs.hasCard(text, "grammar")) {
        showToast("이미 저장됨", _redrawCallback);
        return false;
    }

    // Add as SRS card
    String cardId = srs.addCard("grammar", text, "");
    return cardId.length() > 0;
}

void TextSelectionHelper::showDictionaryPopup(const String& word) {
    DictionaryManager& dict = DictionaryManager::instance();

    if (!dict.isAvailable()) {
        showToast("사전 없음", _redrawCallback);
        return;
    }

    // Look up the word
    _dictResults = dict.lookupByWord(word);

    if (_dictResults.empty()) {
        // Try partial search
        _dictResults = dict.search(word, 5);
    }

    if (_dictResults.empty()) {
        showToast("검색 결과 없음", _redrawCallback);
        return;
    }

    _showingDictionary = true;

    // Draw dictionary popup
    int popupW = 450;
    int popupH = 300;
    int popupX = (SCREEN_WIDTH - popupW) / 2;
    int popupY = (SCREEN_HEIGHT - 60 - popupH) / 2;  // 60 = TAB_BAR_HEIGHT approx

    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.startWrite();

    // Draw popup background with border
    M5.Display.fillRect(popupX, popupY, popupW, popupH, TFT_WHITE);
    M5.Display.drawRect(popupX, popupY, popupW, popupH, TFT_BLACK);
    M5.Display.drawRect(popupX + 1, popupY + 1, popupW - 2, popupH - 2, TFT_BLACK);

    // Draw header with word
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(popupX + 15, popupY + 15);
    M5.Display.print(word);

    // Draw separator
    M5.Display.drawLine(popupX + 10, popupY + 50, popupX + popupW - 10, popupY + 50, TFT_DARKGRAY);

    // Draw entries (max 3)
    int entryY = popupY + 60;
    int maxEntries = min(3, (int)_dictResults.size());

    for (int i = 0; i < maxEntries; i++) {
        drawDictionaryEntry(_dictResults[i], entryY);
        entryY += 70;
    }

    // Draw close button
    int btnW = 100;
    int btnH = 40;
    int btnX = popupX + (popupW - btnW) / 2;
    int btnY = popupY + popupH - btnH - 15;

    M5.Display.fillRoundRect(btnX, btnY, btnW, btnH, 5, TFT_DARKGRAY);
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_WHITE);
    String closeText = "닫기";
    int textW = M5.Display.textWidth(closeText.c_str());
    M5.Display.setCursor(btnX + (btnW - textW) / 2, btnY + (btnH - 16) / 2);
    M5.Display.print(closeText);

    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.endWrite();
}

void TextSelectionHelper::drawDictionaryEntry(const DictEntry& entry, int y) {
    int popupW = 450;
    int popupX = (SCREEN_WIDTH - popupW) / 2;
    int contentX = popupX + 15;
    int contentW = popupW - 30;

    // Reading (ひらがな)
    if (entry.reading.length() > 0 && entry.reading != entry.word) {
        M5.Display.setFont(&fonts::efontJA_16);
        M5.Display.setTextColor(TFT_DARKGRAY);
        M5.Display.setCursor(contentX, y);
        M5.Display.print("[");
        M5.Display.print(entry.reading);
        M5.Display.print("]");
    }

    // Part of speech
    if (entry.partOfSpeech.length() > 0) {
        M5.Display.setFont(&fonts::efontKR_12);
        M5.Display.setTextColor(TFT_DARKGRAY);
        int posX = contentX + 150;
        M5.Display.setCursor(posX, y + 4);
        M5.Display.print(entry.partOfSpeech);
    }

    // Meaning
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setCursor(contentX, y + 25);

    // Truncate if too long
    String meaning = entry.meanings;
    if (M5.Display.textWidth(meaning.c_str()) > contentW) {
        while (meaning.length() > 0 && M5.Display.textWidth((meaning + "...").c_str()) > contentW) {
            meaning = meaning.substring(0, meaning.length() - 1);
        }
        meaning += "...";
    }
    M5.Display.print(meaning);
}
