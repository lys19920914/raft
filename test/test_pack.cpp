//
// Created by liangyusen on 2018/8/17.
//

#include <msgpack.hpp>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <stdio.h>

class MyClass {
public:
    MyClass(int id, std::string name) :
            _id(id),
            _name(name) {

    }

    MyClass() {

    }

    int _id;
    std::string _name;
    MSGPACK_DEFINE(_id, _name
    );
};

template<typename T>
class StableEntryLogs {
public:
    StableEntryLogs(std::string filename) :
            _filename(filename) {
        _log_file.open(filename, std::fstream::out);
        _log_file.close();
        _log_file.open(filename, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);

        _locations.push_back(0);
        _start_index = 1;
    }

    ~StableEntryLogs() {
        _log_file.close();
    }


    T GetEntryByIndex(unsigned long target_index) {
        long index = target_index - _start_index;
        _log_file.seekg(_locations[index]);
        std::string line;
        getline(_log_file, line);
        msgpack::object_handle oh = msgpack::unpack(line.data(), line.size());
        msgpack::object deserialized = oh.get();
        MyClass c;
        deserialized.convert(c);
        return c;
    }

    void AppendEntry(const T &t) {
        std::stringstream buffer;
        msgpack::pack(buffer, t);
        buffer.seekg(0);
        std::string str(buffer.str() + "\n");

        int pos = _locations[_locations.size() - 1];
        _log_file.seekp(pos);
        _log_file.write(str.c_str(), str.size());
        _log_file.flush();

        _locations.push_back(pos + str.size());
    }

    void DeleteEntryAfterIndex(unsigned long target_index) {
        long index = target_index - _start_index;
        _locations.erase(_locations.begin() + index + 1, _locations.end());
    }

    void DeleteEntryBeforeIndex(unsigned long target_index) {
        std::rename(_filename.c_str(), (_filename + "backup").c_str());

        // 新开一个文件，把旧文件要保留的entry读出来，写过去
        std::fstream new_log_file;
        std::vector<int> new_locations;
        new_log_file.open(_filename, std::fstream::out);
        new_locations.push_back(0);

        long index = target_index - _start_index;
        for (int i = index; i < _locations.size() - 1; i++) {
            int pos = _locations[i];
            _log_file.seekg(pos);
            std::string line;
            getline(_log_file, line);
            line = line + "\n";

            int new_pos = new_locations[new_locations.size() - 1];
            new_log_file.seekp(new_pos);
            new_log_file.write(line.c_str(), line.size());

            new_locations.push_back(new_pos + line.size());
        }

        // 重新加载_locations，更新_start_index
        _locations.clear();
        _locations.insert(_locations.begin(), new_locations.begin(), new_locations.end());
        _start_index = target_index;

        // _log_file,重新读新文件，删除backup文件
        new_log_file.close();
        _log_file.close();
        _log_file.open(_filename, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);
        std::remove((_filename + "backup").c_str());
    }

public:
    std::fstream _log_file;
    std::string _filename;
    std::vector<int> _locations;
    unsigned long _start_index;
};

int main() {

    MyClass c1(1, "a"), c2(2, "bb"), c3(3, "ccc"), c4(4, "dddd"), c5(5, "eeeee");
    std::vector<MyClass> cv;
    cv.push_back(c1);
    cv.push_back(c2);
    cv.push_back(c3);
    cv.push_back(c4);
    cv.push_back(c5);

    lraft::StableEntryLogs <MyClass> stableEntryLogs("log");

    for (auto &c:cv) {
        stableEntryLogs.AppendEntry(c);
    }

    for (int i = 1; i <= 5; i++) {
        MyClass c = stableEntryLogs.GetEntryByIndex(i);
        std::cout << c._id << " " << c._name << std::endl;
    }


    stableEntryLogs.DeleteEntryAfterIndex(3);

    MyClass c32(33, "c");
    MyClass c42(44, "d");
    stableEntryLogs.AppendEntry(c32);
    stableEntryLogs.AppendEntry(c42);

    std::cout << "------------------" << std::endl;
    for (int i = 1; i <= 4; i++) {
        MyClass c = stableEntryLogs.GetEntryByIndex(i);
        std::cout << c._id << " " << c._name << std::endl;
    }

    stableEntryLogs.DeleteEntryBeforeIndex(3);
    MyClass c53(555, "eee");
    stableEntryLogs.AppendEntry(c53);
    std::cout << "------------------" << std::endl;
    for (int i = 3; i <= 5; i++) {
        MyClass c = stableEntryLogs.GetEntryByIndex(i);
        std::cout << c._id << " " << c._name << std::endl;
    }

    return 0;
}
