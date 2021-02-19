//
// Created by liangyusen on 2018/8/7.
//

#ifndef RAFT_ENTRY_H
#define RAFT_ENTRY_H

#include <string>
#include <vector>
#include <msgpack.hpp>
#include "node_config.h"

namespace lraft {
    class Entry {
    public:
        Entry() {}

        Entry(const std::string &type, unsigned long index, unsigned long term, const std::string &command, int conn_hash,
              uint16_t SId) :
                _type(type),
                _index(index),
                _term(term),
                _command(command),
                _conn_hash(conn_hash),
                _SId(SId) {}

        Entry(const std::string &type, unsigned long index, unsigned long term, std::vector<NodeConfig> ncv, int conn_hash,
              uint16_t SId) :
                _type(type),
                _index(index),
                _term(term),
                _node_config_vector(ncv),
                _conn_hash(conn_hash),
                _SId(SId) {}

        static Entry GetNullEntry() {
            return Entry("null", 0, 0, "", 0, 0);
        }

        std::string _type; // normal表示常规，config表示集群配置变更
        unsigned long _index;
        unsigned long _term;
        // 常规日志
        std::string _command;
        // 集群配置变更日志
        std::vector<NodeConfig> _node_config_vector;

        int _conn_hash; // 标记该entry的来源connection
        unsigned short _SId;

        MSGPACK_DEFINE(_type, _index, _term, _command, _node_config_vector, _conn_hash, _SId
        );
    };
}

#endif //RAFT_ENTRY_H
