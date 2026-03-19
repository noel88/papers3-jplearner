#include "DictionaryManager.h"
#include "esp_heap_caps.h"

bool DictionaryManager::init() {
    if (_initialized) return true;

    Serial.println("DictionaryManager: Initializing...");

    if (!loadIndex()) {
        Serial.println("DictionaryManager: Failed to load index");
        return false;
    }

    _initialized = true;
    Serial.printf("DictionaryManager: Loaded %d entries, index size: %zu bytes\n",
                  _entryCount, _indexSize);
    return true;
}

bool DictionaryManager::loadIndex() {
    // Check if index file exists
    if (!SD.exists(INDEX_PATH)) {
        Serial.printf("DictionaryManager: Index file not found: %s\n", INDEX_PATH);
        return false;
    }

    File indexFile = SD.open(INDEX_PATH, FILE_READ);
    if (!indexFile) {
        Serial.println("DictionaryManager: Failed to open index file");
        return false;
    }

    // Read header (entry count)
    uint32_t entryCount;
    if (indexFile.read((uint8_t*)&entryCount, sizeof(entryCount)) != sizeof(entryCount)) {
        Serial.println("DictionaryManager: Failed to read entry count");
        indexFile.close();
        return false;
    }

    _entryCount = entryCount;
    _indexSize = entryCount * sizeof(IndexEntry);

    Serial.printf("DictionaryManager: Loading %d index entries (%zu bytes)\n",
                  entryCount, _indexSize);

    // Allocate index in PSRAM
    _index = (IndexEntry*)heap_caps_malloc(_indexSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_index) {
        Serial.println("DictionaryManager: Failed to allocate PSRAM for index");
        indexFile.close();
        return false;
    }

    // Read index data
    size_t bytesRead = indexFile.read((uint8_t*)_index, _indexSize);
    indexFile.close();

    if (bytesRead != _indexSize) {
        Serial.printf("DictionaryManager: Index read mismatch: %zu / %zu\n",
                      bytesRead, _indexSize);
        heap_caps_free(_index);
        _index = nullptr;
        return false;
    }

    return true;
}

uint32_t DictionaryManager::fnvHash(const String& str) {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    const char* data = str.c_str();
    while (*data) {
        hash ^= (uint8_t)*data++;
        hash *= 16777619u;
    }
    return hash;
}

std::vector<IndexEntry> DictionaryManager::binarySearch(uint32_t hash) {
    std::vector<IndexEntry> results;

    if (!_index || _entryCount == 0) return results;

    int left = 0;
    int right = _entryCount - 1;
    int found = -1;

    // Binary search for first match
    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (_index[mid].hash == hash) {
            found = mid;
            right = mid - 1;  // Continue searching left for first occurrence
        } else if (_index[mid].hash < hash) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (found < 0) return results;

    // Collect all entries with matching hash (handle collisions)
    for (int i = found; i < _entryCount && _index[i].hash == hash; i++) {
        results.push_back(_index[i]);
    }

    return results;
}

DictEntry DictionaryManager::readEntry(uint32_t offset, uint16_t length) {
    DictEntry entry;

    File dataFile = SD.open(DATA_PATH, FILE_READ);
    if (!dataFile) {
        Serial.println("DictionaryManager: Failed to open data file");
        return entry;
    }

    if (!dataFile.seek(offset)) {
        Serial.printf("DictionaryManager: Failed to seek to offset %u\n", offset);
        dataFile.close();
        return entry;
    }

    // Read entry data
    char* buffer = new char[length + 1];
    size_t bytesRead = dataFile.read((uint8_t*)buffer, length);
    buffer[bytesRead] = '\0';
    dataFile.close();

    // Parse entry: word\treading\tmeanings\tpos\n
    String line(buffer);
    delete[] buffer;

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
        // Remove trailing newline
        entry.partOfSpeech.trim();
    } else if (tab2 > 0) {
        entry.meanings = line.substring(tab2 + 1);
        entry.meanings.trim();
    }

    return entry;
}

std::vector<DictEntry> DictionaryManager::lookupByReading(const String& reading) {
    std::vector<DictEntry> results;

    if (!_initialized) return results;

    uint32_t hash = fnvHash(reading);
    std::vector<IndexEntry> indexMatches = binarySearch(hash);

    for (const auto& idx : indexMatches) {
        DictEntry entry = readEntry(idx.offset, idx.length);
        // Verify exact match (handle hash collisions)
        if (entry.reading == reading) {
            results.push_back(entry);
        }
    }

    return results;
}

std::vector<DictEntry> DictionaryManager::lookupByWord(const String& word) {
    std::vector<DictEntry> results;

    if (!_initialized) return results;

    // First try lookup by treating word as reading
    uint32_t hash = fnvHash(word);
    std::vector<IndexEntry> indexMatches = binarySearch(hash);

    for (const auto& idx : indexMatches) {
        DictEntry entry = readEntry(idx.offset, idx.length);
        // Match either word or reading
        if (entry.word == word || entry.reading == word) {
            results.push_back(entry);
        }
    }

    // If no results, we need a linear scan (slower but necessary for kanji lookup)
    // This is expensive, so limit to first 100 results
    if (results.empty() && SD.exists(DATA_PATH)) {
        File dataFile = SD.open(DATA_PATH, FILE_READ);
        if (dataFile) {
            String line;
            int count = 0;
            while (dataFile.available() && count < 100) {
                line = dataFile.readStringUntil('\n');
                int tab1 = line.indexOf('\t');
                if (tab1 > 0) {
                    String entryWord = line.substring(0, tab1);
                    if (entryWord == word) {
                        int tab2 = line.indexOf('\t', tab1 + 1);
                        int tab3 = line.indexOf('\t', tab2 + 1);

                        DictEntry entry;
                        entry.word = entryWord;
                        if (tab2 > tab1) {
                            entry.reading = line.substring(tab1 + 1, tab2);
                        }
                        if (tab3 > tab2) {
                            entry.meanings = line.substring(tab2 + 1, tab3);
                            entry.partOfSpeech = line.substring(tab3 + 1);
                        } else if (tab2 > 0) {
                            entry.meanings = line.substring(tab2 + 1);
                        }
                        results.push_back(entry);
                        count++;
                    }
                }
            }
            dataFile.close();
        }
    }

    return results;
}

std::vector<DictEntry> DictionaryManager::search(const String& query, int maxResults) {
    std::vector<DictEntry> results;

    if (!_initialized || !SD.exists(DATA_PATH)) return results;

    // Linear scan for partial matches (expensive but necessary)
    File dataFile = SD.open(DATA_PATH, FILE_READ);
    if (!dataFile) return results;

    String line;
    while (dataFile.available() && (int)results.size() < maxResults) {
        line = dataFile.readStringUntil('\n');

        // Check if line contains query
        if (line.indexOf(query) >= 0) {
            int tab1 = line.indexOf('\t');
            int tab2 = line.indexOf('\t', tab1 + 1);
            int tab3 = line.indexOf('\t', tab2 + 1);

            DictEntry entry;
            if (tab1 > 0) {
                entry.word = line.substring(0, tab1);
            }
            if (tab2 > tab1) {
                entry.reading = line.substring(tab1 + 1, tab2);
            }
            if (tab3 > tab2) {
                entry.meanings = line.substring(tab2 + 1, tab3);
                entry.partOfSpeech = line.substring(tab3 + 1);
            } else if (tab2 > 0) {
                entry.meanings = line.substring(tab2 + 1);
            }

            results.push_back(entry);
        }
    }

    dataFile.close();
    return results;
}
