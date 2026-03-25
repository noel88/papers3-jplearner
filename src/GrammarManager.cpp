#include "GrammarManager.h"
#include <SD.h>
#include <ArduinoJson.h>

bool GrammarManager::init() {
    if (_initialized) {
        return true;
    }

    Serial.println("GrammarManager: Loading grammar database...");

    File file = SD.open(GRAMMAR_FILE, FILE_READ);
    if (!file) {
        Serial.println("GrammarManager: Grammar file not found");
        return false;
    }

    // Parse JSON - file is about 800KB, need streaming parser
    _items.clear();
    _items.reserve(700);  // ~646 items expected

    // Read file into buffer (small enough for direct parse)
    size_t fileSize = file.size();
    Serial.printf("GrammarManager: File size: %d bytes\n", fileSize);

    // Use streaming JSON parser for large file
    DynamicJsonDocument filter(64);
    filter[0]["pattern"] = true;
    filter[0]["meaning"] = true;
    filter[0]["level"] = true;
    filter[0]["examples"][0]["ja"] = true;
    filter[0]["examples"][0]["en"] = true;

    DynamicJsonDocument doc(1024 * 1024);  // 1MB buffer
    DeserializationError error = deserializeJson(doc, file, DeserializationOption::Filter(filter));
    file.close();

    if (error) {
        Serial.printf("GrammarManager: JSON parse error: %s\n", error.c_str());
        return false;
    }

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        GrammarItem item;
        item.pattern = obj["pattern"].as<String>();
        item.meaning = obj["meaning"].as<String>();
        item.level = obj["level"].as<String>();

        // Get first example if available
        JsonArray examples = obj["examples"];
        if (examples.size() > 0) {
            item.example = examples[0]["ja"].as<String>();
            item.exampleEn = examples[0]["en"].as<String>();
        }

        _items.push_back(item);
    }

    _initialized = true;
    Serial.printf("GrammarManager: Loaded %d grammar patterns\n", _items.size());

    return true;
}

std::vector<GrammarItem> GrammarManager::search(const String& query, const String& level, int limit) {
    std::vector<GrammarItem> results;

    if (!_initialized || query.length() == 0) {
        return results;
    }

    String queryLower = query;
    queryLower.toLowerCase();

    for (const auto& item : _items) {
        // Filter by level if specified
        if (level.length() > 0 && item.level != level) {
            continue;
        }

        // Check pattern match
        String patternLower = item.pattern;
        patternLower.toLowerCase();

        String meaningLower = item.meaning;
        meaningLower.toLowerCase();

        if (patternLower.indexOf(queryLower) >= 0 ||
            meaningLower.indexOf(queryLower) >= 0) {
            results.push_back(item);

            if (limit > 0 && results.size() >= limit) {
                break;
            }
        }
    }

    return results;
}

GrammarItem* GrammarManager::getByPattern(const String& pattern) {
    for (auto& item : _items) {
        if (item.pattern == pattern) {
            return &item;
        }
    }
    return nullptr;
}

GrammarItem* GrammarManager::detectPattern(const String& text) {
    if (!_initialized || text.length() == 0) {
        return nullptr;
    }

    Serial.printf("GrammarManager: Detecting pattern in '%s'\n", text.c_str());

    GrammarItem* bestMatch = nullptr;
    int bestMatchLen = 0;

    for (auto& item : _items) {
        // Get the core pattern by removing "〜" prefix
        String corePattern = item.pattern;
        corePattern.replace("〜", "");
        corePattern.replace("～", "");
        corePattern.trim();

        if (corePattern.length() == 0) {
            continue;
        }

        // Check if the pattern appears in the text
        if (text.indexOf(corePattern) >= 0) {
            int patternLen = corePattern.length();
            // Prefer longer matches (more specific patterns)
            if (patternLen > bestMatchLen) {
                bestMatch = &item;
                bestMatchLen = patternLen;
            }
        }

        // Also check if text is in the example sentence
        if (!bestMatch && item.example.length() > 0 && item.example.indexOf(text) >= 0) {
            bestMatch = &item;
            bestMatchLen = corePattern.length();
        }
    }

    if (bestMatch) {
        Serial.printf("GrammarManager: Found pattern '%s' (level: %s)\n",
                      bestMatch->pattern.c_str(), bestMatch->level.c_str());
    } else {
        Serial.println("GrammarManager: No pattern found");
    }

    return bestMatch;
}

std::vector<GrammarItem> GrammarManager::getByLevel(const String& level, int limit) {
    std::vector<GrammarItem> results;

    for (const auto& item : _items) {
        if (item.level == level) {
            results.push_back(item);

            if (limit > 0 && results.size() >= limit) {
                break;
            }
        }
    }

    return results;
}

int GrammarManager::getCountByLevel(const String& level) const {
    int count = 0;
    for (const auto& item : _items) {
        if (item.level == level) {
            count++;
        }
    }
    return count;
}
