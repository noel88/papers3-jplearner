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

    File dir = SD.open(DIR_FONTS);
    if (!dir) {
        Serial.println("FontManager: Cannot open fonts directory");
        return;
    }

    while (File entry = dir.openNextFile()) {
        String name = entry.name();
        bool isFile = !entry.isDirectory();
        uint32_t size = entry.size();
        entry.close();

        if (!isFile) continue;

        // Check for TTF files
        String nameLower = name;
        nameLower.toLowerCase();
        if (!nameLower.endsWith(".ttf")) continue;

        FontInfo info;
        info.filename = name;
        info.fileSize = size;

        // Extract display name (remove extension, replace - with space)
        info.displayName = name.substring(0, name.length() - 4);
        info.displayName.replace("-", " ");
        info.displayName.replace("_", " ");

        _fonts.push_back(info);
        Serial.printf("FontManager: Found font: %s (%d bytes)\n",
                      info.filename.c_str(), info.fileSize);
    }
    dir.close();
}

String FontManager::getFontPath(const String& filename) {
    return String(DIR_FONTS) + "/" + filename;
}

bool FontManager::loadFont(const String& filename) {
    _lastError = 0;

    if (filename.length() == 0) {
        _lastError = -1;  // Empty filename
        _fontLoaded = false;
        return false;
    }

    String path = getFontPath(filename);

    // Check if file exists
    if (!SD.exists(path.c_str())) {
        _lastError = -2;  // File not found
        _fontLoaded = false;
        return false;
    }

    // Free previous font data (but not if using external buffer)
    if (_fontData != nullptr && !_usingExternalBuffer) {
        heap_caps_free(_fontData);
    }
    _fontData = nullptr;
    _usingExternalBuffer = false;

    // Read font file into PSRAM
    File fontFile = SD.open(path.c_str(), FILE_READ);
    if (!fontFile) {
        _lastError = -3;  // Cannot open file
        _fontLoaded = false;
        return false;
    }

    size_t fileSize = fontFile.size();
    _fontDataSize = fileSize;  // Store for debug

    // Check if PSRAM is available
    Serial.println("=== FontManager::loadFont ===");
    Serial.printf("FontManager: File size: %d bytes\n", fileSize);
    Serial.flush();

    // Try allocation - prefer external buffer, then PSRAM, then regular heap
    _fontData = nullptr;
    _usingExternalBuffer = false;

    // Option 1: Use pre-allocated external buffer
    if (_externalBuffer != nullptr && _externalBufferSize >= fileSize) {
        _fontData = _externalBuffer;
        _usingExternalBuffer = true;
        Serial.printf("FontManager: Using external buffer at %p (%d bytes)\n", _fontData, _externalBufferSize);
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
    FT_Error error = _render.loadFont(_fontData, fileSize);
    if (error) {
        _lastError = error;  // FreeType error code (positive)
        if (!_usingExternalBuffer) heap_caps_free(_fontData);
        _fontData = nullptr;
        _fontLoaded = false;
        return false;
    }

    _render.setFontSize(_fontSize);
    _fontLoaded = true;
    _lastError = 0;

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

int FontManager::drawString(const String& text, int x, int y) {
    if (!_fontLoaded) {
        // Use built-in font
        M5.Display.setCursor(x, y);
        M5.Display.print(text);
        return M5.Display.textWidth(text.c_str());
    }

    // Use OpenFontRender
    _render.setCursor(x, y);
    _render.setFontColor(TFT_BLACK);

    // Draw with primary font
    _render.printf("%s", text.c_str());

    return _render.getTextWidth(text.c_str());
}

int FontManager::getTextWidth(const String& text) {
    if (!_fontLoaded) {
        return M5.Display.textWidth(text.c_str());
    }
    return _render.getTextWidth(text.c_str());
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
