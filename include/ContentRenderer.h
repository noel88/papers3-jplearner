#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>
#include "Config.h"
#include "FontManager.h"
#include "TextLayout.h"
#include "PopupMenu.h"

/**
 * ContentRenderer - Common text content rendering for e-ink screens
 *
 * Handles UTF-8 aware text wrapping, line breaking, and rendering
 * with support for both custom SD card fonts and built-in fonts.
 *
 * Used by CopyScreen and ReadScreen for consistent text display.
 */
class ContentRenderer {
public:
    // Layout configuration
    struct Config {
        int headerHeight = 50;
        int navHeight = 50;
        int tabBarHeight = 60;
        int padX = 20;
        int padY = 20;
        int lineSpacing = 8;
        int paragraphSpacing = 24;
        int fontSizePt = 24;  // Font size in points
    };

    ContentRenderer() = default;

    /**
     * Set layout configuration
     */
    void setConfig(const Config& config) { _config = config; }

    /**
     * Render page content
     * @param paragraphs All paragraphs
     * @param pageBreaks Page break indices
     * @param currentPage Current page number
     * @param totalPages Total pages
     * @param textLayout TextLayout for selection tracking
     * @param popupMenu PopupMenu to draw if visible
     */
    void renderPage(
        const std::vector<String>& paragraphs,
        const std::vector<int>& pageBreaks,
        int currentPage,
        int totalPages,
        TextLayout& textLayout,
        PopupMenu& popupMenu
    );

    /**
     * Calculate page breaks for paragraphs
     * @param paragraphs All paragraphs
     * @param pageBreaks Output page break indices
     * @return Total number of pages
     */
    int calculatePages(
        const std::vector<String>& paragraphs,
        std::vector<int>& pageBreaks
    );

private:
    /**
     * Get UTF-8 character length at position
     */
    int getCharLen(uint8_t c) const {
        if (c >= 0xF0) return 4;
        if (c >= 0xE0) return 3;
        if (c >= 0xC0) return 2;
        return 1;
    }

    /**
     * Get text width using appropriate font
     */
    int getTextWidth(const String& text, bool useCustomFont, FontManager& fm) const {
        return useCustomFont ? fm.getTextWidth(text) : M5.Display.textWidth(text.c_str());
    }

    /**
     * Draw text using appropriate font
     */
    void drawText(const String& text, int x, int y, bool useCustomFont, FontManager& fm) const {
        if (useCustomFont) {
            fm.drawString(text, x, y);
        } else {
            M5.Display.setCursor(x, y);
            M5.Display.print(text);
        }
    }

    Config _config;
};
