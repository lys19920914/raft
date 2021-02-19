//
// Created by liangyusen on 2018/7/31.
//

#ifndef RAFT_NODE_CONFIG_H
#define RAFT_NODE_CONFIG_H

#include <msgpack.hpp>

using namespace std;

namespace lraft {
    class NodeConfig {
    public:
        NodeConfig() = default;

        NodeConfig(int node_id, const string &host, int port) {
            this->node_id = node_id;
            this->host = host;
            this->port = port;
        }

        int node_id;
        string host;
        int port;

        MSGPACK_DEFINE(node_id, host, port);
    };
}

#endif //RAFT_NODE_CONFIG_H
