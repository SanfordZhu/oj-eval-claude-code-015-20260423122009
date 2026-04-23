#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

class FileStorage {
private:
    string baseDir;

    string getFilename(const string& index) {
        // Replace special characters to make valid filenames
        string safeIndex = index;
        for (char& c : safeIndex) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        return baseDir + "/" + safeIndex + ".dat";
    }

    vector<int> readValues(const string& index) {
        vector<int> values;
        string filename = getFilename(index);

        if (!fs::exists(filename)) {
            return values;
        }

        ifstream inFile(filename, ios::binary);
        if (!inFile) {
            return values;
        }

        int size;
        inFile.read(reinterpret_cast<char*>(&size), sizeof(size));

        values.resize(size);
        for (int i = 0; i < size; i++) {
            inFile.read(reinterpret_cast<char*>(&values[i]), sizeof(values[i]));
        }

        inFile.close();
        return values;
    }

    void writeValues(const string& index, const vector<int>& values) {
        string filename = getFilename(index);

        ofstream outFile(filename, ios::binary);
        if (!outFile) {
            return;
        }

        int size = values.size();
        outFile.write(reinterpret_cast<const char*>(&size), sizeof(size));

        for (int value : values) {
            outFile.write(reinterpret_cast<const char*>(&value), sizeof(value));
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
        vector<int> values = readValues(index);

        // Check if value already exists
        auto it = lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            return; // Value already exists
        }

        // Insert value in sorted order
        values.insert(it, value);
        writeValues(index, values);
    }

    void remove(const string& index, int value) {
        vector<int> values = readValues(index);

        // Find and remove the value
        auto it = lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            values.erase(it);
            writeValues(index, values);
        }
    }

    string find(const string& index) {
        vector<int> values = readValues(index);

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