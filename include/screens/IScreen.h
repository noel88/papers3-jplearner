#pragma once

#include <Arduino.h>

/**
 * Interface for all screen implementations.
 * Defines the contract that all screens must implement.
 *
 * Design Pattern: Interface
 * - Enables polymorphic screen handling
 * - Allows ScreenManager to treat all screens uniformly
 */
class IScreen {
public:
    virtual ~IScreen() = default;

    // ============================================
    // Lifecycle Methods
    // ============================================

    /**
     * Called when this screen becomes active (tab switch to this screen)
     * Use for initialization, loading data, etc.
     */
    virtual void onEnter() = 0;

    /**
     * Called when leaving this screen (tab switch away)
     * Use for cleanup, saving state, etc.
     */
    virtual void onExit() = 0;

    // ============================================
    // Rendering
    // ============================================

    /**
     * Draw the screen content.
     * Called when the screen needs to be rendered.
     */
    virtual void draw() = 0;

    /**
     * Check if the screen needs to be redrawn.
     * @return true if redraw is needed
     */
    virtual bool needsRedraw() const = 0;

    /**
     * Mark that the screen needs redraw
     */
    virtual void requestRedraw() = 0;

    /**
     * Clear the redraw flag after drawing
     */
    virtual void clearRedrawFlag() = 0;

    // ============================================
    // Touch Handling
    // ============================================

    /**
     * Handle touch start event.
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @return true if screen needs to be redrawn
     */
    virtual bool handleTouchStart(int x, int y) = 0;

    /**
     * Handle touch move event (dragging).
     * @param x Current X coordinate
     * @param y Current Y coordinate
     * @return true if screen needs to be redrawn
     */
    virtual bool handleTouchMove(int x, int y) = 0;

    /**
     * Handle touch end event.
     * @return true if screen needs to be redrawn
     */
    virtual bool handleTouchEnd() = 0;

    // ============================================
    // Identification
    // ============================================

    /**
     * Get the screen name for debugging/logging.
     * @return Screen name string
     */
    virtual const char* getName() const = 0;
};
