#include "FontManager.h"
#include "Config.h"
#include "esp_heap_caps.h"

FontManager& FontManager::instance() {
    static FontManager instance;
    return instance;
}

FontManager::FontManager() {
    // Constructor
}

bool FontManager::init() {
    // Set up OpenFontRender with M5GFX display
    _render.setDrawer(M5.Display);

    // Scan for available fonts
    scanFonts();

    Serial.printf("FontManager: Found %d fonts\n", _fonts.size());
    return true;
}

void FontManager::scanFonts() {
    _fonts.clear();

    Serial.printf("FontManager: Scanning fonts directory: %s\n", DIR_FONTS);
    File dir = SD.open(DIR_FONTS);
    if (!dir) {
        Serial.println("FontManager: Cannot open fonts directory");
        return;
    }

    if (!dir.isDirectory()) {
        Serial.println("FontManager: /fonts is not a directory!");
        dir.close();
        return;
    }

    Serial.println("FontManager: Directory opened, scanning files...");

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        uint32_t size = entry.size();
        entry.close();

        Serial.printf("FontManager: Entry: %s (isFile=%d, size=%d)\n",
                      name.c_str(), isFile, size);

        if (!isFile) continue;

        // Check for TTF files
        String nameLower = name;
        nameLower.toLowerCase();
        if (!nameLower.endsWith(".ttf")) {
            Serial.printf("FontManager: Skipping non-TTF: %s\n", name.c_str());
            continue;
        }

        FontInfo info;
        info.filename = name;
        info.fileSize = size;

        // Extract display name (remove extension, replace - with space)
        info.displayName = name.substring(0, name.length() - 4);
        info.displayName.replace("-", " ");
        info.displayName.replace("_", " ");

        _fonts.push_back(info);
        Serial.printf("FontManager: Found TTF font: %s (%d bytes)\n",
                      info.filename.c_str(), info.fileSize);
    }
    dir.close();

    Serial.printf("FontManager: Scan complete, found %d fonts\n", _fonts.size());
}

String FontManager::getFontPath(const String& filename) {
    return String(DIR_FONTS) + "/" + filename;
}

bool FontManager::loadFont(const String& filename) {
    _lastError = 0;

    Serial.println("=== FontManager::loadFont START ===");
    Serial.printf("FontManager: Input filename: '%s'\n", filename.c_str());
    Serial.printf("FontManager: External buffer state: ptr=%p, size=%d\n",
                  _externalBuffer, _externalBufferSize);
    Serial.flush();

    if (filename.length() == 0) {
        Serial.println("FontManager: ERROR - Empty filename");
        _lastError = -1;  // Empty filename
        _fontLoaded = false;
        return false;
    }

    String path = getFontPath(filename);
    Serial.printf("FontManager: Full path: '%s'\n", path.c_str());
    Serial.flush();

    // Check if file exists
    if (!SD.exists(path.c_str())) {
        Serial.printf("FontManager: ERROR - File not found: %s\n", path.c_str());
        _lastError = -2;  // File not found
        _fontLoaded = false;
        return false;
    }
    Serial.println("FontManager: File exists on SD card");

    // Free previous font data (but not if using external buffer)
    if (_fontData != nullptr && !_usingExternalBuffer) {
        heap_caps_free(_fontData);
    }
    _fontData = nullptr;
    _usingExternalBuffer = false;

    // Read font file into PSRAM
    File fontFile = SD.open(path.c_str(), FILE_READ);
    if (!fontFile) {
        Serial.println("FontManager: ERROR - Cannot open file");
        _lastError = -3;  // Cannot open file
        _fontLoaded = false;
        return false;
    }

    size_t fileSize = fontFile.size();
    _fontDataSize = fileSize;  // Store for debug

    Serial.printf("FontManager: File size: %d bytes (%.1f MB)\n", fileSize, fileSize / 1048576.0f);
    Serial.printf("FontManager: External buffer: %p, size=%d (%.1f MB)\n",
                  _externalBuffer, _externalBufferSize, _externalBufferSize / 1048576.0f);
    Serial.flush();

    // Try allocation - prefer external buffer, then PSRAM, then regular heap
    _fontData = nullptr;
    _usingExternalBuffer = false;

    // Option 1: Use pre-allocated external buffer
    if (_externalBuffer != nullptr && _externalBufferSize >= fileSize) {
        _fontData = _externalBuffer;
        _usingExternalBuffer = true;
        Serial.printf("FontManager: Using external buffer at %p\n", _fontData);
        Serial.flush();
    } else if (_externalBuffer != nullptr) {
        Serial.printf("FontManager: WARNING - Font file (%d bytes) larger than buffer (%d bytes)\n",
                      fileSize, _externalBufferSize);
        Serial.flush();
    } else {
        Serial.println("FontManager: WARNING - No external buffer available");
        Serial.flush();
    }

    // Option 2: Try PSRAM allocation
    if (_fontData == nullptr && psramFound()) {
        Serial.printf("FontManager: PSRAM Free=%d, Largest=%d\n",
                      ESP.getFreePsram(), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        _fontData = (uint8_t*)heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (_fontData) {
            Serial.printf("FontManager: PSRAM alloc successful at %p\n", _fontData);
        }
    }

    // Option 3: Regular malloc
    if (_fontData == nullptr) {
        Serial.println("FontManager: Trying regular malloc...");
        _fontData = (uint8_t*)malloc(fileSize);
        if (_fontData) {
            Serial.printf("FontManager: malloc successful at %p\n", _fontData);
        }
    }

    if (_fontData == nullptr) {
        Serial.println("FontManager: All allocation methods failed!");
        _lastError = -4;  // Memory allocation failed
        fontFile.close();
        _fontLoaded = false;
        return false;
    }

    size_t bytesRead = fontFile.read(_fontData, fileSize);
    fontFile.close();

    if (bytesRead != fileSize) {
        _lastError = -5;  // Read error
        if (!_usingExternalBuffer) heap_caps_free(_fontData);
        _fontData = nullptr;
        _fontLoaded = false;
        return false;
    }

    // Load font from memory
    Serial.println("FontManager: Loading font into renderer...");
    Serial.printf("FontManager: Font data at %p, size=%d\n", _fontData, fileSize);
    Serial.flush();

    FT_Error error = _render.loadFont(_fontData, fileSize);
    if (error) {
        Serial.printf("FontManager: FreeType error %d loading font!\n", error);
        _lastError = error;  // FreeType error code (positive)
        if (!_usingExternalBuffer) heap_caps_free(_fontData);
        _fontData = nullptr;
        _fontLoaded = false;
        return false;
    }
    Serial.println("FontManager: Font loaded successfully into renderer");

    _render.setFontSize(_fontSize);
    _fontLoaded = true;
    _lastError = 0;

    Serial.println("=== FontManager::loadFont SUCCESS ===");
    Serial.flush();
    return true;
}

bool FontManager::setPrimaryFont(const String& filename) {
    _primaryFontName = filename;

    if (filename.length() == 0) {
        _fontLoaded = false;
        return true;
    }

    return loadFont(filename);
}

bool FontManager::setFallbackFont(const String& filename) {
    _fallbackFontName = filename;
    // Fallback is loaded on-demand when primary font fails
    return true;
}

void FontManager::setExternalBuffer(uint8_t* buffer, size_t size) {
    _externalBuffer = buffer;
    _externalBufferSize = size;
    Serial.printf("FontManager: External buffer set: %p (%d bytes)\n", buffer, size);
}

void FontManager::setFontSize(uint16_t size) {
    _fontSize = size;
    if (_fontLoaded) {
        _render.setFontSize(size);
    }
}

// Check if UTF-8 character is Korean (Hangul)
static bool isKoreanChar(const char* utf8, int len) {
    if (len < 3) return false;
    uint8_t b0 = utf8[0], b1 = utf8[1], b2 = utf8[2];
    // Hangul Syllables: U+AC00 - U+D7AF (0xEA 0xB0 0x80 - 0xED 0x9E 0xAF)
    // Hangul Jamo: U+1100 - U+11FF
    // Hangul Compatibility Jamo: U+3130 - U+318F
    if (b0 == 0xEA && b1 >= 0xB0) return true;
    if (b0 == 0xEB) return true;
    if (b0 == 0xEC) return true;
    if (b0 == 0xED && b1 <= 0x9E) return true;
    return false;
}

int FontManager::drawStringWithColor(const String& text, int x, int y, uint16_t color) {
    if (!_fontLoaded) {
        // Use built-in font
        M5.Display.setTextColor(color);
        M5.Display.setCursor(x, y);
        M5.Display.print(text);
        // Note: Color reset handled by caller if needed
        return M5.Display.textWidth(text.c_str());
    }

    // Use TTF font with specified color
    _render.setCursor(x, y);
    _render.setFontColor(color);
    _render.printf("%s", text.c_str());
    // Note: Color reset handled by caller if needed
    return _render.getTextWidth(text.c_str());
}

int FontManager::drawString(const String& text, int x, int y) {
    if (!_fontLoaded) {
        // Use built-in font
        M5.Display.setCursor(x, y);
        M5.Display.print(text);
        return M5.Display.textWidth(text.c_str());
    }

    // If Korean fallback is disabled, use TTF for everything
    if (!_koreanFallback) {
        _render.setCursor(x, y);
        _render.setFontColor(TFT_BLACK);
        _render.printf("%s", text.c_str());
        return _render.getTextWidth(text.c_str());
    }

    // Mixed rendering: TTF for Japanese/ASCII, built-in for Korean
    int curX = x;
    int totalWidth = 0;
    const char* str = text.c_str();
    int len = text.length();
    int pos = 0;

    String ttfBuffer;  // Buffer for TTF characters
    String korBuffer;  // Buffer for Korean characters
    bool lastWasKorean = false;

    while (pos < len) {
        // Get UTF-8 character length
        uint8_t c = str[pos];
        int charLen = 1;
        if (c >= 0xF0) charLen = 4;
        else if (c >= 0xE0) charLen = 3;
        else if (c >= 0xC0) charLen = 2;

        bool isKorean = (charLen == 3) && isKoreanChar(str + pos, charLen);

        if (isKorean != lastWasKorean && (ttfBuffer.length() > 0 || korBuffer.length() > 0)) {
            // Flush the previous buffer
            if (!lastWasKorean && ttfBuffer.length() > 0) {
                _render.setCursor(curX, y);
                _render.setFontColor(TFT_BLACK);
                _render.printf("%s", ttfBuffer.c_str());
                int w = _render.getTextWidth(ttfBuffer.c_str());
                curX += w;
                totalWidth += w;
                ttfBuffer = "";
            } else if (lastWasKorean && korBuffer.length() > 0) {
                M5.Display.setFont(&fonts::efontKR_24);
                M5.Display.setTextColor(TFT_BLACK);
                M5.Display.setCursor(curX, y);
                M5.Display.print(korBuffer);
                int w = M5.Display.textWidth(korBuffer.c_str());
                curX += w;
                totalWidth += w;
                korBuffer = "";
            }
        }

        // Add character to appropriate buffer
        String ch = text.substring(pos, pos + charLen);
        if (isKorean) {
            korBuffer += ch;
        } else {
            ttfBuffer += ch;
        }
        lastWasKorean = isKorean;
        pos += charLen;
    }

    // Flush remaining buffer
    if (ttfBuffer.length() > 0) {
        _render.setCursor(curX, y);
        _render.setFontColor(TFT_BLACK);
        _render.printf("%s", ttfBuffer.c_str());
        totalWidth += _render.getTextWidth(ttfBuffer.c_str());
    }
    if (korBuffer.length() > 0) {
        M5.Display.setFont(&fonts::efontKR_24);
        M5.Display.setTextColor(TFT_BLACK);
        M5.Display.setCursor(curX, y);
        M5.Display.print(korBuffer);
        totalWidth += M5.Display.textWidth(korBuffer.c_str());
    }

    return totalWidth;
}

int FontManager::getTextWidth(const String& text) {
    if (!_fontLoaded) {
        return M5.Display.textWidth(text.c_str());
    }

    // If Korean fallback is disabled, use TTF width
    if (!_koreanFallback) {
        return _render.getTextWidth(text.c_str());
    }

    // Mixed width calculation for Korean fallback
    const char* str = text.c_str();
    int len = text.length();
    int pos = 0;
    int totalWidth = 0;

    String ttfBuffer;
    String korBuffer;
    bool lastWasKorean = false;

    while (pos < len) {
        uint8_t c = str[pos];
        int charLen = 1;
        if (c >= 0xF0) charLen = 4;
        else if (c >= 0xE0) charLen = 3;
        else if (c >= 0xC0) charLen = 2;

        bool isKorean = (charLen == 3) && isKoreanChar(str + pos, charLen);

        if (isKorean != lastWasKorean && (ttfBuffer.length() > 0 || korBuffer.length() > 0)) {
            if (!lastWasKorean && ttfBuffer.length() > 0) {
                totalWidth += _render.getTextWidth(ttfBuffer.c_str());
                ttfBuffer = "";
            } else if (lastWasKorean && korBuffer.length() > 0) {
                M5.Display.setFont(&fonts::efontKR_24);
                totalWidth += M5.Display.textWidth(korBuffer.c_str());
                korBuffer = "";
            }
        }

        String ch = text.substring(pos, pos + charLen);
        if (isKorean) {
            korBuffer += ch;
        } else {
            ttfBuffer += ch;
        }
        lastWasKorean = isKorean;
        pos += charLen;
    }

    if (ttfBuffer.length() > 0) {
        totalWidth += _render.getTextWidth(ttfBuffer.c_str());
    }
    if (korBuffer.length() > 0) {
        M5.Display.setFont(&fonts::efontKR_24);
        totalWidth += M5.Display.textWidth(korBuffer.c_str());
    }

    return totalWidth;
}

void FontManager::useBuiltinFont(const char* type) {
    if (strcmp(type, "ja") == 0) {
        M5.Display.setFont(&fonts::efontJA_24);
    } else if (strcmp(type, "ko") == 0) {
        M5.Display.setFont(&fonts::efontKR_24);
    } else {
        M5.Display.setFont(&fonts::Font2);
    }
}
