#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <map>

using namespace std;
namespace fs = std::filesystem;

class FileStorage {
private:
    string dataFile = "storage.dat";

    // In-memory buffer for batching operations
    map<string, vector<int>> buffer;
    bool bufferLoaded = false;
    int operationCount = 0;
    static const int WRITE_THRESHOLD = 10000; // Write after every 10k operations

    void loadAllData() {
        if (bufferLoaded) return;

        if (!fs::exists(dataFile)) {
            bufferLoaded = true;
            return;
        }

        ifstream in(dataFile, ios::binary);
        if (!in) {
            bufferLoaded = true;
            return;
        }

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

            // Read values
            vector<int>& values = buffer[string(indexName)];
            values.resize(numValues);
            for (int j = 0; j < numValues; j++) {
                in.read(reinterpret_cast<char*>(&values[j]), sizeof(int));
            }
        }

        bufferLoaded = true;
    }

    void saveAllData() {
        ofstream out(dataFile, ios::binary);
        if (!out) return;

        // Write number of indices
        int32_t numIndices = buffer.size();
        out.write(reinterpret_cast<const char*>(&numIndices), sizeof(numIndices));

        // Write each index and its values
        for (const auto& [index, values] : buffer) {
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

        operationCount = 0; // Reset counter after write
    }

public:
    ~FileStorage() {
        if (bufferLoaded) {
            saveAllData();
        }
    }

    // Force save (for testing)
    void forceSave() {
        if (bufferLoaded) {
            saveAllData();
        }
    }

    void insert(const string& index, int value) {
        loadAllData();

        vector<int>& values = buffer[index];

        // Check if value already exists
        auto it = lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            return; // Value already exists
        }

        // Insert in sorted order
        values.insert(it, value);

        // Check if we should write to disk
        operationCount++;
        if (operationCount >= WRITE_THRESHOLD) {
            saveAllData();
        }
    }

    void remove(const string& index, int value) {
        loadAllData();

        auto it = buffer.find(index);
        if (it == buffer.end()) return;

        vector<int>& values = it->second;

        // Find and remove value
        auto vit = lower_bound(values.begin(), values.end(), value);
        if (vit != values.end() && *vit == value) {
            values.erase(vit);

            // If no values left, remove the index
            if (values.empty()) {
                buffer.erase(it);
            }
        }

        // Check if we should write to disk
        operationCount++;
        if (operationCount >= WRITE_THRESHOLD) {
            saveAllData();
        }
    }

    string find(const string& index) {
        loadAllData();

        auto it = buffer.find(index);
        if (it == buffer.end()) {
            return "null";
        }

        const vector<int>& values = it->second;

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