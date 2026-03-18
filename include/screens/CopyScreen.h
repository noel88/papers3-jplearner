#pragma once

#include "ScrollableScreen.h"
#include "Config.h"
#include <vector>

/**
 * 필사 (Copy/Transcription) Screen
 *
 * Displays daily content from 365.txt file.
 * Extends ScrollableScreen for drag scroll functionality.
 *
 * Design Pattern: Template Method (inherited from ScrollableScreen)
 * - Implements drawHeader(), drawContent(), calculateContentHeight()
 */
class CopyScreen : public ScrollableScreen {
public:
    CopyScreen();

    // ============================================
    // IScreen Implementation
    // ============================================

    const char* getName() const override { return "Copy"; }

    void onEnter() override;

    /**
     * Override draw to handle empty content case
     */
    void draw() override;

    // ============================================
    // Content Loading
    // ============================================

    /**
     * Load content for today based on RTC date
     * @return true if content was loaded successfully
     */
    bool loadTodayContent();

protected:
    // ============================================
    // ScrollableScreen Template Method Implementation
    // ============================================

    void drawHeader() override;
    void drawContent() override;
    int calculateContentHeight() override;

    int getHeaderHeight() const override { return 50; }

private:
    // Content data
    String _date;
    String _title;
    String _author;
    std::vector<String> _paragraphs;

    // Helper functions
    int getDayOfYear(int month, int day);
    int printWrappedAt(const String& text, int x, int y, int maxWidth);
    void drawEmptyState();
};
