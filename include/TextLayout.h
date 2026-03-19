#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>

/**
 * TextLayout - Track text positions for touch-based selection
 *
 * Records character/line positions during text rendering,
 * enabling touch coordinate to text mapping for selection.
 */

// Information about a rendered line
struct LineInfo {
    int paraIndex;      // Paragraph index in content
    int byteStart;      // UTF-8 byte start in paragraph
    int byteEnd;        // UTF-8 byte end in paragraph
    int x, y;           // Screen position
    int width, height;  // Dimensions
    String text;        // Actual text content
};

// Information about a word (for selection)
struct WordInfo {
    int paraIndex;
    int byteStart;
    int byteEnd;
    int x, y;
    int width, height;
    String text;
};

class TextLayout {
public:
    TextLayout();

    // Clear all recorded positions
    void clear();

    // Record a line during rendering
    void addLine(int paraIndex, int byteStart, int byteEnd,
                 int x, int y, int width, int height,
                 const String& text);

    // Find word at touch coordinates
    // Returns true if word found, fills wordInfo
    bool findWordAt(int touchX, int touchY, WordInfo& wordInfo);

    // Selection management
    void setSelection(const WordInfo& word);
    void setRangeSelection(const WordInfo& start, const WordInfo& end);
    void clearSelection();
    bool hasSelection() const { return _hasSelection; }

    // Get selected text
    String getSelectedText() const;

    // Get selection bounds for highlight drawing
    bool getSelectionBounds(int& x, int& y, int& w, int& h) const;

    // Draw highlight for selected text
    void drawHighlight();

    // Get selection info for popup positioning
    const WordInfo& getSelection() const { return _selection; }

private:
    std::vector<LineInfo> _lines;

    // Selection state
    bool _hasSelection;
    WordInfo _selection;

    // Find word boundaries in text (UTF-8 aware)
    bool findWordBoundaries(const String& text, int bytePos,
                            int& wordStart, int& wordEnd);

    // Check if byte position is at word character (not punctuation/space)
    bool isWordChar(uint8_t c);

    // Calculate X position of byte within line
    int getByteXPosition(const LineInfo& line, int bytePos);
};
