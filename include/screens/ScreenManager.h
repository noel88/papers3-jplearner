#pragma once

#include "IScreen.h"
#include "Config.h"

/**
 * Manages screen instances and routing.
 *
 * Design Pattern: Singleton
 * - Single point of screen management
 * - Handles tab switching with lifecycle callbacks
 * - Routes touch events to current screen
 *
 * Usage:
 *   ScreenManager::instance().registerScreen(TAB_COPY, &copyScreen);
 *   ScreenManager::instance().switchTo(TAB_COPY);
 *   ScreenManager::instance().draw();
 */
class ScreenManager {
public:
    /**
     * Get the singleton instance
     */
    static ScreenManager& instance();

    // ============================================
    // Screen Registration
    // ============================================

    /**
     * Register a screen for a tab index.
     * Should be called in setup() for all tabs.
     * @param index Tab index (0-5)
     * @param screen Pointer to screen instance (must remain valid)
     */
    void registerScreen(TabIndex index, IScreen* screen);

    /**
     * Get screen for a tab index
     * @param index Tab index
     * @return Screen pointer or nullptr if not registered
     */
    IScreen* getScreen(TabIndex index);

    // ============================================
    // Tab Switching
    // ============================================

    /**
     * Switch to a different tab.
     * Calls onExit() on current screen, onEnter() on new screen.
     * @param index Tab to switch to
     */
    void switchTo(TabIndex index);

    /**
     * Get current tab index
     */
    TabIndex getCurrentTab() const { return _currentTab; }

    /**
     * Get current screen
     */
    IScreen* getCurrentScreen();

    // ============================================
    // Touch Routing
    // ============================================

    /**
     * Route touch start to current screen
     * @return true if screen needs redraw
     */
    bool handleTouchStart(int x, int y);

    /**
     * Route touch move to current screen
     * @return true if screen needs redraw
     */
    bool handleTouchMove(int x, int y);

    /**
     * Route touch end to current screen
     * @return true if screen needs redraw
     */
    bool handleTouchEnd();

    // ============================================
    // Rendering
    // ============================================

    /**
     * Draw current screen content
     */
    void draw();

    /**
     * Check if current screen needs redraw
     */
    bool needsRedraw() const;

    /**
     * Request redraw of current screen
     */
    void requestRedraw();

    /**
     * Clear redraw flag after drawing
     */
    void clearRedrawFlag();

private:
    // Private constructor for singleton
    ScreenManager();

    // Prevent copying
    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    IScreen* _screens[TAB_COUNT];  // Fixed array, no dynamic allocation
    TabIndex _currentTab;
};
