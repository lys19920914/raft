//
// Created by liangyusen on 2018/8/6.
//

#ifndef RAFT_NODE_RPC_SERVER_H
#define RAFT_NODE_RPC_SERVER_H

#include "tcp/tcpserver.hpp"
#include "raft_connection_server.h"
#include "context.h"
#include "callback_function.h"
#include "state_machine.h"

namespace lraft {

    class NodeRPCServer {
    public:

        NodeRPCServer(std::shared_ptr<asio::io_service> io_ptr, CallbackFunction &cb, const Context &context,
                      std::shared_ptr<StateMachine> sm_ptr) :
                _cb(cb),
                _sm_ptr(sm_ptr) {
            //              TServer{&OnNewConnection, io}{

            for (auto it = context.node_config_vector.cbegin(); it != context.node_config_vector.cend(); it++) {
                int node_id = (*it).node_id;
                std::string host = (*it).host;
                int port = (*it).port;

                if (node_id == context.node_id) {
                    // RPC相关
                    ServerPtr = std::make_shared<TCPServer<CBType>>(
                            std::bind(&NodeRPCServer::OnNewConnection, this, std::placeholders::_1), *io_ptr);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::ECHOMSG),
                                           &RaftConnectionServer::EchoMsg);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::REQUESTVOTE),
                                           &RaftConnectionServer::RequestVoteRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::APPENDENTRIES),
                                           &RaftConnectionServer::AppendEntriesRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::INSTALLSNAPSHOT),
                                           &RaftConnectionServer::InstallSnapshotRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::ADDSERVER),
                                           &RaftConnectionServer::AddServerRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::REMOVESERVER),
                                           &RaftConnectionServer::RemoveServerRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::CLIENTREQUEST),
                                           &RaftConnectionServer::ClientRequestRPC);
                    RPCService.RegisterCmd(static_cast<uint16_t>(RPCID::CLIENTQUERY),
                                           &RaftConnectionServer::ClientQueryRPC);
                    const auto &ec1 = ServerPtr->Run(port, host);
                    //const auto &ec1 = TServer.Run(port, host);
                    if (ec1) {
                        std::cerr << "run TCPserver error:" << ec1.message() << std::endl;
                    }
                }
            }
        }


        void OnNewConnection(TCPSocket &&NewCon) {
            auto ConnPtr = std::make_shared<RaftConnectionServer>(_cb, std::move(NewCon), RPCService);
            allconnections.emplace(ConnPtr->GetConnHash(), ConnPtr);
            ConnPtr->RemoveConnection = std::bind(&NodeRPCServer::RemoveConnection, this, std::placeholders::_1);

            const auto &ec = ConnPtr->StartService();
            if (ec) {
                std::cerr << "StartService error:" << ec.message() << std::endl;
            } else {
                std::cout << "NewConnection StartService\n";
            }

            _sm_ptr->OnConnection(ConnPtr.get());
        }

        auto GetConnection(int conn_hash) {
            return allconnections[conn_hash];
        }

        void RemoveConnection(int conn_hash) {
            _sm_ptr->OnError(GetConnection(conn_hash).get());
            allconnections.erase(conn_hash);
        }

    private:
        RPC<RaftConnectionServer> RPCService;
        CallbackFunction &_cb;
        //TCPServer<decltype(&NodeRPCServer::OnNewConnection)> TServer;
        using CBType = decltype(std::bind(&NodeRPCServer::OnNewConnection, std::declval<NodeRPCServer *>(),
                                          std::placeholders::_1));
        std::shared_ptr<TCPServer<CBType>> ServerPtr;
        //std::shared_ptr<TCPServer<decltype(std::bind(&OnNewConnection, std::declval<RaftNode*>(), std::placeholders::_1))>> ServerPtr;

        std::map<std::size_t, std::shared_ptr<RaftConnectionServer>> allconnections;

        std::shared_ptr<StateMachine> _sm_ptr;
    };
}

#endif //RAFT_NODE_RPC_SERVER_H
