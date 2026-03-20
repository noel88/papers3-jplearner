#pragma once

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>

/**
 * SRS Card - Single flashcard for spaced repetition
 */
struct SRSCard {
    String id;          // Unique identifier
    String type;        // "word" or "grammar"
    String front;       // Question/word
    String back;        // Answer/meaning
    float ef;           // Easiness Factor (1.3 ~ 2.5+)
    int interval;       // Current interval in days
    unsigned long due;  // Due timestamp (Unix time)
    int reps;           // Successful repetitions in a row
    int lapses;         // Number of times forgotten

    SRSCard() : ef(2.5f), interval(0), due(0), reps(0), lapses(0) {}
};

/**
 * User response to a card
 */
enum class SRSResponse {
    AGAIN = 0,  // Forgot / Wrong
    HARD = 1,   // Difficult but remembered
    GOOD = 2,   // Normal recall
    EASY = 3    // Easy recall
};

/**
 * Learning statistics
 */
struct SRSStats {
    int streak;              // Consecutive study days
    int todayReviews;        // Reviews completed today
    int todayNew;            // New cards studied today
    int todayCorrect;        // Correct answers today
    int weeklyReviews[7];    // Reviews per day (last 7 days)
    unsigned long lastStudyDate;  // Last study date (Unix timestamp)
};

/**
 * SRSManager - Spaced Repetition System using SM-2 algorithm
 *
 * Features:
 * - SM-2 algorithm for interval calculation
 * - Card storage in JSON format
 * - Due card filtering
 * - Statistics tracking
 */
class SRSManager {
public:
    static SRSManager& instance() {
        static SRSManager inst;
        return inst;
    }

    /**
     * Initialize SRS system
     * Loads cards from storage
     */
    void init();

    /**
     * Add a new card
     * @param type "word" or "grammar"
     * @param front Question text
     * @param back Answer text
     * @return Card ID
     */
    String addCard(const String& type, const String& front, const String& back);

    /**
     * Check if card already exists
     */
    bool hasCard(const String& front, const String& type);

    /**
     * Process user response to a card
     * Updates interval and due date using SM-2 algorithm
     */
    void processResponse(const String& cardId, SRSResponse response);

    /**
     * Get cards due for review
     * @param type Filter by type ("word", "grammar", or "" for all)
     * @param limit Maximum number to return (0 = all)
     */
    std::vector<SRSCard*> getDueCards(const String& type = "", int limit = 0);

    /**
     * Get new cards (never reviewed)
     */
    std::vector<SRSCard*> getNewCards(const String& type = "", int limit = 0);

    /**
     * Get card by ID
     */
    SRSCard* getCard(const String& id);

    /**
     * Get total card count
     */
    int getCardCount(const String& type = "");

    /**
     * Get due card count
     */
    int getDueCount(const String& type = "");

    /**
     * Get new card count
     */
    int getNewCount(const String& type = "");

    /**
     * Save cards to storage
     */
    bool saveCards();

    /**
     * Load cards from storage
     */
    bool loadCards();

    /**
     * Get current Unix timestamp
     */
    unsigned long getCurrentTime();

    /**
     * Get learning statistics
     */
    SRSStats getStats();

    /**
     * Record a review (call after processResponse)
     * @param correct Whether the response was correct (GOOD or EASY)
     * @param isNew Whether this was a new card
     */
    void recordReview(bool correct, bool isNew);

    /**
     * Get mastered card count (interval >= 21 days)
     */
    int getMasteredCount(const String& type = "");

    /**
     * Get due count for specific day offset
     * @param dayOffset 0=today, 1=tomorrow, etc.
     */
    int getDueCountForDay(int dayOffset, const String& type = "");

private:
    SRSManager() = default;
    ~SRSManager() = default;
    SRSManager(const SRSManager&) = delete;
    SRSManager& operator=(const SRSManager&) = delete;

    std::vector<SRSCard> _cards;
    int _nextId = 1;
    SRSStats _stats;

    // Stats helpers
    void loadStats();
    void saveStats();
    void updateStreak();
    int getDayOfYear(unsigned long timestamp);

    // SM-2 algorithm helpers
    float calculateNewEF(float oldEF, SRSResponse response);
    int calculateNewInterval(const SRSCard& card, SRSResponse response);
    unsigned long calculateDueTime(int intervalDays);

    // Storage paths
    static constexpr const char* CARDS_FILE = "/userdata/cards.json";
    static constexpr const char* STATS_FILE = "/userdata/stats.json";

    // SM-2 constants
    static constexpr float MIN_EF = 1.3f;
    static constexpr float INITIAL_EF = 2.5f;

    // Initial intervals for new cards (in minutes)
    static constexpr int NEW_AGAIN_MINUTES = 1;
    static constexpr int NEW_HARD_MINUTES = 6;
    static constexpr int NEW_GOOD_MINUTES = 10;
    static constexpr int NEW_EASY_DAYS = 4;
};
