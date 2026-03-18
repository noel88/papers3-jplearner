#include "screens/ScreenManager.h"

ScreenManager& ScreenManager::instance() {
    static ScreenManager instance;
    return instance;
}

ScreenManager::ScreenManager()
    : _currentTab(static_cast<TabIndex>(-1)) {  // Invalid initial value
    // Initialize all screen pointers to nullptr
    for (int i = 0; i < TAB_COUNT; i++) {
        _screens[i] = nullptr;
    }
}

void ScreenManager::registerScreen(TabIndex index, IScreen* screen) {
    if (index >= 0 && index < TAB_COUNT) {
        _screens[index] = screen;
        Serial.printf("ScreenManager: registered '%s' at tab %d\n",
                      screen ? screen->getName() : "null", index);
    }
}

IScreen* ScreenManager::getScreen(TabIndex index) {
    if (index >= 0 && index < TAB_COUNT) {
        return _screens[index];
    }
    return nullptr;
}

void ScreenManager::switchTo(TabIndex index) {
    if (index < 0 || index >= TAB_COUNT) {
        return;
    }

    if (index == _currentTab) {
        return;  // Already on this tab
    }

    // Call onExit for current screen
    IScreen* currentScreen = getCurrentScreen();
    if (currentScreen) {
        currentScreen->onExit();
    }

    // Switch tab
    TabIndex previousTab = _currentTab;
    _currentTab = index;

    // Call onEnter for new screen
    IScreen* newScreen = getCurrentScreen();
    if (newScreen) {
        newScreen->onEnter();
    }

    Serial.printf("ScreenManager: switched from tab %d to %d\n", previousTab, index);
}

IScreen* ScreenManager::getCurrentScreen() {
    return _screens[_currentTab];
}

bool ScreenManager::handleTouchStart(int x, int y) {
    IScreen* screen = getCurrentScreen();
    if (screen) {
        return screen->handleTouchStart(x, y);
    }
    return false;
}

bool ScreenManager::handleTouchMove(int x, int y) {
    IScreen* screen = getCurrentScreen();
    if (screen) {
        return screen->handleTouchMove(x, y);
    }
    return false;
}

bool ScreenManager::handleTouchEnd() {
    IScreen* screen = getCurrentScreen();
    if (screen) {
        return screen->handleTouchEnd();
    }
    return false;
}

void ScreenManager::draw() {
    IScreen* screen = getCurrentScreen();
    if (screen) {
        screen->draw();
        screen->clearRedrawFlag();
    }
}

bool ScreenManager::needsRedraw() const {
    IScreen* screen = _screens[_currentTab];
    if (screen) {
        return screen->needsRedraw();
    }
    return false;
}

void ScreenManager::requestRedraw() {
    IScreen* screen = _screens[_currentTab];
    if (screen) {
        screen->requestRedraw();
    }
}

void ScreenManager::clearRedrawFlag() {
    IScreen* screen = _screens[_currentTab];
    if (screen) {
        screen->clearRedrawFlag();
    }
}
