#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "TextLayout.h"
#include "PopupMenu.h"
#include "SRSManager.h"
#include "DictionaryManager.h"
#include "Config.h"

/**
 * TextSelectionHelper - Common utilities for text selection screens
 *
 * Provides shared functionality for:
 * - Word/grammar saving to SRS
 * - Dictionary lookups and popups
 * - Toast notifications
 *
 * Usage:
 *   class MyScreen : public BaseScreen {
 *       TextSelectionHelper _selectionHelper;
 *
 *       void handleAction(PopupMenu::Action action) {
 *           String text = _popupMenu.getSelectedText();
 *           if (_selectionHelper.handleAction(action, text)) {
 *               // Action was handled
 *           }
 *       }
 *   };
 */
class TextSelectionHelper {
public:
    TextSelectionHelper() = default;

    /**
     * Handle popup menu action
     * @param action The action from popup menu
     * @param selectedText The selected text
     * @param onRedraw Callback to request screen redraw
     * @return true if dictionary popup is shown (caller should not clear selection)
     */
    bool handleAction(PopupMenu::Action action, const String& selectedText,
                      std::function<void()> onRedraw = nullptr);

    /**
     * Check if dictionary popup is showing
     */
    bool isDictionaryVisible() const { return _showingDictionary; }

    /**
     * Close dictionary popup
     */
    void closeDictionary();

    /**
     * Handle touch on dictionary popup
     * @return true if touch was handled (popup closed)
     */
    bool handleDictionaryTouch();

    /**
     * Show toast notification
     */
    void showToast(const char* message, std::function<void()> onRedraw = nullptr);

    /**
     * Save word to vocabulary (SRS)
     */
    bool saveToVocabulary(const String& word);

    /**
     * Save text to grammar patterns (SRS)
     */
    bool saveToGrammar(const String& text);

    /**
     * Show dictionary popup for word
     */
    void showDictionaryPopup(const String& word);

    /**
     * Get dictionary results
     */
    const std::vector<DictEntry>& getDictResults() const { return _dictResults; }

private:
    void drawDictionaryEntry(const DictEntry& entry, int y);

    bool _showingDictionary = false;
    std::vector<DictEntry> _dictResults;
    std::function<void()> _redrawCallback;
};
