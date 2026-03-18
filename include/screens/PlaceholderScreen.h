#pragma once

#include "BaseScreen.h"

/**
 * Simple placeholder screen for unimplemented tabs.
 * Displays a centered title and description.
 *
 * Usage:
 *   PlaceholderScreen wordScreen("Word", "단어 학습", "Coming soon - SRS 단어 학습");
 */
class PlaceholderScreen : public BaseScreen {
public:
    /**
     * Create a placeholder screen
     * @param name Screen name for debugging
     * @param title Title to display (Korean supported)
     * @param description Description text below title
     */
    PlaceholderScreen(const char* name, const char* title, const char* description);

    // IScreen implementation
    void draw() override;
    const char* getName() const override { return _name; }

private:
    const char* _name;
    const char* _title;
    const char* _description;
};
