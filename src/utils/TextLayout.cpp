#include "TextLayout.h"
#include "FontManager.h"
#include "Config.h"

TextLayout::TextLayout()
    : _hasSelection(false) {
}

void TextLayout::clear() {
    _lines.clear();
    // Don't clear selection here - it should persist across redraws
    // Use clearSelection() explicitly when needed
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

void TextLayout::setRangeSelection(const WordInfo& start, const WordInfo& end) {
    // Determine which word comes first (by Y then X position)
    const WordInfo* first = &start;
    const WordInfo* last = &end;

    if (end.y < start.y || (end.y == start.y && end.x < start.x)) {
        first = &end;
        last = &start;
    }

    // If same word, just use single selection
    if (first->x == last->x && first->y == last->y) {
        setSelection(start);
        return;
    }

    // Simple approach: find all text between the two positions
    String combinedText;

    for (const auto& line : _lines) {
        int lineY = line.y;
        int lineEndY = line.y + line.height;

        // Skip lines before selection
        if (lineEndY <= first->y) continue;
        // Stop after last line
        if (lineY > last->y + last->height) break;

        // This line is in selection range
        if (combinedText.length() > 0) combinedText += "";

        // Same line as start and end
        if (lineY == first->y && lineY == last->y) {
            // Extract text between first.x and last.x + last.width
            combinedText += extractTextInRange(line, first->x, last->x + last->width);
        }
        // First line of selection
        else if (lineY == first->y) {
            combinedText += extractTextInRange(line, first->x, line.x + line.width);
        }
        // Last line of selection
        else if (lineY == last->y) {
            combinedText += extractTextInRange(line, line.x, last->x + last->width);
        }
        // Middle lines - take whole line
        else if (lineY > first->y && lineY < last->y) {
            combinedText += line.text;
        }
    }

    // Create selection bounds
    _selection.x = first->x;
    _selection.y = first->y;

    if (first->y == last->y) {
        // Same line
        _selection.width = last->x + last->width - first->x;
        _selection.height = first->height;
    } else {
        // Multiple lines
        _selection.width = SCREEN_WIDTH - PAD_X - first->x;
        _selection.height = last->y + last->height - first->y;
    }

    _selection.text = combinedText.length() > 0 ? combinedText : first->text;
    _selection.paraIndex = first->paraIndex;
    _selection.byteStart = first->byteStart;
    _selection.byteEnd = last->byteEnd;

    _hasSelection = true;
}

String TextLayout::extractTextInRange(const LineInfo& line, int startX, int endX) {
    // Extract text from line between startX and endX
    String result;
    int charX = line.x;
    int bytePos = 0;
    int startByte = -1;
    int endByte = line.text.length();

    while (bytePos < (int)line.text.length()) {
        // Get character width
        int charLen = 1;
        uint8_t c = line.text[bytePos];
        if (c >= 0xF0) charLen = 4;
        else if (c >= 0xE0) charLen = 3;
        else if (c >= 0xC0) charLen = 2;

        String sub = line.text.substring(0, bytePos + charLen);
        int nextX = line.x + M5.Display.textWidth(sub.c_str());

        // Check if this character is in range
        if (startByte < 0 && nextX > startX) {
            startByte = bytePos;
        }
        if (nextX >= endX) {
            endByte = bytePos + charLen;
            break;
        }

        bytePos += charLen;
    }

    if (startByte >= 0 && startByte < endByte) {
        result = line.text.substring(startByte, endByte);
    }

    return result;
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

    int padding = 2;

    // For multi-line selection, highlight each line separately
    for (const auto& line : _lines) {
        int lineY = line.y;

        // Skip lines outside selection
        if (lineY + line.height <= _selection.y) continue;
        if (lineY > _selection.y + _selection.height) break;

        // Calculate highlight bounds for this line
        int hlX, hlW;
        String hlText;

        if (lineY == _selection.y && _selection.height <= line.height) {
            // Single line selection
            hlX = _selection.x;
            hlW = _selection.width;
            hlText = _selection.text;
        } else if (lineY == _selection.y) {
            // First line of multi-line
            hlX = _selection.x;
            hlW = line.x + line.width - _selection.x;
            hlText = extractTextInRange(line, hlX, hlX + hlW);
        } else if (lineY + line.height >= _selection.y + _selection.height) {
            // Last line of multi-line
            hlX = line.x;
            int endX = _selection.x + _selection.width;
            if (endX > line.x + line.width) endX = line.x + line.width;
            hlW = endX - hlX;
            hlText = extractTextInRange(line, hlX, endX);
        } else {
            // Middle line - full width
            hlX = line.x;
            hlW = line.width;
            hlText = line.text;
        }

        if (hlW <= 0) continue;

        // Draw black background
        M5.Display.fillRect(hlX - padding, lineY - 2, hlW + padding * 2, line.height, TFT_BLACK);

        // Draw white text on top
        M5.Display.setTextColor(TFT_WHITE);
        M5.Display.setFont(&fonts::efontJA_24);
        M5.Display.setTextSize(1.0);
        M5.Display.setCursor(hlX, lineY);
        M5.Display.print(hlText);
    }

    M5.Display.setTextColor(TFT_BLACK);
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
