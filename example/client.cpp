//
// Created by liangyusen on 2018/8/24.
//
#include <iostream>
#include <boost/lexical_cast.hpp>
#include "msgpack.hpp"

#include "raft_client.h"

enum class DEFINED_RPC_ID {
    SET = 10, GET
};

using namespace lraft;

void split(const std::string &line, const std::string &sep, std::vector<std::string> &res) {
    size_t start = 0;
    size_t index = line.find_first_of(sep, start);
    while (index != std::string::npos) {
        if (index - start > 0)
            res.push_back(line.substr(start, index - start));
        start = index + 1;
        index = line.find_first_of(sep, start);
    }
    if (index - start > 0)
        res.push_back(line.substr(start, index - start));
}

void OnResponse(bool status, const std::string &response) {
    std::cout << "Status: " << status << std::endl;
    std::cout << "Response: " << response << std::endl;
}

int main() {

    std::set<Address> address_set;
    address_set.insert(Address("127.0.0.1", 9001));
    address_set.insert(Address("127.0.0.1", 9002));
    address_set.insert(Address("127.0.0.1", 9003));

    auto client_ptr = std::make_shared<RaftClient>(address_set);

    std::thread ServiceThread([&]() {
        client_ptr->Run();
        std::cout << "client thread done\n";
    });

    while (true) {
        std::string buf;
        const auto &Input = std::getline(std::cin, buf);
        if (Input.eof()) {
            break;
        }

        std::vector<std::string> SplitResult;
        split(buf, " ", SplitResult);
        if (SplitResult[0] == "set") {
            client_ptr->ClientRequest(static_cast<uint16_t>(DEFINED_RPC_ID::SET), &OnResponse, SplitResult[1],
                                      SplitResult[2]);
        } else if (SplitResult[0] == "get") {
            client_ptr->ClientQuery(static_cast<uint16_t>(DEFINED_RPC_ID::GET), &OnResponse, SplitResult[1]);
        } else if (SplitResult[0] == "add") {
            int node_id = atoi(SplitResult[1].c_str());
            std::string host = SplitResult[2];
            int port = atoi(SplitResult[3].c_str());
            client_ptr->AddServer(node_id, host, port, &OnResponse);
        } else if (SplitResult[0] == "remove") {
            int node_id = atoi(SplitResult[1].c_str());
            client_ptr->RemoveServer(node_id, &OnResponse);
        } else {
            client_ptr->EchoMsg(buf);
        }
    }
    ServiceThread.join();
    return 0;
}

