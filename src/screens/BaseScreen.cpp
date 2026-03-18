#include "screens/BaseScreen.h"

BaseScreen::BaseScreen()
    : _needsRedraw(true) {
}

void BaseScreen::onEnter() {
    // Default: request redraw when entering screen
    _needsRedraw = true;
    Serial.printf("Screen entered: %s\n", getName());
}

void BaseScreen::onExit() {
    // Default: nothing to clean up
    Serial.printf("Screen exited: %s\n", getName());
}

void BaseScreen::clearContentArea() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_HEIGHT, TFT_WHITE);
}

void BaseScreen::drawCenteredText(const char* text, int y) {
    int textWidth = M5.Display.textWidth(text);
    int x = (SCREEN_WIDTH - textWidth) / 2;
    M5.Display.setCursor(x, y);
    M5.Display.print(text);
}

void BaseScreen::drawPlaceholder(const char* title, const char* description) {
    clearContentArea();

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setTextColor(TFT_BLACK);

    // Title - centered
    drawCenteredText(title, CONTENT_HEIGHT / 2 - 40);

    // Description - centered
    drawCenteredText(description, CONTENT_HEIGHT / 2 + 20);
}
