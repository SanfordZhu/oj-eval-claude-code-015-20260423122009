#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <cstring>
#include <map>

using namespace std;
namespace fs = std::filesystem;

class FileStorage {
private:
    string dataFile = "storage.dat";
    string indexFile = "index.dat";

    struct IndexEntry {
        char index[65]; // 64 bytes + null terminator
        int64_t offset; // File offset for data
        int32_t count;  // Number of values

        IndexEntry() {
            memset(index, 0, sizeof(index));
            offset = 0;
            count = 0;
        }
    };

    vector<int> readValues(int64_t offset, int32_t count) {
        vector<int> values(count);

        ifstream data(dataFile, ios::binary);
        if (!data) return values;

        data.seekg(offset);
        for (int i = 0; i < count; i++) {
            data.read(reinterpret_cast<char*>(&values[i]), sizeof(int));
        }

        return values;
    }

    vector<IndexEntry> readIndex() {
        vector<IndexEntry> index;

        if (!fs::exists(indexFile)) {
            return index;
        }

        ifstream idx(indexFile, ios::binary);
        if (!idx) return index;

        int32_t size;
        idx.read(reinterpret_cast<char*>(&size), sizeof(size));

        index.resize(size);
        for (int i = 0; i < size; i++) {
            idx.read(reinterpret_cast<char*>(&index[i]), sizeof(IndexEntry));
        }

        return index;
    }

    void writeIndex(const vector<IndexEntry>& index) {
        ofstream idx(indexFile, ios::binary);
        if (!idx) return;

        int32_t size = index.size();
        idx.write(reinterpret_cast<const char*>(&size), sizeof(size));

        for (const auto& entry : index) {
            idx.write(reinterpret_cast<const char*>(&entry), sizeof(IndexEntry));
        }
    }

public:
    FileStorage() {
        // Create empty files if they don't exist
        if (!fs::exists(dataFile)) {
            ofstream data(dataFile, ios::binary);
        }
        if (!fs::exists(indexFile)) {
            ofstream idx(indexFile, ios::binary);
            int32_t zero = 0;
            idx.write(reinterpret_cast<const char*>(&zero), sizeof(zero));
        }
    }

    void insert(const string& index, int value) {
        auto idx = readIndex();

        // Find the index entry
        auto it = find_if(idx.begin(), idx.end(), [&](const IndexEntry& e) {
            return string(e.index) == index;
        });

        vector<int> values;

        if (it != idx.end()) {
            // Read existing values
            values = readValues(it->offset, it->count);
        }

        // Check if value already exists
        auto vit = lower_bound(values.begin(), values.end(), value);
        if (vit != values.end() && *vit == value) {
            return; // Value already exists
        }

        // Insert new value
        values.insert(vit, value);

        // Always append to the end of the data file
        ofstream data(dataFile, ios::binary | ios::app);
        if (!data) return;

        int64_t offset = data.tellp();
        for (int v : values) {
            data.write(reinterpret_cast<const char*>(&v), sizeof(int));
        }

        if (it != idx.end()) {
            // Update existing entry
            it->offset = offset;
            it->count = values.size();
        } else {
            // Add new index entry
            IndexEntry newEntry;
            strncpy(newEntry.index, index.c_str(), 64);
            newEntry.index[64] = '\0';
            newEntry.offset = offset;
            newEntry.count = values.size();
            idx.push_back(newEntry);

            // Sort index by index name
            sort(idx.begin(), idx.end(), [](const IndexEntry& a, const IndexEntry& b) {
                return strcmp(a.index, b.index) < 0;
            });
        }

        writeIndex(idx);
    }

    void remove(const string& index, int value) {
        auto idx = readIndex();

        // Find the index entry
        auto it = find_if(idx.begin(), idx.end(), [&](const IndexEntry& e) {
            return string(e.index) == index;
        });

        if (it == idx.end()) return;

        // Read values
        vector<int> values = readValues(it->offset, it->count);

        // Find and remove value
        auto vit = lower_bound(values.begin(), values.end(), value);
        if (vit != values.end() && *vit == value) {
            values.erase(vit);

            if (values.empty()) {
                // Remove the index entry entirely
                idx.erase(it);
                writeIndex(idx);
            } else {
                // Write back
                fstream data(dataFile, ios::binary | ios::in | ios::out);
                if (!data) return;

                data.seekp(it->offset);
                for (int v : values) {
                    data.write(reinterpret_cast<const char*>(&v), sizeof(int));
                }

                it->count = values.size();
                writeIndex(idx);
            }
        }
    }

    string find(const string& index) {
        auto idx = readIndex();

        // Find the index entry
        auto it = find_if(idx.begin(), idx.end(), [&](const IndexEntry& e) {
            return string(e.index) == index;
        });

        if (it == idx.end()) {
            return "null";
        }

        vector<int> values = readValues(it->offset, it->count);

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