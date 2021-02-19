//
// Created by liangyusen on 2018/8/7.
//

#ifndef RAFT_RESPONSE_RESULT_H
#define RAFT_RESPONSE_RESULT_H

namespace lraft {
    class ResponseResult {
    public:
        ResponseResult(unsigned long _term, bool _success, int _node_id, unsigned long index) :
                term(_term),
                success(_success),
                node_id(_node_id),
                _index(index) {}

        unsigned long term;
        bool success;
        int node_id;
        unsigned long _index;
    };

}


#endif //RAFT_RESPONSE_RESULT_H
