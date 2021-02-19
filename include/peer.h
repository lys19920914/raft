//
// Created by liangyusen on 2018/7/31.
//

#ifndef RAFT_PEER_H
#define RAFT_PEER_H

#include <memory>
#include <thread>

#include "node_config.h"
#include "raft_connection_client.h"
#include "rpcnet.hpp"


namespace lraft {
    class Peer {
    public:
        NodeConfig node_config;
    private:
        std::shared_ptr<asio::io_service> _io_ptr;

//      rpc相关
        RPC<RaftConnectionClient> &RPCService;


    public:
        Peer(std::shared_ptr<asio::io_service> io_ptr, NodeConfig nodeConfig, RPC<RaftConnectionClient> &R) :
                node_config(nodeConfig),
                _io_ptr(io_ptr),
                RPCService(R) {
            connection = nullptr;
        }

        Peer(const Peer &peer) :
                node_config(peer.node_config),
                _io_ptr(peer._io_ptr),
                RPCService(peer.RPCService),
                nextIndex(peer.nextIndex),
                matchIndex(peer.matchIndex) {
            connection = nullptr;
        }

        std::shared_ptr<RaftConnectionClient> connection;

        bool BuildConnection();

        void SetNextIndex(unsigned long next_index) {
            nextIndex = next_index;
        }

        void SetMatchIndex(unsigned long match_index) {
            matchIndex = match_index;
        }

    public:
        unsigned long nextIndex; // 下一条要发送给follower的index
        unsigned long matchIndex; // 已经被成功复制的log的index
    };
}

#endif //RAFT_PEER_H
