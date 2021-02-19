//
// Created by liangyusen on 2018/7/31.
//

#include "peer.h"
#include "raft_connection_client.h"

using namespace lraft;

bool Peer::BuildConnection() {
    if (connection != nullptr && !connection->IsDisconnected()) {
        return true;
    }
    connection = std::make_shared<RaftConnectionClient>(*_io_ptr, RPCService, NETPROTOCOL::TCP);
    connection->node_id = node_config.node_id;
    const auto &ec = connection->Connect(node_config.host.c_str(), node_config.port);
    if (ec) {
        connection == nullptr; // 注意还是要把connection置为空才能返回
        std::cerr << __LINE__ << " conncet error:" << ec.message() << std::endl;
        return false;
    }
    return true;
}

