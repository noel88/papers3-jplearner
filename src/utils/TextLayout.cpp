#include "TextLayout.h"
#include "FontManager.h"

TextLayout::TextLayout()
    : _hasSelection(false) {
}

void TextLayout::clear() {
    _lines.clear();
    _hasSelection = false;
}

void TextLayout::addLine(int paraIndex, int byteStart, int byteEnd,
                         int x, int y, int width, int height,
                         const String& text) {
    LineInfo line;
    line.paraIndex = paraIndex;
    line.byteStart = byteStart;
    line.byteEnd = byteEnd;
    line.x = x;
    line.y = y;
    line.width = width;
    line.height = height;
    line.text = text;
    _lines.push_back(line);
}

bool TextLayout::findWordAt(int touchX, int touchY, WordInfo& wordInfo) {
    // Find line at touch Y coordinate
    for (const auto& line : _lines) {
        if (touchY >= line.y && touchY < line.y + line.height) {
            // Found the line, now find character at X
            if (touchX < line.x || touchX >= line.x + line.width) {
                continue;  // Touch outside this line's text
            }

            // Calculate approximate byte position based on X
            FontManager& fm = FontManager::instance();
            bool useCustomFont = fm.hasCustomFont();

            int relX = touchX - line.x;
            int bytePos = 0;
            int textLen = line.text.length();

            // Iterate through characters to find position
            while (bytePos < textLen) {
                // Get UTF-8 character length
                int charLen = 1;
                uint8_t c = line.text[bytePos];
                if (c >= 0xF0) charLen = 4;
                else if (c >= 0xE0) charLen = 3;
                else if (c >= 0xC0) charLen = 2;

                String substr = line.text.substring(0, bytePos + charLen);
                int charX = useCustomFont ?
                    fm.getTextWidth(substr) :
                    M5.Display.textWidth(substr.c_str());

                if (charX >= relX) {
                    // Found character at touch position
                    break;
                }

                bytePos += charLen;
            }

            // Find word boundaries around this position
            int wordStart, wordEnd;
            if (findWordBoundaries(line.text, bytePos, wordStart, wordEnd)) {
                // Calculate word X position and width
                String beforeWord = line.text.substring(0, wordStart);
                String word = line.text.substring(wordStart, wordEnd);

                int wordX = line.x + (useCustomFont ?
                    fm.getTextWidth(beforeWord) :
                    M5.Display.textWidth(beforeWord.c_str()));
                int wordW = useCustomFont ?
                    fm.getTextWidth(word) :
                    M5.Display.textWidth(word.c_str());

                wordInfo.paraIndex = line.paraIndex;
                wordInfo.byteStart = line.byteStart + wordStart;
                wordInfo.byteEnd = line.byteStart + wordEnd;
                wordInfo.x = wordX;
                wordInfo.y = line.y;
                wordInfo.width = wordW;
                wordInfo.height = line.height;
                wordInfo.text = word;

                return true;
            }
        }
    }

    return false;
}

bool TextLayout::findWordBoundaries(const String& text, int bytePos,
                                     int& wordStart, int& wordEnd) {
    int textLen = text.length();
    if (bytePos >= textLen) return false;

    // Check if current position is at a word character
    uint8_t c = text[bytePos];

    // For CJK characters (3-byte UTF-8), each character is a "word"
    if (c >= 0xE0 && c < 0xF0) {
        // 3-byte UTF-8 character (CJK, Japanese, etc.)
        wordStart = bytePos;
        wordEnd = bytePos + 3;
        if (wordEnd > textLen) wordEnd = textLen;
        return true;
    }

    // For ASCII, find word boundaries
    if (!isWordChar(c)) {
        // Not at a word character, try to find nearby word
        // Look backward
        int searchPos = bytePos;
        while (searchPos > 0 && !isWordChar(text[searchPos - 1])) {
            searchPos--;
        }
        if (searchPos > 0) {
            bytePos = searchPos - 1;
        } else {
            // Look forward
            searchPos = bytePos;
            while (searchPos < textLen && !isWordChar(text[searchPos])) {
                searchPos++;
            }
            if (searchPos < textLen) {
                bytePos = searchPos;
            } else {
                return false;  // No word found
            }
        }
    }

    // Find start of word
    wordStart = bytePos;
    while (wordStart > 0) {
        uint8_t prev = text[wordStart - 1];
        if (prev >= 0xE0) {
            // Previous is CJK, stop here
            break;
        }
        if (!isWordChar(prev)) break;
        wordStart--;
    }

    // Find end of word
    wordEnd = bytePos;
    while (wordEnd < textLen) {
        uint8_t curr = text[wordEnd];
        if (curr >= 0xE0) {
            // CJK character
            if (wordEnd == wordStart) {
                // Single CJK char as word
                wordEnd += 3;
                if (wordEnd > textLen) wordEnd = textLen;
            }
            break;
        }
        if (!isWordChar(curr)) break;

        // Move to next character
        int charLen = 1;
        if (curr >= 0xC0) charLen = 2;
        wordEnd += charLen;
    }

    return wordStart < wordEnd;
}

bool TextLayout::isWordChar(uint8_t c) {
    // ASCII letters, numbers, and high bytes (part of UTF-8)
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c >= 0x80) return true;  // UTF-8 continuation or start byte
    return false;
}

void TextLayout::setSelection(const WordInfo& word) {
    _selection = word;
    _hasSelection = true;
}

void TextLayout::clearSelection() {
    _hasSelection = false;
}

String TextLayout::getSelectedText() const {
    if (!_hasSelection) return "";
    return _selection.text;
}

bool TextLayout::getSelectionBounds(int& x, int& y, int& w, int& h) const {
    if (!_hasSelection) return false;

    x = _selection.x;
    y = _selection.y;
    w = _selection.width;
    h = _selection.height;
    return true;
}

void TextLayout::drawHighlight() {
    if (!_hasSelection) return;

    // Draw inverted background (black) for clear visibility on e-ink
    int padding = 4;
    M5.Display.fillRect(
        _selection.x - padding,
        _selection.y - 2,
        _selection.width + padding * 2,
        _selection.height,
        TFT_BLACK
    );

    // Draw the selected text in white (inverted) using M5.Display
    // This works for both custom font and built-in font display
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setFont(&fonts::efontJA_24);
    M5.Display.setTextSize(1.0);
    M5.Display.setCursor(_selection.x, _selection.y);
    M5.Display.print(_selection.text);
    M5.Display.setTextColor(TFT_BLACK);  // Reset
}

int TextLayout::getByteXPosition(const LineInfo& line, int bytePos) {
    if (bytePos <= 0) return line.x;

    FontManager& fm = FontManager::instance();
    bool useCustomFont = fm.hasCustomFont();

    // Get text up to bytePos
    int relPos = bytePos - line.byteStart;
    if (relPos < 0) relPos = 0;
    if (relPos > (int)line.text.length()) relPos = line.text.length();

    String substr = line.text.substring(0, relPos);
    int width = useCustomFont ?
        fm.getTextWidth(substr) :
        M5.Display.textWidth(substr.c_str());

    return line.x + width;
}
