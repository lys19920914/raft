//
// Created by liangyusen on 2018/8/4.
//

#ifndef RAFT_NODE_STATE_H
#define RAFT_NODE_STATE_H

#include <set>

namespace lraft {

    enum class Role {
        follow, candidate, leader
    };

    class NodeState {
    public:
        NodeState() :
                currentTerm(0),
                votedFor(-1),
                leaderId(0),
                commitIndex(0),
                lastApplied(0),
                config_changing(false),
                catch_up(true),
                _election_timeout_src(false) {}

        Role role;

        unsigned long currentTerm;
        int votedFor;
        std::set<int> votedNodeSet;
        int leaderId;


        // log[]
        unsigned long commitIndex; // 需要被commit的最高index
        unsigned long lastApplied; // 已经被commit的最高index

        // cluster membership changes
        bool config_changing;
        bool catch_up;
        bool _election_timeout_src; // 这个标志为了防止4.2.3 Disruptive servers
        // 如果是从append_entry设置的，则为true

    };
}

#endif //RAFT_NODE_STATE_H
