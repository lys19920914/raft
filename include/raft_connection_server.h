//
// Created by liangyusen on 2018/8/6.
//

#ifndef RAFT_RAFT_CONNECTION_SERVER_H
#define RAFT_RAFT_CONNECTION_SERVER_H

#include "response_result.h"
#include "callback_function.h"
#include <functional>

namespace lraft {
    struct RaftConnectionServer : public Connection<RaftConnectionServer> {

        CallbackFunction &_cb;
        bool Disconnected = false;
        int _hash_value;
        std::function<void(int)> RemoveConnection;

        template<typename... TArgs>
        RaftConnectionServer(CallbackFunction &cb, TArgs &&... Args):
                Connection(std::forward<TArgs>(Args)...),
                _cb(cb) {
            _hash_value = std::hash<RaftConnectionServer *>{}(this);
        }

        void OnError(const system::error_code &ec) override {
            std::cout << "RaftConnectionServer OnError:" << ec.message() << std::endl;
            Disconnected = true;
            RemoveConnection(_hash_value); // 从node_rpc_server中移除自己
        }

//    void OnConnect() override {
//        std::cout << Name << " OnConnect...\n";
//    }

        bool IsDisconnected() const {
            return Disconnected;
        }

        void EchoMsg(uint16_t SId, const std::string &Msg) {
            RPCResponse(SId, "echo: " + Msg);
        }

        int GetConnHash() {
            return _hash_value;
        }

        ~RaftConnectionServer() {
            std::cout << "Destroy RaftConnectionServer\n";
        }

        // 协议相关RPC
        void RequestVoteRPC(uint16_t SId, unsigned long term, int candidateId, unsigned long lastLogIndex,
                            unsigned long lastLogTerm) {

            ResponseResult responseResult = _cb.RequestVoteFun(term, candidateId, lastLogIndex, lastLogTerm);
            RPCResponse(SId, responseResult.term, responseResult.success, responseResult.node_id,
                        responseResult._index);
        }

        void AppendEntriesRPC(uint16_t SId, unsigned long term, int leaderId, unsigned long prevLogIndex,
                              unsigned long prevLogTerm, Entry entry,
                              unsigned long leaderCommit) {
            ResponseResult responseResult = _cb.AppendEntriesFun(term, leaderId, prevLogIndex, prevLogTerm, entry,
                                                                 leaderCommit);
            RPCResponse(SId, responseResult.term, responseResult.success, responseResult.node_id,
                        responseResult._index);
        }

        void InstallSnapshotRPC(uint16_t SId, unsigned long term, int leaderId, unsigned long lastIndex,
                                unsigned long lastTerm, std::vector<NodeConfig> lastConfig, const std::string &data) {

            ResponseResult responseResult = _cb.InstallSnapshotFun(term, leaderId, lastIndex, lastTerm, lastConfig,
                                                                   data);
            RPCResponse(SId, responseResult.term, responseResult.success, responseResult.node_id,
                        responseResult._index);
        }

        void AddServerRPC(uint16_t SId, int node_id, const std::string &host, int port) {
            _cb.AddServerFun(_hash_value, SId, NodeConfig(node_id, host, port));
        }

        void RemoveServerRPC(uint16_t SId, int node_id) {
            _cb.RemoveServerFun(_hash_value, SId, node_id);
        }

        void ClientRequestRPC(uint16_t SId, const std::string &command) {
            _cb.ClientRequestFun(_hash_value, SId, command);
        }

        void ClientQueryRPC(uint16_t SId, const std::string &query) {
            _cb.ClientQueryFun(_hash_value, SId, query);
        }
    };
}


#endif //RAFT_RAFT_CONNECTION_SERVER_H
