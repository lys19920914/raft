//
// Created by liangyusen on 2018/8/10.
//

#ifndef RAFT_STATE_MACHINE_H
#define RAFT_STATE_MACHINE_H

#include <map>
#include "raft_connection_server.h"


namespace lraft {
    class StateMachine {
    public:
        void Commit(RaftConnectionServer *ConnPtr, RPC<RaftConnectionServer> &RPCService, unsigned short SId,
                    const std::string &command) {
            RPCService.UnpackCmd(ConnPtr, SId, command.c_str(), command.size());
        }

        virtual std::string Serialize() = 0;

        virtual void ResetStateMachine(const std::string &data) = 0;

        virtual void OnConnection(RaftConnectionServer *ConnPtr) {}

        virtual void OnError(RaftConnectionServer *ConnPtr) {}
    };
}


#endif //RAFT_STATE_MACHINE_H
