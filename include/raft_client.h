//
// Created by liangyusen on 2018/8/24.
//

#ifndef RAFT_RAFT_CLIENT_H
#define RAFT_RAFT_CLIENT_H

#include <unordered_set>
#include "rpcnet.hpp"
#include "raft_connection_client.h"
#include <set>
#include <cstring>
#include <mutex>
#include <condition_variable>

namespace lraft {
    class Address {
    public:
        Address(const std::string &host, int port) :
                _host(host),
                _port(port) {}

        std::string _host;
        int _port;

        bool operator<(const Address &address) const {
            if (_port != address._port)
                return _port < address._port;
            else
                return strcmp(_host.c_str(), address._host.c_str());
        }
    };

    class RaftClient {
    private:
        bool BuildConnection();

        void UpdateLeader(std::string host, int port);

    public:
        RaftClient(std::set<Address> &as) :
                _io_ptr(std::make_shared<asio::io_service>()),
                _work_ptr(std::make_shared<asio::io_service::work>(*_io_ptr)),
                ConnPtr(nullptr),
                _leader_host(""),
                _leader_port(0),
                _new_leaeder(false),
                _address_set(as) {
        }

        void Run();

        void EchoMsg(const std::string &msg);

        template<typename CALLBACK>
        void AddServer(int node_id, const std::string &host, int port, CALLBACK OnResponse) {
            auto CalRes = [=](lraft::RaftConnectionClient *ConnPtr, bool status, std::string response,
                              std::string leader_host, int leader_port) {
                if (status) {
                    OnResponse(true, response);
                } else if (leader_host == "" || leader_port == 0) {
                    OnResponse(false, "Leader is null.");
                } else {
                    UpdateLeader(leader_host, leader_port);
                    this->AddServer(node_id, host, port, OnResponse);
                }
            };

            if (BuildConnection()) {
                ConnPtr->CallRPC(CalRes, RPCID::ADDSERVER, node_id, host, port);
            } else {
                OnResponse(false, "BuildConnnection fail.");
            }
        }

        template<typename CALLBACK>
        void RemoveServer(int node_id, CALLBACK OnResponse) {
            auto CalRes = [=](lraft::RaftConnectionClient *ConnPtr, bool status, std::string response,
                              std::string leader_host, int leader_port) {
                if (status) {
                    OnResponse(true, response);
                } else if (leader_host == "" || leader_port == 0) {
                    OnResponse(false, "Leader is null.");
                } else {
                    UpdateLeader(leader_host, leader_port);
                    this->RemoveServer(node_id, OnResponse);
                }
            };

            if (BuildConnection()) {
                ConnPtr->CallRPC(CalRes, RPCID::REMOVESERVER, node_id);
            } else {
                OnResponse(false, "BuildConnnection fail.");
            }
        }

        template<typename CALLBACK, typename... TArgs>
        void ClientRequest(uint16_t FId, CALLBACK OnResponse, TArgs &&... Args) {
            auto CalRes = [=](lraft::RaftConnectionClient *ConnPtr, bool status, std::string response,
                              std::string leader_host,
                              int leader_port) {
                if (status) {
                    OnResponse(true, response);
                } else if (leader_host == "" || leader_port == 0) {
                    OnResponse(false, "Leader is null.");
                } else {
                    UpdateLeader(leader_host, leader_port);
                    OnResponse(false, "Change leader.");
                    //this->ClientRequest(FId, OnResponse, std::forward<TArgs>(Args)...);
                }
            };

            auto Buf = ClientRPCService.PackCmd(FId, std::forward<TArgs>(Args)...);
            std::string msg(Buf.data(), Buf.size());

            if (BuildConnection()) {
                ConnPtr->CallRPC(CalRes, RPCID::CLIENTREQUEST, msg);
            } else {
                OnResponse(false, "BuildConnnection fail.");
            }
        }

        template<typename CALLBACK, typename... TArgs>
        void ClientQuery(uint16_t FId, CALLBACK OnResponse, TArgs &&... Args) {
            auto CalRes = [=](lraft::RaftConnectionClient *ConnPtr, bool status, std::string response,
                              std::string leader_host,
                              int leader_port) {
                if (status) {
                    OnResponse(status, response);
                } else if (leader_host == "" || leader_port == 0) {
                    OnResponse(false, "Leader is null.");
                } else {
                    UpdateLeader(leader_host, leader_port);
                    OnResponse(false, "Change leader.");
                    //this->ClientQuery(FId, OnResponse, std::forward<TArgs>(Args)...);
                }
            };

            auto Buf = ClientRPCService.PackCmd(FId, std::forward<TArgs>(Args)...);
            std::string msg(Buf.data(), Buf.size());

            if (BuildConnection()) {
                ConnPtr->CallRPC(CalRes, RPCID::CLIENTQUERY, msg);
            } else {
                OnResponse(false, "BuildConnection fail.");
            }
        }

    private:
        std::shared_ptr<asio::io_service> _io_ptr;
        std::shared_ptr<asio::io_service::work> _work_ptr;
        RPC<lraft::RaftConnectionClient> ClientRPCService;
        std::shared_ptr<RaftConnectionClient> ConnPtr;
        std::string _leader_host;
        int _leader_port;
        bool _new_leaeder; // 新leader时，置为true，连接一次，就置为false，防止新leader不能用，但是一直在连的情况。
        std::set<Address> &_address_set;

    };

}

#endif //RAFT_RAFT_CLIENT_H
