#include "DictionaryManager.h"

bool DictionaryManager::init() {
    Serial.println("DictionaryManager: Initializing (Wiktionary binary search mode)...");

    // Check if index file exists
    if (!SD.exists(_indexPath)) {
        Serial.printf("DictionaryManager: Index file not found: %s\n", _indexPath);
        _available = false;
        return false;
    }

    // Check if data file exists
    if (!SD.exists(_dataPath)) {
        Serial.printf("DictionaryManager: Data file not found: %s\n", _dataPath);
        _available = false;
        return false;
    }

    // Read entry count from index file header
    File indexFile = SD.open(_indexPath, FILE_READ);
    if (!indexFile) {
        Serial.println("DictionaryManager: Cannot open index file");
        _available = false;
        return false;
    }

    // First 4 bytes = entry count (little-endian)
    uint8_t buf[4];
    if (indexFile.read(buf, 4) != 4) {
        Serial.println("DictionaryManager: Cannot read entry count");
        indexFile.close();
        _available = false;
        return false;
    }

    _entryCount = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    indexFile.close();

    Serial.printf("DictionaryManager: Ready - %u entries (binary search mode)\n", _entryCount);
    _available = true;
    return true;
}

uint32_t DictionaryManager::fnv1aHash(const String& s) {
    uint32_t hash = 2166136261u;
    const char* str = s.c_str();
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

int32_t DictionaryManager::binarySearchIndex(uint32_t targetHash, uint16_t& outLength) {
    if (_entryCount == 0) return -1;

    File indexFile = SD.open(_indexPath, FILE_READ);
    if (!indexFile) {
        Serial.println("DictionaryManager: Cannot open index for search");
        return -1;
    }

    // Index structure: 4 bytes header + (10 bytes per entry)
    // Each entry: hash(4) + offset(4) + length(2)
    const size_t HEADER_SIZE = 4;
    const size_t ENTRY_SIZE = 10;

    uint32_t left = 0;
    uint32_t right = _entryCount - 1;
    int32_t result = -1;

    uint8_t entryBuf[10];

    while (left <= right) {
        uint32_t mid = left + (right - left) / 2;

        // Seek to entry
        size_t entryPos = HEADER_SIZE + mid * ENTRY_SIZE;
        if (!indexFile.seek(entryPos)) {
            break;
        }

        // Read entry
        if (indexFile.read(entryBuf, 10) != 10) {
            break;
        }

        // Parse hash (little-endian)
        uint32_t hash = entryBuf[0] | (entryBuf[1] << 8) | (entryBuf[2] << 16) | (entryBuf[3] << 24);

        if (hash == targetHash) {
            // Found! Extract offset and length
            uint32_t offset = entryBuf[4] | (entryBuf[5] << 8) | (entryBuf[6] << 16) | (entryBuf[7] << 24);
            outLength = entryBuf[8] | (entryBuf[9] << 8);
            result = (int32_t)offset;
            break;
        } else if (hash < targetHash) {
            left = mid + 1;
        } else {
            if (mid == 0) break;
            right = mid - 1;
        }
    }

    indexFile.close();
    return result;
}

DictEntry DictionaryManager::readEntryAt(uint32_t offset, uint16_t length) {
    DictEntry entry;

    File dataFile = SD.open(_dataPath, FILE_READ);
    if (!dataFile) {
        return entry;
    }

    if (!dataFile.seek(offset)) {
        dataFile.close();
        return entry;
    }

    // Read entry data
    char* buf = (char*)malloc(length + 1);
    if (!buf) {
        dataFile.close();
        return entry;
    }

    size_t bytesRead = dataFile.read((uint8_t*)buf, length);
    dataFile.close();

    if (bytesRead > 0) {
        buf[bytesRead] = '\0';
        String line(buf);
        entry = parseLine(line);
    }

    free(buf);
    return entry;
}

DictEntry DictionaryManager::parseLine(const String& line) {
    DictEntry entry;
    entry.source = "wiktionary";

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

    unsigned long startTime = millis();
    Serial.printf("DictionaryManager: Looking up '%s'...\n", word.c_str());

    // Calculate hash and binary search
    uint32_t hash = fnv1aHash(word);
    uint16_t length = 0;

    int32_t offset = binarySearchIndex(hash, length);

    if (offset >= 0) {
        result = readEntryAt((uint32_t)offset, length);

        // Verify it's actually the right word (hash collision check)
        if (result.word != word) {
            Serial.printf("DictionaryManager: Hash collision - found '%s' instead of '%s'\n",
                          result.word.c_str(), word.c_str());
            // Clear result on collision
            result = DictEntry();
        }
    }

    unsigned long elapsed = millis() - startTime;

    if (!result.word.isEmpty()) {
        Serial.printf("DictionaryManager: Found '%s' in %lu ms\n", word.c_str(), elapsed);
    } else {
        Serial.printf("DictionaryManager: '%s' not found (%lu ms)\n", word.c_str(), elapsed);
    }

    return result;
}

std::vector<DictEntry> DictionaryManager::search(const String& query, int maxResults) {
    std::vector<DictEntry> results;

    if (!_available || query.isEmpty()) {
        return results;
    }

    Serial.printf("DictionaryManager: Searching '%s' (max %d)...\n", query.c_str(), maxResults);

    // First try exact match with binary search
    DictEntry exact = lookupWord(query);
    if (!exact.word.isEmpty()) {
        results.push_back(exact);
        if ((int)results.size() >= maxResults) {
            return results;
        }
    }

    // For partial matches, fall back to limited linear scan
    File dataFile = SD.open(_dataPath, FILE_READ);
    if (!dataFile) {
        return results;
    }

    String line;
    int linesScanned = 0;
    unsigned long startTime = millis();

    while (dataFile.available() && (int)results.size() < maxResults) {
        line = dataFile.readStringUntil('\n');
        linesScanned++;

        // Check if line contains query anywhere (skip exact match we already have)
        if (line.indexOf(query) >= 0) {
            DictEntry entry = parseLine(line);
            // Avoid duplicates
            if (entry.word != query) {
                results.push_back(entry);
            }
        }

        // Safety limit
        if (linesScanned > 50000) {
            break;
        }
    }

    dataFile.close();
    unsigned long elapsed = millis() - startTime;
    Serial.printf("DictionaryManager: Found %d results in %lu ms (%d lines scanned)\n",
                  (int)results.size(), elapsed, linesScanned);

    return results;
}
