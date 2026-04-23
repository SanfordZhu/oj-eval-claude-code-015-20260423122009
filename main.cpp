#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <cstring>

using namespace std;
namespace fs = std::filesystem;

class FileStorage {
private:
    string baseDir;
    static const int BUCKET_COUNT = 100; // Use 100 buckets to reduce file count

    int getBucketIndex(const string& index) {
        // Simple hash function
        int hash = 0;
        for (char c : index) {
            hash = hash * 31 + c;
        }
        return abs(hash) % BUCKET_COUNT;
    }

    string getBucketFilename(int bucketIdx) {
        return baseDir + "/bucket_" + to_string(bucketIdx) + ".dat";
    }

    struct Entry {
        char index[65]; // 64 bytes + null terminator
        int value;

        Entry() {
            memset(index, 0, sizeof(index));
            value = 0;
        }

        Entry(const string& idx, int val) {
            strncpy(index, idx.c_str(), 64);
            index[64] = '\0';
            value = val;
        }
    };

    vector<Entry> readBucket(int bucketIdx) {
        vector<Entry> entries;
        string filename = getBucketFilename(bucketIdx);

        if (!fs::exists(filename)) {
            return entries;
        }

        ifstream inFile(filename, ios::binary);
        if (!inFile) {
            return entries;
        }

        int size;
        inFile.read(reinterpret_cast<char*>(&size), sizeof(size));

        entries.resize(size);
        for (int i = 0; i < size; i++) {
            inFile.read(reinterpret_cast<char*>(&entries[i]), sizeof(Entry));
        }

        inFile.close();
        return entries;
    }

    void writeBucket(int bucketIdx, const vector<Entry>& entries) {
        string filename = getBucketFilename(bucketIdx);

        ofstream outFile(filename, ios::binary);
        if (!outFile) {
            return;
        }

        int size = entries.size();
        outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));

        for (const Entry& entry : entries) {
            outFile.write(reinterpret_cast<const char*>(&entry), sizeof(Entry));
        }

        outFile.close();
    }

public:
    FileStorage() {
        baseDir = "data";
        // Create data directory if it doesn't exist
        if (!fs::exists(baseDir)) {
            fs::create_directory(baseDir);
        }
    }

    void insert(const string& index, int value) {
        int bucketIdx = getBucketIndex(index);
        vector<Entry> entries = readBucket(bucketIdx);

        // Check if entry already exists
        bool found = false;
        for (const Entry& entry : entries) {
            if (string(entry.index) == index && entry.value == value) {
                found = true;
                break;
            }
        }

        if (found) {
            return; // Entry already exists
        }

        // Add new entry
        entries.emplace_back(index, value);

        // Sort entries by index and then by value
        sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
            int cmp = strcmp(a.index, b.index);
            if (cmp != 0) return cmp < 0;
            return a.value < b.value;
        });

        writeBucket(bucketIdx, entries);
    }

    void remove(const string& index, int value) {
        int bucketIdx = getBucketIndex(index);
        vector<Entry> entries = readBucket(bucketIdx);

        // Find and remove the entry
        auto it = find_if(entries.begin(), entries.end(), [&](const Entry& entry) {
            return string(entry.index) == index && entry.value == value;
        });

        if (it != entries.end()) {
            entries.erase(it);
            writeBucket(bucketIdx, entries);
        }
    }

    string find(const string& index) {
        int bucketIdx = getBucketIndex(index);
        vector<Entry> entries = readBucket(bucketIdx);

        vector<int> values;
        for (const Entry& entry : entries) {
            if (string(entry.index) == index) {
                values.push_back(entry.value);
            }
        }

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