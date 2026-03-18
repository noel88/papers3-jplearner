#pragma once

#include "IScreen.h"
#include "Config.h"
#include <M5Unified.h>

/**
 * Abstract base class for all screens.
 * Provides common functionality and default implementations.
 *
 * Design Pattern: Template Method (partial)
 * - Provides default implementations for optional methods
 * - Subclasses override only what they need
 */
class BaseScreen : public IScreen {
public:
    BaseScreen();
    virtual ~BaseScreen() = default;

    // ============================================
    // IScreen Implementation (Default Behaviors)
    // ============================================

    void onEnter() override;
    void onExit() override;

    // Default touch handling - no-ops, override as needed
    bool handleTouchStart(int x, int y) override { return false; }
    bool handleTouchMove(int x, int y) override { return false; }
    bool handleTouchEnd() override { return false; }

    // Redraw management
    bool needsRedraw() const override { return _needsRedraw; }
    void requestRedraw() override { _needsRedraw = true; }
    void clearRedrawFlag() override { _needsRedraw = false; }

protected:
    bool _needsRedraw;

    // ============================================
    // Common Drawing Helpers
    // ============================================

    /**
     * Clear the content area (excluding tab bar)
     */
    void clearContentArea();

    /**
     * Draw text centered horizontally at given Y position
     * @param text Text to draw
     * @param y Y coordinate
     */
    void drawCenteredText(const char* text, int y);

    /**
     * Draw a placeholder screen with title and description
     * Useful for unimplemented screens
     * @param title Main title text
     * @param description Description text below title
     */
    void drawPlaceholder(const char* title, const char* description);

    /**
     * Get content area dimensions
     */
    int getContentWidth() const { return SCREEN_WIDTH; }
    int getContentHeight() const { return CONTENT_HEIGHT; }
};
