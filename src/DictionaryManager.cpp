#include "DictionaryManager.h"
#include "esp_heap_caps.h"

bool DictionaryManager::init() {
    Serial.println("DictionaryManager: Initializing...");

    // Try to load JMdict
    if (loadIndex(_jmdict)) {
        Serial.printf("DictionaryManager: JMdict loaded - %d entries, %zu bytes\n",
                      _jmdict.entryCount, _jmdict.indexSize);
    } else {
        Serial.println("DictionaryManager: JMdict not available");
    }

    // Try to load Wiktionary
    if (loadIndex(_wiktionary)) {
        Serial.printf("DictionaryManager: Wiktionary loaded - %d entries, %zu bytes\n",
                      _wiktionary.entryCount, _wiktionary.indexSize);
    } else {
        Serial.println("DictionaryManager: Wiktionary not available");
    }

    if (!isAvailable()) {
        Serial.println("DictionaryManager: No dictionaries available");
        return false;
    }

    Serial.printf("DictionaryManager: Total %d entries available\n", getTotalEntryCount());
    return true;
}

bool DictionaryManager::loadIndex(DictSource& source) {
    // Check if index file exists
    if (!SD.exists(source.indexPath)) {
        Serial.printf("DictionaryManager: Index file not found: %s\n", source.indexPath);
        return false;
    }

    File indexFile = SD.open(source.indexPath, FILE_READ);
    if (!indexFile) {
        Serial.printf("DictionaryManager: Failed to open index file: %s\n", source.indexPath);
        return false;
    }

    // Read header (entry count)
    uint32_t entryCount;
    if (indexFile.read((uint8_t*)&entryCount, sizeof(entryCount)) != sizeof(entryCount)) {
        Serial.println("DictionaryManager: Failed to read entry count");
        indexFile.close();
        return false;
    }

    source.entryCount = entryCount;
    source.indexSize = entryCount * sizeof(IndexEntry);

    Serial.printf("DictionaryManager: Loading %d index entries (%zu bytes) from %s\n",
                  entryCount, source.indexSize, source.indexPath);

    // Allocate index in PSRAM
    source.index = (IndexEntry*)heap_caps_malloc(source.indexSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!source.index) {
        Serial.println("DictionaryManager: Failed to allocate PSRAM for index");
        indexFile.close();
        return false;
    }

    // Read index data
    size_t bytesRead = indexFile.read((uint8_t*)source.index, source.indexSize);
    indexFile.close();

    if (bytesRead != source.indexSize) {
        Serial.printf("DictionaryManager: Index read mismatch: %zu / %zu\n",
                      bytesRead, source.indexSize);
        heap_caps_free(source.index);
        source.index = nullptr;
        return false;
    }

    source.loaded = true;
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

std::vector<IndexEntry> DictionaryManager::binarySearch(const DictSource& source, uint32_t hash) {
    std::vector<IndexEntry> results;

    if (!source.index || source.entryCount == 0) return results;

    int left = 0;
    int right = source.entryCount - 1;
    int found = -1;

    // Binary search for first match
    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (source.index[mid].hash == hash) {
            found = mid;
            right = mid - 1;  // Continue searching left for first occurrence
        } else if (source.index[mid].hash < hash) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (found < 0) return results;

    // Collect all entries with matching hash (handle collisions)
    for (int i = found; i < source.entryCount && source.index[i].hash == hash; i++) {
        results.push_back(source.index[i]);
    }

    return results;
}

DictEntry DictionaryManager::readEntry(const DictSource& source, uint32_t offset, uint16_t length) {
    DictEntry entry;
    entry.source = source.name;

    File dataFile = SD.open(source.dataPath, FILE_READ);
    if (!dataFile) {
        Serial.printf("DictionaryManager: Failed to open data file: %s\n", source.dataPath);
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

    // JMdict is indexed by reading
    if (_jmdict.loaded) {
        uint32_t hash = fnvHash(reading);
        std::vector<IndexEntry> indexMatches = binarySearch(_jmdict, hash);

        for (const auto& idx : indexMatches) {
            DictEntry entry = readEntry(_jmdict, idx.offset, idx.length);
            // Verify exact match (handle hash collisions)
            if (entry.reading == reading) {
                results.push_back(entry);
            }
        }
    }

    return results;
}

std::vector<DictEntry> DictionaryManager::lookupByWord(const String& word) {
    std::vector<DictEntry> results;

    uint32_t hash = fnvHash(word);

    // Wiktionary is indexed by word - try first
    if (_wiktionary.loaded) {
        std::vector<IndexEntry> indexMatches = binarySearch(_wiktionary, hash);

        for (const auto& idx : indexMatches) {
            DictEntry entry = readEntry(_wiktionary, idx.offset, idx.length);
            // Verify exact match
            if (entry.word == word) {
                results.push_back(entry);
            }
        }
    }

    // Also try JMdict (indexed by reading)
    if (_jmdict.loaded) {
        std::vector<IndexEntry> indexMatches = binarySearch(_jmdict, hash);

        for (const auto& idx : indexMatches) {
            DictEntry entry = readEntry(_jmdict, idx.offset, idx.length);
            // Match either word or reading
            if (entry.word == word || entry.reading == word) {
                results.push_back(entry);
            }
        }
    }

    // If still no results and JMdict available, do linear scan for kanji
    if (results.empty() && _jmdict.loaded && SD.exists(_jmdict.dataPath)) {
        File dataFile = SD.open(_jmdict.dataPath, FILE_READ);
        if (dataFile) {
            String line;
            int count = 0;
            while (dataFile.available() && count < 50) {
                line = dataFile.readStringUntil('\n');
                int tab1 = line.indexOf('\t');
                if (tab1 > 0) {
                    String entryWord = line.substring(0, tab1);
                    if (entryWord == word) {
                        int tab2 = line.indexOf('\t', tab1 + 1);
                        int tab3 = line.indexOf('\t', tab2 + 1);

                        DictEntry entry;
                        entry.word = entryWord;
                        entry.source = "jmdict";
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

    // Search both dictionaries
    auto searchInSource = [&](const DictSource& source) {
        if (!source.loaded || !SD.exists(source.dataPath)) return;

        File dataFile = SD.open(source.dataPath, FILE_READ);
        if (!dataFile) return;

        String line;
        while (dataFile.available() && (int)results.size() < maxResults) {
            line = dataFile.readStringUntil('\n');

            // Check if line contains query
            if (line.indexOf(query) >= 0) {
                int tab1 = line.indexOf('\t');
                int tab2 = line.indexOf('\t', tab1 + 1);
                int tab3 = line.indexOf('\t', tab2 + 1);

                DictEntry entry;
                entry.source = source.name;
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
    };

    // Search Wiktionary first (usually more detailed)
    searchInSource(_wiktionary);

    // Then JMdict if we need more results
    if ((int)results.size() < maxResults) {
        searchInSource(_jmdict);
    }

    return results;
}
