#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <OpenFontRender.h>
#include <vector>

/**
 * FontManager - TTF Font Loading with Fallback Support
 *
 * Uses OpenFontRender library for TTF rendering with LovyanGFX.
 * Supports:
 * - TTF fonts loaded from /fonts/ directory
 * - Primary + Fallback font configuration
 * - Multiple font sizes
 * - Anti-aliased rendering
 *
 * Font files should be placed in SD:/fonts/
 * Example: /fonts/NotoSansJP-Regular.ttf
 */

struct FontInfo {
    String filename;      // e.g., "NotoSansJP-Regular.ttf"
    String displayName;   // e.g., "Noto Sans JP"
    uint32_t fileSize;    // File size in bytes
};

class FontManager {
public:
    static FontManager& instance();

    // Initialize FontManager
    bool init();

    // Get available fonts from /fonts/ directory
    const std::vector<FontInfo>& getAvailableFonts() const { return _fonts; }

    // Set fonts (filename only, e.g., "NotoSansJP-Regular.ttf")
    // Empty string = use built-in efont
    bool setPrimaryFont(const String& filename);
    bool setFallbackFont(const String& filename);

    // Get current font names
    const String& getPrimaryFontName() const { return _primaryFontName; }
    const String& getFallbackFontName() const { return _fallbackFontName; }

    // Set font size (default: 24)
    void setFontSize(uint16_t size);
    uint16_t getFontSize() const { return _fontSize; }

    // Draw text using TTF font (returns width drawn)
    // Falls back to built-in font if primary doesn't have glyph
    int drawString(const String& text, int x, int y);

    // Draw text with specified color (for highlights)
    int drawStringWithColor(const String& text, int x, int y, uint16_t color);

    // Get text width
    int getTextWidth(const String& text);

    // Enable/disable Korean fallback to built-in efontKR
    void setKoreanFallback(bool enable) { _koreanFallback = enable; }
    bool hasKoreanFallback() const { return _koreanFallback; }

    // Check if using custom TTF font
    bool hasCustomFont() const { return _primaryFontName.length() > 0 && _fontLoaded; }

    // Use built-in efont (for when TTF not needed)
    void useBuiltinFont(const char* type = "ja");

    // Scan /fonts/ directory
    void scanFonts();

    // Get OpenFontRender instance for advanced usage
    OpenFontRender& getRenderer() { return _render; }

private:
    FontManager();
    ~FontManager() = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    OpenFontRender _render;
    std::vector<FontInfo> _fonts;

    String _primaryFontName;
    String _fallbackFontName;
    uint16_t _fontSize = 24;
    bool _fontLoaded = false;
    uint8_t* _fontData = nullptr;  // Font data in PSRAM
    size_t _fontDataSize = 0;      // Font file size
    int _lastError = 0;            // Last error code for debugging
    bool _koreanFallback = true;   // Use built-in font for Korean characters

public:
    int getLastError() const { return _lastError; }
    size_t getFontDataSize() const { return _fontDataSize; }

    // Load TTF font file
    bool loadFont(const String& filename);

    // Get full path for font
    String getFontPath(const String& filename);

    // Set external buffer for font data (to avoid PSRAM fragmentation)
    void setExternalBuffer(uint8_t* buffer, size_t size);

private:
    uint8_t* _externalBuffer = nullptr;
    size_t _externalBufferSize = 0;
    bool _usingExternalBuffer = false;
};
