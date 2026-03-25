#pragma once

#include <Arduino.h>
#include <vector>

/**
 * Grammar pattern item from JLPT grammar database
 */
struct GrammarItem {
    String pattern;     // Grammar pattern (e.g., "〜てもいい")
    String meaning;     // English meaning
    String level;       // JLPT level (N5, N4, N3, N2, N1)
    String example;     // Example sentence (Japanese)
    String exampleEn;   // Example sentence (English)
};

/**
 * GrammarManager - Manages JLPT grammar patterns database
 *
 * Loads grammar patterns from JSON file on SD card
 * and provides search functionality.
 */
class GrammarManager {
public:
    static GrammarManager& instance() {
        static GrammarManager inst;
        return inst;
    }

    /**
     * Initialize - load grammar database from SD card
     */
    bool init();

    /**
     * Check if initialized
     */
    bool isInitialized() const { return _initialized; }

    /**
     * Search grammar patterns
     * @param query Search string (matches pattern or meaning)
     * @param level Filter by JLPT level ("" for all)
     * @param limit Max results (0 for all)
     * @return Matching grammar items
     */
    std::vector<GrammarItem> search(const String& query, const String& level = "", int limit = 20);

    /**
     * Get grammar by exact pattern
     */
    GrammarItem* getByPattern(const String& pattern);

    /**
     * Detect grammar pattern in given text
     * @param text Text to analyze (e.g., "食べてもいいですか")
     * @return Matching grammar item, or nullptr if not found
     */
    GrammarItem* detectPattern(const String& text);

    /**
     * Get all patterns for a level
     */
    std::vector<GrammarItem> getByLevel(const String& level, int limit = 0);

    /**
     * Get total count
     */
    int getCount() const { return _items.size(); }

    /**
     * Get count by level
     */
    int getCountByLevel(const String& level) const;

private:
    GrammarManager() = default;
    ~GrammarManager() = default;
    GrammarManager(const GrammarManager&) = delete;
    GrammarManager& operator=(const GrammarManager&) = delete;

    std::vector<GrammarItem> _items;
    bool _initialized = false;

    static constexpr const char* GRAMMAR_FILE = "/dict/jlpt_grammar.json";
};
