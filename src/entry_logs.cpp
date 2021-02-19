//
// Created by liangyusen on 2018/8/22.
//
#include <msgpack.hpp>
#include "entry_logs.h"
#include <iostream>

using namespace lraft;

#define LOG_FILE "entry_log"
#define LINE_BREAK "\n"
#define REPLACE_SYMBOL "*****"


EntryLogs::EntryLogs(const std::string &folder_name) :
        _folder_name(folder_name) {

    _log_file.open(folder_name + "/" + LOG_FILE,
                   ios::in | ios::out | ios::binary | ios::ate);

    if (!_log_file) {
        _log_file.open(folder_name + "/" + LOG_FILE, ios::out);
        _log_file.close();
        _log_file.open(folder_name + "/" + LOG_FILE,
                       ios::in | ios::out | ios::binary | ios::ate);
    }

    if (_log_file.tellp() > 0) {// 如果已经存在log文件，则加载原数据
        _log_file.seekg(0, ios::beg);
        _locations.push_back(0);

        std::string line;
        while (getline(_log_file, line)) {

            Replace(line, REPLACE_SYMBOL, LINE_BREAK);
            unsigned long pos = _log_file.tellg();
            _locations.push_back(pos);

            msgpack::object_handle oh = msgpack::unpack(line.data(), line.size());
            msgpack::object deserialized = oh.get();
            Entry entry;
            deserialized.convert(entry);
            if (Logs.size() == 0) {
                _start_index = entry._index;
            }
            Logs.push_back(entry);
        }
    } else {
        _locations.push_back(0);
        _start_index = 1;
    }
    _log_file.clear(); // 注意！！这个要设置，否则接下来的文件操作都不起作用了
    // After the EOF flag has ben set, you will not be able to extract anything.
    // You have to clear those flags to be able to extract again.
}

Entry EntryLogs::GetEntryByIndex(unsigned long target_index) {

    if (target_index < _start_index || target_index > (_start_index + Logs.size() - 1)) {
        return Entry::GetNullEntry();
    }

    long index;
    index = target_index - _start_index;

    return Logs[index];
}

void EntryLogs::AppendEntry(const Entry &entry) {
    // 内存中的Logs
    Logs.push_back(entry);

    // 磁盘中的Logs
    std::stringstream buffer;
    msgpack::pack(buffer, entry);
    buffer.seekg(0);
    std::string str(buffer.str());

    Replace(str, LINE_BREAK, REPLACE_SYMBOL);
    str = str + "\n";

    unsigned long pos = _locations[_locations.size() - 1];
    _log_file.seekp(pos, std::fstream::beg);
    _log_file.write(str.c_str(), str.size());
    _log_file.flush();

    _locations.push_back(pos + str.size());
}

void EntryLogs::DeleteEntryAfterIndex(unsigned long target_start) { // 包括这个index上的也要删除
    if (target_start < _start_index) {
        Logs.clear();
        _locations.clear();
        return;
    }

    if (target_start > _start_index + Logs.size() - 1) {
        return;
    }

    long index;
    index = target_start - _start_index;
    Logs.erase(Logs.begin() + index, Logs.end());

    // 磁盘中的log也要去删掉，不然重启的时候会影响到
    _locations.erase(_locations.begin() + index + 1, _locations.end());
}

unsigned long EntryLogs::TermAt(unsigned long target_index) {
    return GetEntryByIndex(target_index)._term;
}

unsigned long EntryLogs::GetLastLogIndex() {
    return _start_index + Logs.size() - 1;
}

void EntryLogs::DeleteEntryBeforeIndex(unsigned long target_index) {
    // 内存中的Logs
    if (target_index < _start_index) {
        return;
    }
    if (target_index > _start_index + Logs.size() - 1) {
        Logs.clear();
        return;
    }
    unsigned long index;
    index = target_index - _start_index;
    // 这样子，end_index这条不要删，永远留一条，不然很多地方要额外处理
    Logs.erase(Logs.begin(), Logs.begin() + index);
    _start_index = target_index;

    // 磁盘中的Logs
    std::string logs_filename = _folder_name + "/" + LOG_FILE;
    std::rename(logs_filename.c_str(), (logs_filename + "backup").c_str());

    // 新开一个文件，把旧文件要保留的entry读出来，写过去
    std::fstream new_log_file;
    std::vector<int> new_locations;
    new_log_file.open(logs_filename, std::fstream::out);
    new_locations.push_back(0);

    for (unsigned long i = index; i < _locations.size() - 1; i++) {
        unsigned long pos = _locations[i];
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
    _log_file.open(logs_filename, std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::ate);
    std::remove((logs_filename + "backup").c_str());
}

void EntryLogs::Replace(std::string &s, const std::string &target, const std::string &replace) {
    while (true) {
        std::string::size_type position;
        position = s.find(target);
        if (position != s.npos) {
            s = s.replace(position, target.size(), replace);
        } else {
            break;
        }
    }
}
