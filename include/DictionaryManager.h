#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>

/**
 * DictionaryManager - Offline Japanese dictionary with binary search
 *
 * Uses Wiktionary data with FNV-1a hash index for fast O(log n) lookup.
 *
 * Dictionary Format:
 * - Index file (wikt_index.bin): hash(4) + offset(4) + length(2) per entry
 * - Data file (wikt_entries.dat): word\treading\tmeanings\tpos\n
 */

struct DictEntry {
    String word;           // 漢字 or kana
    String reading;        // ひらがな reading
    String meanings;       // Meanings (English)
    String partOfSpeech;   // 品詞 (noun, verb, etc.)
    String source;         // "wiktionary"
};

class DictionaryManager {
public:
    static DictionaryManager& instance() {
        static DictionaryManager inst;
        return inst;
    }

    /**
     * Initialize dictionary - loads index entry count
     */
    bool init();

    /**
     * Check if dictionary is available
     */
    bool isAvailable() const { return _available; }

    /**
     * Look up a word using binary search on hash index
     * O(log n) complexity
     * @param word Word to search (kanji or kana)
     * @return First matching entry (or empty if not found)
     */
    DictEntry lookupWord(const String& word);

    /**
     * Search for entries containing the query (limited scan)
     * Falls back to linear scan for partial matches
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
     * Calculate FNV-1a hash (same as Python converter)
     */
    uint32_t fnv1aHash(const String& s);

    /**
     * Binary search for hash in index file
     * @return offset in data file, or -1 if not found
     */
    int32_t binarySearchIndex(uint32_t targetHash, uint16_t& outLength);

    /**
     * Read entry from data file at offset
     */
    DictEntry readEntryAt(uint32_t offset, uint16_t length);

    /**
     * Parse a line from entries.dat into DictEntry
     */
    DictEntry parseLine(const String& line);

    bool _available = false;
    uint32_t _entryCount = 0;

    const char* _indexPath = "/dict/wikt_index.bin";
    const char* _dataPath = "/dict/wikt_entries.dat";
};
