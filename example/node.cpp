#include <iostream>
#include <boost/lexical_cast.hpp>
#include <memory>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

#include "lraft.h"
#include "msgpack.hpp"

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

std::map<std::string, std::string> StrMap;
std::shared_ptr<RaftNode> node_ptr;


class MyStateMachine : public StateMachine {
public:
    std::string Serialize() {
        std::stringstream buffer;
        msgpack::pack(buffer, StrMap);
        buffer.seekg(0);
        return std::string(buffer.str());
    }

    void ResetStateMachine(const std::string &snapshotStr) {
        msgpack::object_handle oh = msgpack::unpack(snapshotStr.data(), snapshotStr.size());
        msgpack::object deserialized = oh.get();
        deserialized.convert(StrMap);
        std::cout << " Reset state machine success, map size is " << StrMap.size() << std::endl;
    }

    void OnConnection(RaftConnectionServer *ConnPtr) override {
        std::cout << "User know new connection." << std::endl;
    }

    void OnError(RaftConnectionServer *ConnPtr) override {
        std::cout << "User know connection error." << std::endl;
    }
};

void Set(RaftConnectionServer *ConnPtr, unsigned short SId, const std::string &key, const std::string &value) {
    StrMap[key] = value;
    node_ptr->RPCResponse(ConnPtr, SId, true, "Set OK.");
}

void Get(RaftConnectionServer *ConnPtr, unsigned short SId, const std::string &key) {
    std::string value = StrMap[key];
    node_ptr->RPCResponse(ConnPtr, SId, true, std::move(value));
}


void RunNode(std::vector<NodeConfig> node_config_vector, int node_id, bool add_node) {

    auto raft_params_ptr = std::make_shared<RaftParams>();
    Context context(node_id, node_config_vector, std::make_shared<MyStateMachine>(), raft_params_ptr, add_node,
                    "node" + to_string(node_id), false, LogType::debug);
    node_ptr = std::make_shared<RaftNode>(context);
    node_ptr->RegisterRPC(static_cast<uint16_t>(DEFINED_RPC_ID::SET), Set);
    node_ptr->RegisterRPC(static_cast<uint16_t>(DEFINED_RPC_ID::GET), Get);
    node_ptr->Run();
}

int main(int argc, char **argv) {

    std::vector<NodeConfig> node_config_vector;
    int node_id;

    NodeConfig n1(1, "127.0.0.1", 9001);
    node_config_vector.push_back(n1);
    NodeConfig n2(2, "127.0.0.1", 9002);
    node_config_vector.push_back(n2);
    NodeConfig n3(3, "127.0.0.1", 9003);
    node_config_vector.push_back(n3);

    char c;
    bool add_node = false;
    while ((c = getopt(argc, argv, "i:a")) != -1) {
        switch (c) {
            case 'i':
                node_id = lexical_cast<int>(optarg);
                break;
            case 'a':
                add_node = true;
                break;
            case '?':
            default:
                std::cout << argv[0] << " -i node_id -[a]\n";
                return 0;
        }
    }
    if (node_id <= 0) {
        std::cerr << "error node_id " << node_id << std::endl;
        return 0;
    }

    if (add_node) {
        node_config_vector.clear();
        NodeConfig n4(4, "127.0.0.1", 9004);
        node_config_vector.push_back(n4);
    }

    RunNode(node_config_vector, node_id, add_node);

    return 0;
}
