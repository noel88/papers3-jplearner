#include "DictionaryManager.h"

bool DictionaryManager::init() {
    Serial.println("DictionaryManager: Initializing (SD-card-only mode)...");

    // Just check if dictionary file exists
    if (!SD.exists(_dataPath)) {
        Serial.printf("DictionaryManager: Data file not found: %s\n", _dataPath);
        _available = false;
        return false;
    }

    // Verify file is readable
    File f = SD.open(_dataPath, FILE_READ);
    if (!f) {
        Serial.printf("DictionaryManager: Cannot open data file: %s\n", _dataPath);
        _available = false;
        return false;
    }

    size_t fileSize = f.size();
    f.close();

    Serial.printf("DictionaryManager: Ready - %zu bytes (SD-card search mode)\n", fileSize);
    _available = true;
    return true;
}

DictEntry DictionaryManager::parseLine(const String& line) {
    DictEntry entry;
    entry.source = "jmdict";

    // Format: word\treading\tmeanings\tpos\n
    int tab1 = line.indexOf('\t');
    int tab2 = line.indexOf('\t', tab1 + 1);
    int tab3 = line.indexOf('\t', tab2 + 1);

    if (tab1 > 0) {
        entry.word = line.substring(0, tab1);
    }
    if (tab2 > tab1) {
        entry.reading = line.substring(tab1 + 1, tab2);
    }
    if (tab3 > tab2) {
        entry.meanings = line.substring(tab2 + 1, tab3);
        entry.partOfSpeech = line.substring(tab3 + 1);
        entry.partOfSpeech.trim();
    } else if (tab2 > 0) {
        entry.meanings = line.substring(tab2 + 1);
        entry.meanings.trim();
    }

    return entry;
}

DictEntry DictionaryManager::lookupWord(const String& word) {
    DictEntry result;

    if (!_available || word.isEmpty()) {
        return result;
    }

    Serial.printf("DictionaryManager: Looking up '%s'...\n", word.c_str());

    File dataFile = SD.open(_dataPath, FILE_READ);
    if (!dataFile) {
        Serial.println("DictionaryManager: Failed to open data file");
        return result;
    }

    String line;
    int linesScanned = 0;
    unsigned long startTime = millis();

    // Limit scan to 50000 lines or 3 seconds to avoid UI freeze
    const int MAX_LINES = 50000;
    const unsigned long MAX_TIME_MS = 3000;

    while (dataFile.available()) {
        line = dataFile.readStringUntil('\n');
        linesScanned++;

        // Check limits
        if (linesScanned > MAX_LINES || (millis() - startTime) > MAX_TIME_MS) {
            dataFile.close();
            Serial.printf("DictionaryManager: Scan limit reached (%d lines, %lu ms)\n",
                          linesScanned, millis() - startTime);
            return result;
        }

        // Quick check: does line start with the word?
        int tab1 = line.indexOf('\t');
        if (tab1 > 0) {
            String entryWord = line.substring(0, tab1);

            // Check exact match with word field
            if (entryWord == word) {
                dataFile.close();
                unsigned long elapsed = millis() - startTime;
                Serial.printf("DictionaryManager: Found '%s' in %lu ms (%d lines)\n",
                              word.c_str(), elapsed, linesScanned);
                return parseLine(line);
            }

            // Also check reading field (between tab1 and tab2)
            int tab2 = line.indexOf('\t', tab1 + 1);
            if (tab2 > tab1) {
                String reading = line.substring(tab1 + 1, tab2);
                if (reading == word) {
                    dataFile.close();
                    unsigned long elapsed = millis() - startTime;
                    Serial.printf("DictionaryManager: Found '%s' by reading in %lu ms (%d lines)\n",
                                  word.c_str(), elapsed, linesScanned);
                    return parseLine(line);
                }
            }
        }
    }

    dataFile.close();
    unsigned long elapsed = millis() - startTime;
    Serial.printf("DictionaryManager: '%s' not found (%lu ms, %d lines)\n",
                  word.c_str(), elapsed, linesScanned);

    return result;
}

std::vector<DictEntry> DictionaryManager::search(const String& query, int maxResults) {
    std::vector<DictEntry> results;

    if (!_available || query.isEmpty()) {
        return results;
    }

    Serial.printf("DictionaryManager: Searching '%s' (max %d)...\n", query.c_str(), maxResults);

    File dataFile = SD.open(_dataPath, FILE_READ);
    if (!dataFile) {
        Serial.println("DictionaryManager: Failed to open data file");
        return results;
    }

    String line;
    int linesScanned = 0;
    unsigned long startTime = millis();

    while (dataFile.available() && (int)results.size() < maxResults) {
        line = dataFile.readStringUntil('\n');
        linesScanned++;

        // Check if line contains query anywhere
        if (line.indexOf(query) >= 0) {
            results.push_back(parseLine(line));
        }

        // Safety limit: don't scan more than 100k lines for partial search
        if (linesScanned > 100000) {
            Serial.println("DictionaryManager: Search limit reached");
            break;
        }
    }

    dataFile.close();
    unsigned long elapsed = millis() - startTime;
    Serial.printf("DictionaryManager: Found %d results in %lu ms (%d lines)\n",
                  (int)results.size(), elapsed, linesScanned);

    return results;
}
