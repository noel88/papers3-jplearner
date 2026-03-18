#include "screens/PlaceholderScreen.h"

PlaceholderScreen::PlaceholderScreen(const char* name, const char* title, const char* description)
    : BaseScreen()
    , _name(name)
    , _title(title)
    , _description(description) {
}

void PlaceholderScreen::draw() {
    drawPlaceholder(_title, _description);
}
