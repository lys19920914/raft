//
// Created by liangyusen on 2018/8/6.
//

#ifndef RAFT_RAFT_CONNECTION_CLIENT_H
#define RAFT_RAFT_CONNECTION_CLIENT_H

#include "tcp/tcpserver.hpp"
#include "udp/udpserver.hpp"
#include "rpcnet.hpp"
#include "RPCID.h"
#include <atomic>

namespace lraft {

    enum class ConnnectionStatus {
        CONNECTED, DISCONNECTED, CONNECTING
    };

    struct RaftConnectionClient : public Connection<RaftConnectionClient> {

        int node_id;

        bool Disconnected = false;

        ConnnectionStatus _status;

        template<typename... TArgs>
        RaftConnectionClient(TArgs &&... Args):
                Connection(std::forward<TArgs>(Args)...) {
            _status = ConnnectionStatus::CONNECTING;
        }

        void OnError(const system::error_code &ec) override {
            std::cout << "Connection to node " << node_id << " OnError:" << ec.message() << std::endl;
            // ec.message()，Connection refused是server没开
            // End of file是连上之后，server关了
            // 应该是tcpconnection.cpp里边报错，会一直throw到这里来
            Disconnected = true;
            _status = ConnnectionStatus::DISCONNECTED;
            // logout(dynamic_pointer_cast<RaftConnection>(shared_from_this()));
        }

        void OnConnect() override {
            // std::cout << Name << " OnConnect...\n";
            _status = ConnnectionStatus::CONNECTED;
        }

        bool IsDisconnected() const {
            return Disconnected;
        }

        ConnnectionStatus GetConnectionStatus() {
            return _status;
        }

        ~RaftConnectionClient() {
            std::cout << "Destroy RaftConnectionClient\n";
        }
    };
}

#endif //RAFT_RAFT_CONNECTION_CLIENT_H
