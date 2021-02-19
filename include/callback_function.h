//
// Created by liangyusen on 2018/9/3.
//

#ifndef RAFT_CALLBACK_FUNCTION_H
#define RAFT_CALLBACK_FUNCTION_H

#include <functional>
#include "entry.h"
#include "node_config.h"
#include "response_result.h"

namespace lraft {
    class CallbackFunction {
    public:
        std::function<ResponseResult(unsigned long term, int candidateId, unsigned long lastLogIndex,
                                     unsigned long lastLogTerm)> RequestVoteFun;

        std::function<ResponseResult(unsigned long term, int leaderId, unsigned long prevLogIndex,
                                     unsigned long prevLogTerm, Entry entry,
                                     unsigned long leaderCommit)> AppendEntriesFun;

        std::function<ResponseResult(unsigned long term, int leaderId, unsigned long lastIndex,
                                     unsigned long lastTerm, std::vector<NodeConfig> &lastConfig,
                                     const std::string &data)> InstallSnapshotFun;

        std::function<void(int conn_hash, unsigned short SId, NodeConfig nodeConfig)> AddServerFun;

        std::function<void(int conn_hash, unsigned short SId, int node_id)> RemoveServerFun;

        std::function<void(int conn_hash, unsigned short SId, const std::string &command)> ClientRequestFun;

        std::function<void(int conn_hash, unsigned short SId, const std::string &query)> ClientQueryFun;
    };
}


#endif //RAFT_CALLBACK_FUNCTION_H
