#pragma once

#include <Arduino.h>
#include <vector>

class CopyScreen {
public:
    CopyScreen();

    // Load content for today based on RTC date
    bool loadTodayContent();

    // Drawing
    void draw();

    // Touch handling (returns true if screen needs redraw)
    bool handleTouchStart(int x, int y);
    bool handleTouchMove(int x, int y);
    bool handleTouchEnd();

    // Reset scroll position
    void resetScroll();

private:
    // Content data
    String date;
    String title;
    String author;
    std::vector<String> paragraphs;

    // Scroll state
    int scrollY;           // Current scroll position (pixels from top)
    int contentHeight;     // Total content height
    int visibleHeight;     // Visible area height

    // Touch tracking for scroll
    int lastTouchY;
    bool isDragging;

    // Helper functions
    int getDayOfYear(int month, int day);
    void drawHeader();
    void drawContent();
    void drawScrollIndicator();
    int calculateContentHeight();
    int printWrappedAt(const String& text, int x, int y, int maxWidth);
};
