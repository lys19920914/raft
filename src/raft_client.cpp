//
// Created by liangyusen on 2018/8/24.
//

#include "raft_client.h"

using namespace lraft;

void RaftClient::Run() {
    _io_ptr->run();
}

bool RaftClient::BuildConnection() {
    if (ConnPtr && ConnPtr->GetConnectionStatus() == ConnnectionStatus::CONNECTED) {
        return true;
    }

    if (_new_leaeder) { // 如果是根据回复的leader，不需要检查，直接建立连接后返回
        ConnPtr = std::make_shared<RaftConnectionClient>(*_io_ptr, ClientRPCService, NETPROTOCOL::TCP);
        const auto &ec = ConnPtr->Connect(_leader_host.c_str(), _leader_port);
        _new_leaeder = false;
        if (ec) {// 这个错不是连接不上的错，是已经创建了一个连接了，再创建第二个，就错
            // 连接不上的错，会在connection的OnError里边报
            std::cerr << __LINE__ << " conncet error:" << ec.message() << std::endl;
            return false;
        } else {

            while (ConnPtr->GetConnectionStatus() == ConnnectionStatus::CONNECTING) {
                sleep(1);
            }
            if (ConnPtr->GetConnectionStatus() == ConnnectionStatus::CONNECTED) {
                return true;
            } else
                return false;
        }
    }

    // 如果是根据自己的address set去建立连接，则直到建立连接，或者遍历完地址之后，还是连不上，才返回
    ConnPtr = nullptr;
    unsigned int address_index = 0;
    while (true) {
        std::string host;
        int port;
        if (address_index == _address_set.size())
            return false;

        auto it = _address_set.begin();
        advance(it, address_index);
        host = it->_host;
        port = it->_port;
        address_index++;

        ConnPtr = std::make_shared<RaftConnectionClient>(*_io_ptr, ClientRPCService, NETPROTOCOL::TCP);
        const auto &ec = ConnPtr->Connect(host.c_str(), port);
        if (ec) {
            std::cerr << __LINE__ << " test conncet error:" << ec.message() << std::endl;
            continue;
        }

        while (ConnPtr->GetConnectionStatus() == ConnnectionStatus::CONNECTING) {
            sleep(1);
        }

        if (ConnPtr->GetConnectionStatus() == ConnnectionStatus::CONNECTED) {
            return true;
        }
    }
}

void RaftClient::EchoMsg(const std::string &msg) {
    auto CalRes = [](lraft::RaftConnectionClient *ConnPtr, std::string Msg) {
        LOG("got the Result:", Msg);
    };

    if (BuildConnection()) {
        ConnPtr->CallRPC(CalRes, RPCID::ECHOMSG, msg);
    }
}

void RaftClient::UpdateLeader(std::string host, int port) {
    if (_leader_host == host && _leader_port == port)
        return;
    _leader_host = host;
    _leader_port = port;
    Address address(host, port);
    _address_set.insert(address);

    ConnPtr = nullptr;
    _new_leaeder = true;
    BuildConnection();
}

