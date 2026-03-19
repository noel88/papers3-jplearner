#pragma once

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>

/**
 * DictionaryManager - Offline Japanese dictionary with SD-based lookup
 *
 * Supports multiple dictionary sources:
 * - JMdict (Japanese-English, reading-based index)
 * - Wiktionary (word-based index, more detailed)
 *
 * Dictionary Format (optimized for ESP32):
 *
 * 1. Index file (dict/index.bin or dict/wikt_index.bin):
 *    - Sorted by hash (reading or word)
 *    - Each entry: hash(4) + file_offset(4) + entry_length(2)
 *    - Binary search for O(log n) lookup
 *
 * 2. Data file (dict/entries.dat or dict/wikt_entries.dat):
 *    - Variable-length entries
 *    - Format: word\treading\tmeanings\tpos\n
 *
 * Memory usage: Index loaded to PSRAM (~2-3MB)
 * Lookup: Binary search index → seek to file offset → read entry
 */

struct DictEntry {
    String word;           // 漢字 or kana
    String reading;        // ひらがな reading
    String meanings;       // Meanings (English)
    String partOfSpeech;   // 品詞 (noun, verb, etc.)
    String source;         // "jmdict" or "wikt"
};

struct IndexEntry {
    uint32_t hash;         // FNV-1a hash
    uint32_t offset;       // File offset in entries.dat
    uint16_t length;       // Entry length in bytes
} __attribute__((packed));

/**
 * Dictionary source info
 */
struct DictSource {
    bool loaded;
    int entryCount;
    size_t indexSize;
    IndexEntry* index;
    const char* indexPath;
    const char* dataPath;
    const char* name;
    bool hashByWord;       // true: hash by word, false: hash by reading
};

class DictionaryManager {
public:
    static DictionaryManager& instance() {
        static DictionaryManager inst;
        return inst;
    }

    /**
     * Initialize dictionary
     * Loads available dictionaries from SD card to PSRAM
     * @return true if at least one dictionary loaded successfully
     */
    bool init();

    /**
     * Check if dictionary is available
     */
    bool isAvailable() const { return _jmdict.loaded || _wiktionary.loaded; }

    /**
     * Check which dictionaries are available
     */
    bool hasJMdict() const { return _jmdict.loaded; }
    bool hasWiktionary() const { return _wiktionary.loaded; }

    /**
     * Look up a word by its reading (hiragana)
     * Uses JMdict (reading-indexed)
     * @param reading Hiragana reading to search
     * @return Vector of matching entries
     */
    std::vector<DictEntry> lookupByReading(const String& reading);

    /**
     * Look up a word by its kanji/kana form
     * Uses both dictionaries for best results
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
    int getJMdictEntryCount() const { return _jmdict.entryCount; }
    int getWiktionaryEntryCount() const { return _wiktionary.entryCount; }
    int getTotalEntryCount() const { return _jmdict.entryCount + _wiktionary.entryCount; }

private:
    DictionaryManager() = default;
    ~DictionaryManager() = default;
    DictionaryManager(const DictionaryManager&) = delete;
    DictionaryManager& operator=(const DictionaryManager&) = delete;

    /**
     * Load index file to PSRAM
     */
    bool loadIndex(DictSource& source);

    /**
     * Read entry from data file at given offset
     */
    DictEntry readEntry(const DictSource& source, uint32_t offset, uint16_t length);

    /**
     * Calculate FNV-1a hash of string
     */
    uint32_t fnvHash(const String& str);

    /**
     * Binary search in index
     * @return Vector of matching index entries (hash collisions possible)
     */
    std::vector<IndexEntry> binarySearch(const DictSource& source, uint32_t hash);

    // Dictionary sources
    DictSource _jmdict = {
        false, 0, 0, nullptr,
        "/dict/index.bin", "/dict/entries.dat",
        "jmdict", false  // hash by reading
    };

    DictSource _wiktionary = {
        false, 0, 0, nullptr,
        "/dict/wikt_index.bin", "/dict/wikt_entries.dat",
        "wikt", true  // hash by word
    };
};
