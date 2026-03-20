#include "PopupMenu.h"

PopupMenu::PopupMenu()
    : _visible(false), _x(0), _y(0), _width(0), _height(0) {
    calculateSize();
}

void PopupMenu::calculateSize() {
    _width = BUTTON_COUNT * BUTTON_WIDTH + (BUTTON_COUNT - 1) * BUTTON_SPACING + MENU_PADDING * 2;
    _height = BUTTON_HEIGHT + MENU_PADDING * 2;
}

void PopupMenu::show(int x, int y, const String& selectedText) {
    _selectedText = selectedText;

    // Position menu centered above the touch point
    _x = x - _width / 2;
    _y = y - _height - 10;  // 10px above

    // Keep within screen bounds
    if (_x < 10) _x = 10;
    if (_x + _width > M5.Display.width() - 10) {
        _x = M5.Display.width() - _width - 10;
    }
    if (_y < 10) _y = 10;

    _visible = true;
}

void PopupMenu::hide() {
    _visible = false;
}

void PopupMenu::draw() {
    if (!_visible) return;

    // Draw menu background with rounded corners
    M5.Display.fillRoundRect(_x, _y, _width, _height, MENU_RADIUS, TFT_WHITE);
    M5.Display.drawRoundRect(_x, _y, _width, _height, MENU_RADIUS, TFT_BLACK);

    // Draw buttons
    M5.Display.setFont(&fonts::efontKR_16);
    M5.Display.setTextColor(TFT_BLACK);

    for (int i = 0; i < BUTTON_COUNT; i++) {
        int bx, by, bw, bh;
        getButtonBounds(i, bx, by, bw, bh);

        // Button outline
        M5.Display.drawRoundRect(bx, by, bw, bh, 4, TFT_DARKGRAY);

        // Button label (centered)
        const char* label = getButtonLabel(i);
        int textW = M5.Display.textWidth(label);
        int textX = bx + (bw - textW) / 2;
        int textY = by + (bh - 16) / 2;

        M5.Display.setCursor(textX, textY);
        M5.Display.print(label);
    }
}

PopupMenu::Action PopupMenu::handleTouch(int x, int y) {
    if (!_visible) return NONE;

    // Check if touch is outside menu
    if (x < _x || x >= _x + _width || y < _y || y >= _y + _height) {
        return CANCEL;
    }

    // Check which button was touched
    for (int i = 0; i < BUTTON_COUNT; i++) {
        int bx, by, bw, bh;
        getButtonBounds(i, bx, by, bw, bh);

        if (x >= bx && x < bx + bw && y >= by && y < by + bh) {
            switch (i) {
                case 0: return SAVE;
                case 1: return GRAMMAR;
            }
        }
    }

    return NONE;
}

void PopupMenu::getButtonBounds(int index, int& x, int& y, int& w, int& h) const {
    x = _x + MENU_PADDING + index * (BUTTON_WIDTH + BUTTON_SPACING);
    y = _y + MENU_PADDING;
    w = BUTTON_WIDTH;
    h = BUTTON_HEIGHT;
}

const char* PopupMenu::getButtonLabel(int index) const {
    switch (index) {
        case 0: return "저장";
        case 1: return "문형";
        default: return "";
    }
}

void PopupMenu::getBounds(int& x, int& y, int& w, int& h) const {
    x = _x;
    y = _y;
    w = _width;
    h = _height;
}
