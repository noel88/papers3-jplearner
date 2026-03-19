#pragma once

#include <M5Unified.h>
#include "Config.h"

/**
 * SleepManager - Handles device sleep mode with e-ink sleep screen
 *
 * Features:
 * - Inactivity timer (configurable via config.sleepMinutes)
 * - Minimal clock sleep screen (time, date, battery)
 * - Touch wakeup
 * - E-ink display retains image while sleeping (zero power)
 */
class SleepManager {
public:
    static SleepManager& instance() {
        static SleepManager inst;
        return inst;
    }

    /**
     * Initialize sleep manager
     * Call once in setup()
     */
    void init();

    /**
     * Update activity timer
     * Call in loop() - resets timer on any user activity
     */
    void resetActivity();

    /**
     * Check if device should sleep
     * Call in loop() - handles sleep transition automatically
     */
    void update();

    /**
     * Check if currently sleeping
     */
    bool isSleeping() const { return _sleeping; }

    /**
     * Check if we just woke up from sleep (for triggering redraw)
     */
    bool justWokeUp() const { return _justWokeUp; }

    /**
     * Clear the woke up flag after handling
     */
    void clearWakeFlag() { _justWokeUp = false; }

    /**
     * Force immediate sleep
     */
    void enterSleep();

    /**
     * Wake up from sleep
     * Called automatically on touch interrupt
     */
    void wakeUp();

    /**
     * Set sleep timeout in minutes (0 = disabled)
     */
    void setSleepMinutes(int minutes) { _sleepMinutes = minutes; }

private:
    SleepManager() = default;
    ~SleepManager() = default;
    SleepManager(const SleepManager&) = delete;
    SleepManager& operator=(const SleepManager&) = delete;

    /**
     * Draw minimal clock sleep screen
     * Shows: time, date, battery, "Touch to wake"
     */
    void drawSleepScreen();

    /**
     * Get battery percentage
     */
    int getBatteryPercent();

    // State
    bool _sleeping = false;
    bool _justWokeUp = false;
    unsigned long _lastActivity = 0;
    int _sleepMinutes = 5;  // Default 5 minutes
};
