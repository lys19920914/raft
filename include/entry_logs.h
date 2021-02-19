//
// Created by liangyusen on 2018/8/7.
//

#ifndef RAFT_ENTRY_LOGS_H
#define RAFT_ENTRY_LOGS_H

#include <vector>
#include <fstream>
#include <sstream>
#include "entry.h"

namespace lraft {
    class EntryLogs {
    public:
        EntryLogs(const std::string &folder_name);

        Entry GetEntryByIndex(unsigned long target_index);

        void AppendEntry(const Entry &entry);

        void DeleteEntryAfterIndex(unsigned long start);

        unsigned long TermAt(unsigned long target_index);

        unsigned long GetLastLogIndex();

        void DeleteEntryBeforeIndex(unsigned long end_index);

        unsigned long GetStartIndex() {
            return _start_index;
        }

        void SetStartIndex(unsigned long index) {
            _start_index = index;
        }

        int GetLogsSize() {
            return Logs.size();
        }

    private:
        std::vector<Entry> Logs;
        unsigned long _start_index; // log中第一条entry的index，没有entry以0表示
        std::string _folder_name;
        std::fstream _log_file;
        std::vector<unsigned long> _locations;

    private:
        void Replace(std::string &s, const std::string &target, const std::string &replace);
    };
}

#endif //RAFT_ENTRY_LOGS_H
