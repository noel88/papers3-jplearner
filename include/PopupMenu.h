#pragma once

#include <Arduino.h>
#include <M5Unified.h>

/**
 * PopupMenu - Action menu for text selection
 *
 * Shows a popup menu with actions:
 * - 검색 (Search) - Dictionary lookup (Phase 4)
 * - 저장 (Save) - Add to vocabulary
 * - 문형 (Grammar) - Add to grammar patterns
 */

class PopupMenu {
public:
    enum Action {
        NONE = 0,
        SEARCH,     // Dictionary search
        SAVE,       // Save to vocabulary
        GRAMMAR,    // Add to grammar patterns
        CANCEL      // Dismiss menu
    };

    PopupMenu();

    // Show menu at position (centered above the given point)
    void show(int x, int y, const String& selectedText);

    // Hide menu
    void hide();

    // Check if visible
    bool isVisible() const { return _visible; }

    // Handle touch - returns action if button touched
    Action handleTouch(int x, int y);

    // Draw the popup menu
    void draw();

    // Get selected text
    const String& getSelectedText() const { return _selectedText; }

    // Get menu bounds (for redraw optimization)
    void getBounds(int& x, int& y, int& w, int& h) const;

private:
    bool _visible;
    int _x, _y;             // Menu position (top-left)
    int _width, _height;
    String _selectedText;

    // Button layout
    static constexpr int BUTTON_COUNT = 3;
    static constexpr int BUTTON_WIDTH = 60;
    static constexpr int BUTTON_HEIGHT = 36;
    static constexpr int BUTTON_SPACING = 8;
    static constexpr int MENU_PADDING = 10;
    static constexpr int MENU_RADIUS = 8;

    // Calculate menu dimensions
    void calculateSize();

    // Get button bounds
    void getButtonBounds(int index, int& x, int& y, int& w, int& h) const;

    // Button labels
    const char* getButtonLabel(int index) const;
};
