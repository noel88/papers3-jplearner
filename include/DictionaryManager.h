#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>

/**
 * DictionaryManager - Offline Japanese dictionary with SD-based lookup
 *
 * Memory-efficient version: searches directly from SD card without loading index
 *
 * Dictionary Format:
 * - Data file (dict/entries.dat): word\treading\tmeanings\tpos\n
 * - Lookup: Linear scan through file (slower but no PSRAM needed)
 */

struct DictEntry {
    String word;           // 漢字 or kana
    String reading;        // ひらがな reading
    String meanings;       // Meanings (English)
    String partOfSpeech;   // 品詞 (noun, verb, etc.)
    String source;         // "jmdict"
};

class DictionaryManager {
public:
    static DictionaryManager& instance() {
        static DictionaryManager inst;
        return inst;
    }

    /**
     * Initialize dictionary
     * Just checks if dictionary file exists
     */
    bool init();

    /**
     * Check if dictionary is available
     */
    bool isAvailable() const { return _available; }

    /**
     * Look up a word directly from SD card
     * Searches through the data file for exact match
     * @param word Word to search (kanji or kana)
     * @return First matching entry (or empty if not found)
     */
    DictEntry lookupWord(const String& word);

    /**
     * Search for entries containing the query (limited scan)
     * @param query Search query
     * @param maxResults Maximum number of results
     * @return Vector of matching entries
     */
    std::vector<DictEntry> search(const String& query, int maxResults = 5);

private:
    DictionaryManager() = default;
    ~DictionaryManager() = default;
    DictionaryManager(const DictionaryManager&) = delete;
    DictionaryManager& operator=(const DictionaryManager&) = delete;

    /**
     * Parse a line from entries.dat into DictEntry
     */
    DictEntry parseLine(const String& line);

    bool _available = false;
    const char* _dataPath = "/dict/entries.dat";
};
