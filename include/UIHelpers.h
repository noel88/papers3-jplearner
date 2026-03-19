#pragma once

#include <M5Unified.h>

/**
 * UI Helper functions for consistent styling across the app
 *
 * Font styling:
 * - Bold effect: Draw text twice with 1px offset
 * - Larger content text: Use setTextSize(1.1) or higher
 */

namespace UI {

// Font size presets
constexpr float SIZE_TITLE = 1.2f;      // Screen titles
constexpr float SIZE_HEADER = 1.1f;     // Headers, menu items
constexpr float SIZE_CONTENT = 1.1f;    // Content text (settings details)
constexpr float SIZE_BODY = 1.0f;       // Normal body text
constexpr float SIZE_SMALL = 0.9f;      // Secondary info

/**
 * Draw bold text by rendering twice with 1px offset
 * Creates a bolder appearance on e-ink display
 */
inline void drawBoldText(const char* text, int x, int y) {
    M5.Display.setCursor(x, y);
    M5.Display.print(text);
    M5.Display.setCursor(x + 1, y);
    M5.Display.print(text);
}

inline void drawBoldText(const String& text, int x, int y) {
    drawBoldText(text.c_str(), x, y);
}

/**
 * Draw bold text centered horizontally
 */
inline void drawBoldTextCentered(const char* text, int y, int screenWidth = 960) {
    int textW = M5.Display.textWidth(text);
    int x = (screenWidth - textW) / 2;
    drawBoldText(text, x, y);
}

/**
 * Draw title text (large and bold)
 */
inline void drawTitle(const char* text, int x, int y) {
    float oldSize = M5.Display.getTextSizeX();
    M5.Display.setTextSize(SIZE_TITLE);
    drawBoldText(text, x, y);
    M5.Display.setTextSize(oldSize);
}

/**
 * Draw header text (slightly larger and bold)
 */
inline void drawHeader(const char* text, int x, int y) {
    float oldSize = M5.Display.getTextSizeX();
    M5.Display.setTextSize(SIZE_HEADER);
    drawBoldText(text, x, y);
    M5.Display.setTextSize(oldSize);
}

/**
 * Draw content text (larger for readability)
 */
inline void drawContent(const char* text, int x, int y) {
    float oldSize = M5.Display.getTextSizeX();
    M5.Display.setTextSize(SIZE_CONTENT);
    drawBoldText(text, x, y);
    M5.Display.setTextSize(oldSize);
}

/**
 * Draw menu item (bold, normal size)
 */
inline void drawMenuItem(const char* text, int x, int y) {
    M5.Display.setTextSize(SIZE_HEADER);
    drawBoldText(text, x, y);
}

/**
 * Set up font for Korean text with bold style
 */
inline void setupKoreanFont(float size = SIZE_BODY) {
    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextSize(size);
    M5.Display.setTextColor(TFT_BLACK);
}

/**
 * Set up font for Japanese text with bold style
 */
inline void setupJapaneseFont(float size = SIZE_BODY) {
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(size);
    M5.Display.setTextColor(TFT_BLACK);
}

} // namespace UI
