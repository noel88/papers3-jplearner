#include "SleepManager.h"
#include "SRSManager.h"
#include "UIHelpers.h"
#include <esp_sleep.h>

void SleepManager::init() {
    _sleepMinutes = config.sleepMinutes;
    _lastActivity = millis();
    _sleeping = false;
    _justWokeUp = false;
}

void SleepManager::resetActivity() {
    _lastActivity = millis();
}

void SleepManager::update() {
    // Skip if already sleeping or sleep disabled
    if (_sleeping || _sleepMinutes <= 0) {
        return;
    }

    unsigned long elapsed = millis() - _lastActivity;
    unsigned long timeout = (unsigned long)_sleepMinutes * 60 * 1000;

    if (elapsed >= timeout) {
        enterSleep();
    }
}

void SleepManager::enterSleep() {
    if (_sleeping) return;

    _sleeping = true;

    // Draw sleep screen
    drawSleepScreen();

    // Wait for display to finish
    M5.Display.display();
    M5.Display.waitDisplay();

    // Put e-ink display to sleep (retains image, zero power)
    M5.Display.sleep();

    // Configure touch wakeup (M5Paper S3 uses GPIO21 for touch interrupt)
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);

    // Enter light sleep loop - stay asleep until real touch detected
    while (true) {
        esp_light_sleep_start();

        // --- Execution resumes here after wakeup ---
        // Give touch controller time to stabilize before checking
        delay(50);
        M5.update();

        auto touch = M5.Touch.getDetail();
        if (touch.wasPressed() || touch.isPressed()) {
            // Real touch detected - wake up
            break;
        }

        // False wakeup (noise) - go back to sleep
    }

    wakeUp();
}

void SleepManager::wakeUp() {
    if (!_sleeping) return;

    // Wake display
    M5.Display.wakeup();

    _sleeping = false;
    _justWokeUp = true;
    _lastActivity = millis();

    // Check for due cards and trigger review prompt
    SRSManager& srs = SRSManager::instance();
    int dueCount = srs.getDueCount();
    int newCount = srs.getNewCount();
    int totalReviewable = dueCount + newCount;

    if (totalReviewable > 0) {
        _showReviewPrompt = true;
        _promptDueCount = totalReviewable;
    } else {
        _showReviewPrompt = false;
        _promptDueCount = 0;
    }
}

int SleepManager::getBatteryPercent() {
    int percent = M5.Power.getBatteryLevel();

    if (percent < 0 || percent > 100) {
        int32_t voltage = M5.Power.getBatteryVoltage();
        if (voltage > 0) {
            percent = (voltage - 3000) * 100 / 1200;
        } else {
            percent = 50;  // Unknown
        }
    }

    return constrain(percent, 0, 100);
}

void SleepManager::drawSleepScreen() {
    // Full screen clear for clean sleep image
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.fillScreen(TFT_WHITE);

    M5.Display.setFont(&fonts::efontKR_24);
    M5.Display.setTextColor(TFT_BLACK);

    // Get current time
    auto dt = M5.Rtc.getDateTime();
    int battery = getBatteryPercent();

    // Center positions
    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;

    // ============================================
    // Large Time Display (center)
    // ============================================
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", dt.time.hours, dt.time.minutes);

    M5.Display.setTextSize(3.0);  // Large text for time
    int timeW = M5.Display.textWidth(timeBuf);
    UI::drawBoldText(timeBuf, centerX - timeW / 2, centerY - 60);

    // ============================================
    // Date (below time)
    // ============================================
    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%d.%02d.%02d",
             dt.date.year, dt.date.month, dt.date.date);

    M5.Display.setTextSize(UI::SIZE_CONTENT);
    int dateW = M5.Display.textWidth(dateBuf);
    UI::drawBoldText(dateBuf, centerX - dateW / 2, centerY + 20);

    // ============================================
    // Battery indicator (below date)
    // ============================================
    int battY = centerY + 70;

    // Battery icon (simple rectangle)
    int battIconW = 50;
    int battIconH = 24;
    int battIconX = centerX - 60;

    // Battery outline
    uint16_t battColor = TFT_BLACK;
    if (battery <= 10) {
        battColor = TFT_RED;
    } else if (battery <= 20) {
        battColor = 0xFD20;  // Orange
    }

    M5.Display.drawRect(battIconX, battY, battIconW, battIconH, battColor);
    M5.Display.fillRect(battIconX + battIconW, battY + 6, 4, 12, battColor);

    // Battery fill
    int fillW = (battIconW - 4) * battery / 100;
    if (fillW > 0) {
        M5.Display.fillRect(battIconX + 2, battY + 2, fillW, battIconH - 4, battColor);
    }

    // Battery percentage text
    char battBuf[8];
    snprintf(battBuf, sizeof(battBuf), "%d%%", battery);
    M5.Display.setTextSize(UI::SIZE_BODY);
    UI::drawBoldText(battBuf, battIconX + battIconW + 15, battY);

    // ============================================
    // Wake instruction (bottom)
    // ============================================
    M5.Display.setTextSize(UI::SIZE_SMALL);
    M5.Display.setTextColor(TFT_DARKGRAY);

    const char* wakeText = "Touch to wake";
    int wakeW = M5.Display.textWidth(wakeText);
    M5.Display.setCursor(centerX - wakeW / 2, SCREEN_HEIGHT - 60);
    M5.Display.print(wakeText);

    M5.Display.setTextColor(TFT_BLACK);
}
