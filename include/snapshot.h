//
// Created by liangyusen on 2018/8/16.
//

#ifndef RAFT_SNAPSHOT_H
#define RAFT_SNAPSHOT_H

#include "node_config.h"
#include <string>
#include <fstream>

namespace lraft {
    class Snapshot {
    public:
        Snapshot() = default;

        Snapshot(unsigned long index, unsigned long term, const std::vector<NodeConfig> &cluster_config,
                 const std::string &data) :
                _index(index),
                _term(term),
                _cluster_config(cluster_config),
                _data(data) {}

        unsigned long _index;
        unsigned long _term;
        std::vector<NodeConfig> _cluster_config;
        std::string _data;

        MSGPACK_DEFINE(_index, _term, _cluster_config, _data
        );
    };
}

#endif //RAFT_SNAPSHOT_H
