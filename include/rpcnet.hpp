#pragma once
#include <memory>
#include <utility>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <functional>
#include <type_traits>
#include <unistd.h>
#include <set>
#include <thread>
#include <queue>
#include <iostream>
#include "rpcmanager.hpp"
using namespace boost;
using namespace boost::asio;

enum class RPCID;
//base of tcp and udp
struct ISocket//:public std::enable_shared_from_this<ISocket>
{
	virtual system::error_code StartService() = 0;
	virtual system::error_code Connect(const char* Ip, unsigned short Port) = 0;
	virtual void SendData(const char* Data, size_t Len) = 0;
	virtual system::error_code Stop() = 0;
};

enum class NETPROTOCOL{TCP, UDP};
class TCPSocket;
class UDPSocket;
struct IConnection//:public std::enable_shared_from_this<IConnection>
{
		virtual void OnMessage(const char* Data, size_t Len) = 0;
		virtual void OnError(const system::error_code& ec) = 0;
		virtual void OnConnect() = 0;
};
template<typename T>class Connection:public IConnection, public std::enable_shared_from_this<Connection<T>>
{
	std::shared_ptr<ISocket> ImplConn;
	RPC<T>& RPCService;
	const NETPROTOCOL NetPto;
	public:
		Connection(asio::io_service& Service, RPC<T>& R, NETPROTOCOL Mode):RPCService(R), NetPto(Mode)
		{
			if( Mode == NETPROTOCOL::UDP ){
				ImplConn = static_pointer_cast<ISocket>(std::make_shared<UDPSocket>(Service, this));
			}else{
				ImplConn = static_pointer_cast<ISocket>(std::make_shared<TCPSocket>(Service, this));
			}
		}
		Connection(UDPSocket&& Conn, RPC<T>& R):RPCService(R), NetPto(NETPROTOCOL::UDP)
		{
			ImplConn = static_pointer_cast<ISocket>(std::make_shared<UDPSocket>(std::move(Conn), this));
		}
		Connection(TCPSocket&& Conn, RPC<T>& R):RPCService(R), NetPto(NETPROTOCOL::TCP)
		{
			ImplConn = static_pointer_cast<ISocket>(std::make_shared<TCPSocket>(std::move(Conn), this));
		}
		Connection(const Connection&) = delete;
		Connection(Connection&& Conn):ImplConn{std::move(Conn.ImplConn)}, RPCService{Conn.RPCService}, NetPto{Conn.NetPto}{}
		system::error_code StartService()
		{
			return ImplConn->StartService();
		}
		system::error_code Connect(const char* Ip, unsigned short Port)
		{
			return ImplConn->Connect(Ip, Port);
		}
		system::error_code Stop()
		{
			return ImplConn->Stop();
		}
		template<typename... TArgs> void CallRPC(RPCID Id, TArgs&&... Args)
		{
			if( NetPto == NETPROTOCOL::UDP ){
				const auto& Buf = RPCService.PackCmd(static_cast<uint16_t>(Id), std::forward<TArgs>(Args)...);
				ImplConn->SendData(Buf.data(), Buf.size());
			}else{
				const auto& Buf = RPCService.PackCmdWithHead(static_cast<uint16_t>(Id), std::forward<TArgs>(Args)...);
				ImplConn->SendData(Buf.data(), Buf.size());
			}
		}
		template<typename TCB, typename... TArgs> void CallRPC(TCB&& CB, RPCID Id, TArgs&&... Args)
		{
			auto SId = RPCService.RegisterCallback(std::forward<TCB>(CB));
			CallRPC(Id, SId, std::forward<TArgs>(Args)...);
		}
		template<typename... TArgs> void RPCResponse(uint16_t SId, TArgs&&... Args)
		{
			if( NetPto == NETPROTOCOL::UDP ){
				const auto& Buf = RPCService.PackResponse(SId, std::forward<TArgs>(Args)...);
				ImplConn->SendData(Buf.data(), Buf.size());
			}else{
				const auto& Buf = RPCService.PackResponseWithHead(SId, std::forward<TArgs>(Args)...);
				ImplConn->SendData(Buf.data(), Buf.size());
			}
		}
		virtual void OnMessage(const char* Data, size_t Len)override
		{
			RPCService.UnpackCmd(static_cast<T*>(this), Data, Len);
		}
		virtual void OnError(const system::error_code& ec)override
		{
		}
		virtual void OnConnect()override
		{
		}
		virtual ~Connection()
		{
			Stop();
			ImplConn = nullptr;
		}
};
