//
// Created by liangyusen on 2018/8/10.
//

#ifndef RAFT_RAFT_PARAMS_H
#define RAFT_RAFT_PARAMS_H


namespace lraft {
    class RaftParams {
    public:
        RaftParams() :
                election_timeout_upper_bound(6000),
                election_timeout_lower_bound(4000),
                heartbeat_timeout(1000),
                add_node_timeout(1000),
                _max_log_length(5) {
        }

        int election_timeout_upper_bound; // 论文说是10-500ms
        int election_timeout_lower_bound;
        int heartbeat_timeout; // 论文说是0.5-20ms
        int add_node_timeout; // 用于添加节点超时

        int _max_log_length;
    };
}

#endif //RAFT_RAFT_PARAMS_H
