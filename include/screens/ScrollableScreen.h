#pragma once

#include "BaseScreen.h"

/**
 * Abstract base class for screens with scrollable content.
 *
 * Design Pattern: Template Method
 * - draw() defines the algorithm skeleton: clearContentArea -> drawHeader -> drawContent -> drawScrollIndicator
 * - Subclasses implement drawHeader(), drawContent(), calculateContentHeight()
 *
 * Features:
 * - Touch-based drag scrolling
 * - Automatic bounds checking
 * - Visual scroll indicator
 */
class ScrollableScreen : public BaseScreen {
public:
    ScrollableScreen();
    virtual ~ScrollableScreen() = default;

    // ============================================
    // IScreen Implementation
    // ============================================

    /**
     * Template Method: Defines the drawing algorithm
     * Calls: clearContentArea -> drawHeader -> drawContent -> drawScrollIndicator
     */
    void draw() override;

    /**
     * Handle touch start - begin drag tracking
     */
    bool handleTouchStart(int x, int y) override;

    /**
     * Handle touch move - update scroll position
     */
    bool handleTouchMove(int x, int y) override;

    /**
     * Handle touch end - finalize scroll
     */
    bool handleTouchEnd() override;

    // ============================================
    // Scroll Management
    // ============================================

    /**
     * Reset scroll position to top
     */
    void resetScroll();

    /**
     * Get current scroll Y position
     */
    int getScrollY() const { return _scrollY; }

    /**
     * Set scroll Y position (with bounds checking)
     */
    void setScrollY(int y);

    /**
     * Check if content is scrollable (content height > visible height)
     */
    bool isScrollable() const;

protected:
    // ============================================
    // Template Methods (Must be implemented by subclasses)
    // ============================================

    /**
     * Draw the fixed header area (not affected by scroll)
     */
    virtual void drawHeader() = 0;

    /**
     * Draw the scrollable content area
     * Content should be offset by getScrollY() for scrolling effect
     */
    virtual void drawContent() = 0;

    /**
     * Calculate the total content height for scroll bounds
     * @return Total height in pixels
     */
    virtual int calculateContentHeight() = 0;

    // ============================================
    // Configuration (Override for customization)
    // ============================================

    /**
     * Get header height in pixels
     * Default: 50px
     */
    virtual int getHeaderHeight() const { return 50; }

    /**
     * Get scroll bar width in pixels
     * Default: 8px
     */
    virtual int getScrollBarWidth() const { return 8; }

    /**
     * Get visible content height (excluding header)
     */
    int getVisibleHeight() const { return CONTENT_HEIGHT - getHeaderHeight(); }

    // ============================================
    // Drawing Helpers
    // ============================================

    /**
     * Draw the scroll indicator bar on the right side
     */
    virtual void drawScrollIndicator();

    // ============================================
    // State
    // ============================================

    int _scrollY;          // Current scroll position (pixels from top)
    int _contentHeight;    // Total content height (set by subclass)

private:
    int _lastTouchY;       // Last touch Y position for drag calculation
    bool _isDragging;      // Currently dragging flag

    /**
     * Ensure scroll position is within valid bounds
     */
    void clampScroll();
};
