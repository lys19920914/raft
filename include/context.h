//
// Created by liangyusen on 2018/7/31.
//

#ifndef RAFT_CONTEXT_H
#define RAFT_CONTEXT_H

#include <vector>

#include "state_machine.h"
#include "node_config.h"
#include "raft_params.h"
#include "logger_type.h"

namespace lraft {
    class Context {
    public:
        Context(int n_id, std::vector<NodeConfig> &n_config_vector, std::shared_ptr<StateMachine> sm_ptr,
                std::shared_ptr<RaftParams> rp_ptr, bool an, const std::string &folder_name, bool log_file,
                LogType severity) :
                node_id(n_id),
                node_config_vector(n_config_vector),
                state_machine_ptr(sm_ptr),
                raft_params_ptr(rp_ptr),
                add_node(an),
                _folder_name(std::move(folder_name)),
                _log_file(log_file),
                _severity(severity) {}

        int node_id;
        std::vector<NodeConfig> &node_config_vector;
        std::shared_ptr<StateMachine> state_machine_ptr;
        std::shared_ptr<RaftParams> raft_params_ptr;
        bool add_node;
        std::string _folder_name;
        bool _log_file;
        LogType _severity;
    };
}

#endif //RAFT_CONTEXT_H
