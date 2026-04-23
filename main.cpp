#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <unordered_map>
#include <list>

using namespace std;
namespace fs = std::filesystem;

class FileStorage {
private:
    string dataFile = "storage.dat";
    static const int CACHE_SIZE = 1000; // Cache up to 1000 indices

    struct CacheEntry {
        string index;
        vector<int> values;
        bool dirty = false;  // Has been modified
    };

    // LRU cache implementation
    unordered_map<string, list<CacheEntry>::iterator> cacheMap;
    list<CacheEntry> cacheList;

    // Helper to read a single index from file
    vector<int> readIndexFromFile(const string& index) {
        vector<int> values;

        if (!fs::exists(dataFile)) {
            return values;
        }

        ifstream in(dataFile, ios::binary);
        if (!in) return values;

        // Read number of indices
        int32_t numIndices;
        in.read(reinterpret_cast<char*>(&numIndices), sizeof(numIndices));

        for (int i = 0; i < numIndices; i++) {
            // Read index name
            char indexName[65];
            in.read(indexName, 64);
            indexName[64] = '\0';

            // Read number of values
            int32_t numValues;
            in.read(reinterpret_cast<char*>(&numValues), sizeof(numValues));

            if (string(indexName) == index) {
                // Found our index, read values
                values.resize(numValues);
                for (int j = 0; j < numValues; j++) {
                    in.read(reinterpret_cast<char*>(&values[j]), sizeof(int));
                }
                return values;
            } else {
                // Skip this index's values
                in.seekg(numValues * sizeof(int), ios::cur);
            }
        }

        return values;
    }

    // Helper to write entire file (only when necessary)
    void writeFile() {
        ofstream out(dataFile, ios::binary);
        if (!out) return;

        // First collect all indices (from cache and file)
        unordered_map<string, vector<int>> allData;

        // Read existing data from file
        if (fs::exists(dataFile)) {
            ifstream in(dataFile, ios::binary);
            if (in) {
                int32_t numIndices;
                in.read(reinterpret_cast<char*>(&numIndices), sizeof(numIndices));

                for (int i = 0; i < numIndices; i++) {
                    char indexName[65];
                    in.read(indexName, 64);
                    indexName[64] = '\0';

                    int32_t numValues;
                    in.read(reinterpret_cast<char*>(&numValues), sizeof(numValues));

                    // Check if this index is in cache and dirty
                    string idx(indexName);
                    auto cacheIt = cacheMap.find(idx);
                    if (cacheIt != cacheMap.end() && cacheIt->second->dirty) {
                        // Use cached values
                        allData[idx] = cacheIt->second->values;
                        // Mark as not dirty since we're writing
                        cacheIt->second->dirty = false;
                    } else {
                        // Read from file
                        vector<int>& values = allData[idx];
                        values.resize(numValues);
                        for (int j = 0; j < numValues; j++) {
                            in.read(reinterpret_cast<char*>(&values[j]), sizeof(int));
                        }
                    }
                }
            }
        }

        // Add any new indices from cache
        for (auto& entry : cacheList) {
            if (entry.dirty && allData.find(entry.index) == allData.end()) {
                allData[entry.index] = entry.values;
                entry.dirty = false;
            }
        }

        // Write everything back
        int32_t numIndices = allData.size();
        out.write(reinterpret_cast<const char*>(&numIndices), sizeof(numIndices));

        for (const auto& [index, values] : allData) {
            // Write index name (fixed 64 bytes)
            char indexName[65] = {0};
            strncpy(indexName, index.c_str(), 64);
            out.write(indexName, 64);

            // Write number of values
            int32_t numValues = values.size();
            out.write(reinterpret_cast<const char*>(&numValues), sizeof(numValues));

            // Write values
            for (int value : values) {
                out.write(reinterpret_cast<const char*>(&value), sizeof(int));
            }
        }
    }

    // Add to cache with LRU eviction
    void addToCache(const string& index, const vector<int>& values, bool dirty = false) {
        auto it = cacheMap.find(index);
        if (it != cacheMap.end()) {
            // Update existing entry
            it->second->values = values;
            it->second->dirty = dirty;
            // Move to front
            cacheList.splice(cacheList.begin(), cacheList, it->second);
        } else {
            // Add new entry
            if (cacheList.size() >= CACHE_SIZE) {
                // Evict least recently used
                auto& lru = cacheList.back();
                if (lru.dirty) {
                    // Must write back before evicting
                    writeFile();
                }
                cacheMap.erase(lru.index);
                cacheList.pop_back();
            }

            cacheList.emplace_front(CacheEntry{index, values, dirty});
            cacheMap[index] = cacheList.begin();
        }
    }

    // Get from cache or file
    vector<int> getValues(const string& index) {
        auto it = cacheMap.find(index);
        if (it != cacheMap.end()) {
            // Move to front (LRU)
            cacheList.splice(cacheList.begin(), cacheList, it->second);
            return it->second->values;
        }

        // Not in cache, read from file
        vector<int> values = readIndexFromFile(index);
        if (!values.empty()) {
            addToCache(index, values);
        }
        return values;
    }

public:
    ~FileStorage() {
        // Write back any dirty entries
        bool hasDirty = false;
        for (const auto& entry : cacheList) {
            if (entry.dirty) {
                hasDirty = true;
                break;
            }
        }
        if (hasDirty) {
            writeFile();
        }
    }

    void insert(const string& index, int value) {
        vector<int> values = getValues(index);

        // Check if value already exists
        auto it = lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            return; // Value already exists
        }

        // Insert in sorted order
        values.insert(it, value);

        // Update cache
        addToCache(index, values, true);
    }

    void remove(const string& index, int value) {
        vector<int> values = getValues(index);
        if (values.empty()) return;

        // Find and remove value
        auto it = lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            values.erase(it);

            if (values.empty()) {
                // Remove from cache
                auto cacheIt = cacheMap.find(index);
                if (cacheIt != cacheMap.end()) {
                    cacheList.erase(cacheIt->second);
                    cacheMap.erase(cacheIt);
                }
                // Will be removed from file on next write
                writeFile();
            } else {
                // Update cache
                addToCache(index, values, true);
            }
        }
    }

    string find(const string& index) {
        vector<int> values = getValues(index);

        if (values.empty()) {
            return "null";
        }

        stringstream ss;
        for (size_t i = 0; i < values.size(); i++) {
            if (i > 0) ss << " ";
            ss << values[i];
        }

        return ss.str();
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    FileStorage storage;

    int n;
    cin >> n;
    cin.ignore();

    for (int i = 0; i < n; i++) {
        string line;
        getline(cin, line);

        stringstream ss(line);
        string command;
        ss >> command;

        if (command == "insert") {
            string index;
            int value;
            ss >> index >> value;
            storage.insert(index, value);
        } else if (command == "delete") {
            string index;
            int value;
            ss >> index >> value;
            storage.remove(index, value);
        } else if (command == "find") {
            string index;
            ss >> index;
            string result = storage.find(index);
            cout << result << "\n";
        }
    }

    return 0;
}