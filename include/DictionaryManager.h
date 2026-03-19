#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>

/**
 * DictionaryManager - Offline Japanese dictionary with SD-based lookup
 *
 * Dictionary Format (optimized for ESP32):
 *
 * 1. Index file (dict/index.bin):
 *    - Sorted by reading (hiragana)
 *    - Each entry: reading_hash(4) + file_offset(4) + entry_length(2)
 *    - Binary search for O(log n) lookup
 *
 * 2. Data file (dict/entries.dat):
 *    - Variable-length entries
 *    - Format: word\treadings\tmeanings\n
 *    - Meanings can include Korean translations
 *
 * Memory usage: Only index (~2MB) loaded to PSRAM
 * Lookup: Binary search index → seek to file offset → read entry
 */

struct DictEntry {
    String word;           // 漢字 or kana
    String reading;        // ひらがな reading
    String meanings;       // Meanings (Korean/English)
    String partOfSpeech;   // 品詞 (noun, verb, etc.)
};

struct IndexEntry {
    uint32_t hash;         // FNV-1a hash of reading
    uint32_t offset;       // File offset in entries.dat
    uint16_t length;       // Entry length in bytes
} __attribute__((packed));

class DictionaryManager {
public:
    static DictionaryManager& instance() {
        static DictionaryManager inst;
        return inst;
    }

    /**
     * Initialize dictionary
     * Loads index from SD card to PSRAM
     * @return true if dictionary loaded successfully
     */
    bool init();

    /**
     * Check if dictionary is available
     */
    bool isAvailable() const { return _initialized; }

    /**
     * Look up a word by its reading (hiragana)
     * @param reading Hiragana reading to search
     * @return Vector of matching entries
     */
    std::vector<DictEntry> lookupByReading(const String& reading);

    /**
     * Look up a word by its kanji/kana form
     * @param word Word to search (kanji or kana)
     * @return Vector of matching entries
     */
    std::vector<DictEntry> lookupByWord(const String& word);

    /**
     * Search for entries containing the query
     * @param query Search query (partial match)
     * @param maxResults Maximum number of results
     * @return Vector of matching entries
     */
    std::vector<DictEntry> search(const String& query, int maxResults = 10);

    /**
     * Get dictionary stats
     */
    int getEntryCount() const { return _entryCount; }
    size_t getIndexSize() const { return _indexSize; }

private:
    DictionaryManager() = default;
    ~DictionaryManager() = default;
    DictionaryManager(const DictionaryManager&) = delete;
    DictionaryManager& operator=(const DictionaryManager&) = delete;

    /**
     * Load index file to PSRAM
     */
    bool loadIndex();

    /**
     * Read entry from data file at given offset
     */
    DictEntry readEntry(uint32_t offset, uint16_t length);

    /**
     * Calculate FNV-1a hash of string
     */
    uint32_t fnvHash(const String& str);

    /**
     * Binary search in index
     * @return Vector of matching index entries (hash collisions possible)
     */
    std::vector<IndexEntry> binarySearch(uint32_t hash);

    // State
    bool _initialized = false;
    int _entryCount = 0;
    size_t _indexSize = 0;

    // Index in PSRAM
    IndexEntry* _index = nullptr;

    // File paths
    static constexpr const char* INDEX_PATH = "/dict/index.bin";
    static constexpr const char* DATA_PATH = "/dict/entries.dat";
};
