//
// Created by liangyusen on 2018/8/24.
//

#ifndef RAFT_RPCID_H
#define RAFT_RPCID_H

enum class RPCID {
    ECHOMSG = 10,
    REQUESTVOTE,
    APPENDENTRIES,
    INSTALLSNAPSHOT,
    ADDSERVER,
    REMOVESERVER,
    CLIENTREQUEST,
    CLIENTQUERY
};

#endif //RAFT_RPCID_H
