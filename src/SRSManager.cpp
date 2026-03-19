#include "SRSManager.h"
#include <LittleFS.h>
#include <M5Unified.h>

void SRSManager::init() {
    loadCards();
}

String SRSManager::addCard(const String& type, const String& front, const String& back) {
    // Check for duplicates
    if (hasCard(front, type)) {
        return "";
    }

    SRSCard card;
    card.id = type + "_" + String(_nextId++);
    card.type = type;
    card.front = front;
    card.back = back;
    card.ef = INITIAL_EF;
    card.interval = 0;
    card.due = 0;  // New card, not yet scheduled
    card.reps = 0;
    card.lapses = 0;

    _cards.push_back(card);
    saveCards();

    return card.id;
}

bool SRSManager::hasCard(const String& front, const String& type) {
    for (const auto& card : _cards) {
        if (card.front == front && card.type == type) {
            return true;
        }
    }
    return false;
}

void SRSManager::processResponse(const String& cardId, SRSResponse response) {
    SRSCard* card = getCard(cardId);
    if (!card) return;

    // Update EF based on response
    card->ef = calculateNewEF(card->ef, response);

    // Calculate new interval
    int newInterval = calculateNewInterval(*card, response);

    // Update card state
    if (response == SRSResponse::AGAIN) {
        // Reset progress on failure
        card->lapses++;
        card->reps = 0;
        card->interval = 0;
        // Due in 1 minute
        card->due = getCurrentTime() + 60;
    } else {
        card->reps++;
        card->interval = newInterval;
        card->due = calculateDueTime(newInterval);
    }

    saveCards();
}

float SRSManager::calculateNewEF(float oldEF, SRSResponse response) {
    // SM-2 EF adjustment formula
    // EF' = EF + (0.1 - (5-q) * (0.08 + (5-q) * 0.02))
    // where q is response quality (0-5), we map our 0-3 to 0-5

    int q;
    switch (response) {
        case SRSResponse::AGAIN: q = 0; break;
        case SRSResponse::HARD:  q = 3; break;
        case SRSResponse::GOOD:  q = 4; break;
        case SRSResponse::EASY:  q = 5; break;
        default: q = 3;
    }

    float newEF = oldEF + (0.1f - (5 - q) * (0.08f + (5 - q) * 0.02f));

    // Clamp to minimum
    if (newEF < MIN_EF) newEF = MIN_EF;

    return newEF;
}

int SRSManager::calculateNewInterval(const SRSCard& card, SRSResponse response) {
    // New card - use initial intervals
    if (card.reps == 0) {
        switch (response) {
            case SRSResponse::AGAIN:
                return 0;  // 1 minute (handled separately)
            case SRSResponse::HARD:
                return 0;  // 6 minutes (handled separately for first review)
            case SRSResponse::GOOD:
                return 1;  // 1 day
            case SRSResponse::EASY:
                return NEW_EASY_DAYS;  // 4 days
            default:
                return 1;
        }
    }

    // Existing card - apply SM-2 formula
    int currentInterval = card.interval;
    if (currentInterval < 1) currentInterval = 1;

    float multiplier;
    switch (response) {
        case SRSResponse::AGAIN:
            return 0;  // Reset
        case SRSResponse::HARD:
            multiplier = 1.2f;
            break;
        case SRSResponse::GOOD:
            multiplier = card.ef;
            break;
        case SRSResponse::EASY:
            multiplier = card.ef * 1.3f;
            break;
        default:
            multiplier = card.ef;
    }

    int newInterval = (int)(currentInterval * multiplier);
    if (newInterval < 1) newInterval = 1;

    // Cap at 365 days
    if (newInterval > 365) newInterval = 365;

    return newInterval;
}

unsigned long SRSManager::calculateDueTime(int intervalDays) {
    return getCurrentTime() + (intervalDays * 24 * 60 * 60);
}

std::vector<SRSCard*> SRSManager::getDueCards(const String& type, int limit) {
    std::vector<SRSCard*> result;
    unsigned long now = getCurrentTime();

    for (auto& card : _cards) {
        // Skip if type filter doesn't match
        if (type.length() > 0 && card.type != type) continue;

        // Skip new cards (due == 0)
        if (card.due == 0) continue;

        // Check if due
        if (card.due <= now) {
            result.push_back(&card);
            if (limit > 0 && (int)result.size() >= limit) break;
        }
    }

    return result;
}

std::vector<SRSCard*> SRSManager::getNewCards(const String& type, int limit) {
    std::vector<SRSCard*> result;

    for (auto& card : _cards) {
        // Skip if type filter doesn't match
        if (type.length() > 0 && card.type != type) continue;

        // New cards have due == 0
        if (card.due == 0) {
            result.push_back(&card);
            if (limit > 0 && (int)result.size() >= limit) break;
        }
    }

    return result;
}

SRSCard* SRSManager::getCard(const String& id) {
    for (auto& card : _cards) {
        if (card.id == id) {
            return &card;
        }
    }
    return nullptr;
}

int SRSManager::getCardCount(const String& type) {
    if (type.length() == 0) return _cards.size();

    int count = 0;
    for (const auto& card : _cards) {
        if (card.type == type) count++;
    }
    return count;
}

int SRSManager::getDueCount(const String& type) {
    unsigned long now = getCurrentTime();
    int count = 0;

    for (const auto& card : _cards) {
        if (type.length() > 0 && card.type != type) continue;
        if (card.due > 0 && card.due <= now) count++;
    }

    return count;
}

int SRSManager::getNewCount(const String& type) {
    int count = 0;

    for (const auto& card : _cards) {
        if (type.length() > 0 && card.type != type) continue;
        if (card.due == 0) count++;
    }

    return count;
}

unsigned long SRSManager::getCurrentTime() {
    // Get time from RTC
    auto dt = M5.Rtc.getDateTime();

    // Convert to Unix timestamp (approximate)
    // Days since 1970-01-01
    int year = dt.date.year;
    int month = dt.date.month;
    int day = dt.date.date;

    // Simple calculation (not accounting for leap years precisely)
    int days = (year - 1970) * 365 + (year - 1969) / 4;  // Leap year approximation

    const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m < month; m++) {
        days += daysInMonth[m];
    }
    days += day - 1;

    unsigned long seconds = days * 86400UL;
    seconds += dt.time.hours * 3600UL;
    seconds += dt.time.minutes * 60UL;
    seconds += dt.time.seconds;

    return seconds;
}

bool SRSManager::saveCards() {
    // Ensure directory exists
    if (!LittleFS.exists("/userdata")) {
        LittleFS.mkdir("/userdata");
    }

    File file = LittleFS.open(CARDS_FILE, FILE_WRITE);
    if (!file) {
        return false;
    }

    // Create JSON document
    JsonDocument doc;
    doc["nextId"] = _nextId;

    JsonArray cardsArray = doc["cards"].to<JsonArray>();

    for (const auto& card : _cards) {
        JsonObject cardObj = cardsArray.add<JsonObject>();
        cardObj["id"] = card.id;
        cardObj["type"] = card.type;
        cardObj["front"] = card.front;
        cardObj["back"] = card.back;
        cardObj["ef"] = card.ef;
        cardObj["interval"] = card.interval;
        cardObj["due"] = card.due;
        cardObj["reps"] = card.reps;
        cardObj["lapses"] = card.lapses;
    }

    // Serialize to file
    size_t written = serializeJson(doc, file);
    file.close();

    return written > 0;
}

bool SRSManager::loadCards() {
    if (!LittleFS.exists(CARDS_FILE)) {
        return false;
    }

    File file = LittleFS.open(CARDS_FILE, FILE_READ);
    if (!file) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        return false;
    }

    _nextId = doc["nextId"] | 1;
    _cards.clear();

    JsonArray cardsArray = doc["cards"].as<JsonArray>();
    for (JsonObject cardObj : cardsArray) {
        SRSCard card;
        card.id = cardObj["id"].as<String>();
        card.type = cardObj["type"].as<String>();
        card.front = cardObj["front"].as<String>();
        card.back = cardObj["back"].as<String>();
        card.ef = cardObj["ef"] | INITIAL_EF;
        card.interval = cardObj["interval"] | 0;
        card.due = cardObj["due"] | 0UL;
        card.reps = cardObj["reps"] | 0;
        card.lapses = cardObj["lapses"] | 0;

        _cards.push_back(card);
    }

    return true;
}
