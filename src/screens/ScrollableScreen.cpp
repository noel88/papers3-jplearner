#include "screens/ScrollableScreen.h"

ScrollableScreen::ScrollableScreen()
    : BaseScreen()
    , _scrollY(0)
    , _contentHeight(0)
    , _lastTouchY(0)
    , _isDragging(false) {
}

void ScrollableScreen::draw() {
    // Template Method: defines the drawing algorithm
    clearContentArea();
    drawHeader();
    drawContent();

    // Only draw scroll indicator if content is scrollable
    if (isScrollable()) {
        drawScrollIndicator();
    }
}

bool ScrollableScreen::handleTouchStart(int x, int y) {
    if (!isScrollable()) {
        return false;
    }

    _lastTouchY = y;
    _isDragging = true;
    return false;  // No redraw needed on touch start
}

bool ScrollableScreen::handleTouchMove(int x, int y) {
    if (!_isDragging || !isScrollable()) {
        return false;
    }

    int delta = _lastTouchY - y;  // Drag up = positive delta = scroll down
    _lastTouchY = y;

    if (delta == 0) {
        return false;
    }

    _scrollY += delta;
    clampScroll();

    return true;  // Redraw needed
}

bool ScrollableScreen::handleTouchEnd() {
    if (!_isDragging) {
        return false;
    }

    _isDragging = false;
    Serial.printf("%s: scroll end, scrollY=%d/%d\n",
                  getName(), _scrollY, _contentHeight - getVisibleHeight());
    return true;  // Final redraw
}

void ScrollableScreen::resetScroll() {
    _scrollY = 0;
    _isDragging = false;
}

void ScrollableScreen::setScrollY(int y) {
    _scrollY = y;
    clampScroll();
}

bool ScrollableScreen::isScrollable() const {
    return _contentHeight > getVisibleHeight();
}

void ScrollableScreen::clampScroll() {
    int maxScroll = _contentHeight - getVisibleHeight();
    if (maxScroll < 0) maxScroll = 0;

    if (_scrollY < 0) _scrollY = 0;
    if (_scrollY > maxScroll) _scrollY = maxScroll;
}

void ScrollableScreen::drawScrollIndicator() {
    int visibleHeight = getVisibleHeight();
    int headerHeight = getHeaderHeight();
    int scrollBarWidth = getScrollBarWidth();

    if (_contentHeight <= visibleHeight) {
        return;  // No scroll indicator needed
    }

    // Calculate scroll bar dimensions
    int scrollBarHeight = visibleHeight;
    int thumbHeight = (visibleHeight * visibleHeight) / _contentHeight;
    if (thumbHeight < 20) thumbHeight = 20;  // Minimum thumb size

    int maxScroll = _contentHeight - visibleHeight;
    int thumbY = headerHeight + (_scrollY * (scrollBarHeight - thumbHeight)) / maxScroll;

    // Draw scroll track (light gray)
    int trackX = SCREEN_WIDTH - scrollBarWidth - 4;
    M5.Display.fillRect(trackX, headerHeight, scrollBarWidth, scrollBarHeight, 0xDEDB);

    // Draw scroll thumb (dark)
    M5.Display.fillRect(trackX, thumbY, scrollBarWidth, thumbHeight, TFT_BLACK);
}
