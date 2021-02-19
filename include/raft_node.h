//
// Created by liangyusen on 2018/7/31.
//

#ifndef RAFT_RAFT_NODE_H
#define RAFT_RAFT_NODE_H

#include <iostream>
#include <map>
#include <functional>
#include <thread>
#include <boost/lexical_cast.hpp>
#include <functional>

#include "peer.h"
#include "context.h"
#include "tcp/tcpserver.hpp"
#include "timer.h"
#include "node_state.h"
#include "raft_connection_client.h"
#include "response_result.h"
#include "entry_logs.h"
#include "state_machine.h"
#include "raft_params.h"
#include "node_config.h"
#include "snapshot.h"
#include "node_rpc_server.h"
#include "callback_function.h"


namespace lraft {

    class RaftNode {
    public:
        RaftNode(const Context &context);

    private:
        std::map<int, Peer> peer_map;
        NodeConfig node_config;
        NodeState node_state;
        std::shared_ptr<asio::io_service> _io_ptr;
        Timer election_;
        Timer heartbeat_;
        Timer add_node_;
        std::shared_ptr<EntryLogs> _entry_logs_ptr;

        std::shared_ptr<StateMachine> sm_ptr;
        std::shared_ptr<RaftParams> raft_params_ptr;
        std::shared_ptr<Peer> node_to_join; // 如果有机器要加入，指向要加入的机器
        uint16_t add_node_SId; // 用来记录加入机器的RPC的SId
        int add_node_conn_hash;
        std::vector<NodeConfig> &_cluster_config; // 集群信息，和peer_map的区别是包括自己
        std::shared_ptr<Snapshot> _snapshot_ptr;
        std::string _folder;
        RPC<RaftConnectionClient> RPCService;
        std::shared_ptr<NodeRPCServer> _node_rpc_ptr;
        CallbackFunction _callback_function;


    public:
        RPC<RaftConnectionServer> _SM_RPCService;

        void RegisterCallback();

        void Run();

        // 协议相关
        void HandleElectionTimeout();

        ResponseResult HandleRequestVoteRPC(unsigned long term, int candidateId, unsigned long lastLogIndex,
                                            unsigned long lastLogTerm);

        void HandleRequestVoteRPCResponse(unsigned long term, bool voteGranted, int src_node_id);

        void BecomeLeader();

        void AppendEntriesRPC();

        void AppendEntriesRPC(Peer &peer);

        ResponseResult HandleAppendEntriesRPC(unsigned long term, int leaderId, unsigned long prevLogIndex,
                                              unsigned long prevLogTerm, Entry entry,
                                              unsigned long leaderCommit);

        void BecomeFollower();

        void HandleAppendEntriesRPCResponse(unsigned long term, bool success, int src_node_id, unsigned long index);

        void Commit();

        void TermCompare(unsigned long term);

        int GetRandomElectionTimeout();

        void HandleAddServerRPC(int conn_hash, unsigned short SId, NodeConfig nodeConfig);

        void HandleAddServerTimeout();

        void ClusterMembershipChanges(int conn_hash, unsigned short SId, std::vector<NodeConfig> &node_config_vector);

        void HandleRemoveServerRPC(int conn_hash, unsigned short SId, int node_id);

        void TakeSnapshot();

        void SendInstallSnapshotRPC(Peer &peer);

        ResponseResult HandleInstallSnapshotRPC(unsigned long term, int leaderId, unsigned long lastIndex,
                                                unsigned long lastTerm, std::vector<NodeConfig> &lastConfig,
                                                const std::string &data);

        void SaveSnapshot();

        void HandleClientRequest(int conn_hash, unsigned short SId, const std::string &command);

        void HandleClientQuery(int conn_hash, unsigned short SId, const std::string &query);

        template<typename FTYPE>
        void RegisterRPC(uint16_t CmdId, FTYPE &&Function) {
            _SM_RPCService.RegisterCmd(CmdId, Function);
        }

        void RPCResponse(RaftConnectionServer *ConnPtr, unsigned short SId, bool status, const std::string &response);

    private:
        void RPCResponse(int conn_hash, unsigned short SId, bool status, const std::string &response);
    };
}

#endif //RAFT_RAFT_NODE_H
