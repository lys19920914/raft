//
// Created by liangyusen on 2018/7/31.
//
#define BOOST_LOG_DYN_LINK

#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <ctime>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "raft_node.h"
#include "node_config.h"
#include "context.h"
#include "logger.h"

using namespace lraft;

RaftNode::RaftNode(const Context &context)
        : _io_ptr(std::make_shared<asio::io_service>()),
          election_(_io_ptr),
          heartbeat_(_io_ptr),
          add_node_(_io_ptr),
          sm_ptr(context.state_machine_ptr),
          raft_params_ptr(context.raft_params_ptr),
          node_to_join(nullptr),
          _cluster_config(context.node_config_vector),
          _snapshot_ptr(nullptr),
          _folder(context._folder_name),
          _node_rpc_ptr(std::make_shared<NodeRPCServer>(_io_ptr, _callback_function, context, sm_ptr)) {

    // 初始化
    mkdir(context._folder_name.c_str(), 0766);
    _entry_logs_ptr = std::make_shared<EntryLogs>(context._folder_name);

    Logger::GetLogger()->Init(context._log_file, context._folder_name + "/log", context._severity);
    RegisterCallback();

    if (context.add_node) { // 节点不是一开始的，是后来新增的
        node_state.catch_up = false;

        for (auto &nc: context.node_config_vector) {
            int node_id = nc.node_id;
            std::string host = nc.host;
            int port = nc.port;

            if (node_id == context.node_id) { // 是自己的配置
                node_config.node_id = node_id;
                node_config.host = host;
                node_config.port = port;
                break;
            }
        }
    } else {
        for (auto it = context.node_config_vector.cbegin(); it != context.node_config_vector.cend(); it++) {
            int node_id = (*it).node_id;
            std::string host = (*it).host;
            int port = (*it).port;

            if (node_id == context.node_id) { // 是自己的配置
                node_config.node_id = node_id;
                node_config.host = host;
                node_config.port = port;

            } else { // 是同伴的配置
                Peer peer(_io_ptr, (*it), RPCService);
                peer_map.insert(std::pair<int, Peer>(node_id, peer));
            }
        }

        // 看之前有没有快照，有的话就加载
        std::string filename = _folder + "/snapshot";
        std::fstream file;
        file.open(filename, std::fstream::binary | std::fstream::in | std::fstream::ate);
        if (file) { // 如果有文件，还要判断文件是否不为空
            file.seekg(0, std::fstream::end);
            if (file.tellg() > 0) {
                file.seekg(0);
                stringstream buf;
                buf << file.rdbuf();
                std::string str(buf.str());
                msgpack::object_handle oh = msgpack::unpack(str.data(), str.size());
                msgpack::object deserialized = oh.get();
                Snapshot ss;
                deserialized.convert(ss);
                _snapshot_ptr = std::make_shared<Snapshot>(ss._index, ss._term, ss._cluster_config, ss._data);
                sm_ptr->ResetStateMachine(ss._data);
                // 集群配置不从快照恢复，一律以新配置的为准
                node_state.lastApplied = _snapshot_ptr->_index;
                node_state.commitIndex = _snapshot_ptr->_index;
                node_state.currentTerm = _snapshot_ptr->_term;
            }
            file.close();
        }

        // 恢复完之后，开始选举时钟
        srand((unsigned) time(nullptr));
        election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
            this->HandleElectionTimeout();
        }, false);
        node_state._election_timeout_src = false;
    }
}

void RaftNode::HandleElectionTimeout() {
    LOG(LogType::info, "Election timeout, change to candidate.");

    node_state.leaderId = 0;

    node_state.role = Role::candidate;
    node_state.currentTerm++;
    node_state.votedNodeSet.clear();

    node_state.votedFor = node_config.node_id;
    node_state.votedNodeSet.insert(node_config.node_id);


    // 重置选举时钟
    election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
        this->HandleElectionTimeout();
    }, false);
    node_state._election_timeout_src = false;

    // Send RequestVoter RPCs to all other servers
    auto CalRes = [this](RaftConnectionClient *Ptr, unsigned long term, bool voteGranted, int src_node_id) {
        this->HandleRequestVoteRPCResponse(term, voteGranted, src_node_id);
    };
    for (auto &kv : peer_map) {
        if (kv.second.BuildConnection()) {
            kv.second.connection->CallRPC(CalRes, RPCID::REQUESTVOTE, node_state.currentTerm,
                                          node_config.node_id, _entry_logs_ptr->GetLastLogIndex(),
                                          _entry_logs_ptr->TermAt(_entry_logs_ptr->GetLastLogIndex()));
        }
    }
}

ResponseResult RaftNode::HandleRequestVoteRPC(unsigned long term, int candidateId, unsigned long lastLogIndex,
                                              unsigned long lastLogTerm) {

    ResponseResult response_result(node_state.currentTerm, false, node_config.node_id, node_state.lastApplied);

    // leader直接不管RequestVote，能让leader变为follower只能是从AppendEntries
    // election timeout src为true时，代表4.2.3 Disruptive servers的情况，即正常接收AppendEntry
    if (node_state.role == Role::leader || node_state._election_timeout_src) {
        return response_result;
    }

    LOG(LogType::info, "Receive RequestVote RPC from node %i. [term: %lu, lastLogIndex: %lu, lastLogTerm: %lu]",
        candidateId,
        term, lastLogIndex, lastLogTerm);

    // 比较term，如果term比较大，则自己变成follower
    TermCompare(term);

    bool log_up_to_date = false;
    bool voted = false;
    if (term < node_state.currentTerm) {
        response_result.success = false;
    } else {

        if (lastLogTerm > _entry_logs_ptr->TermAt(_entry_logs_ptr->GetLastLogIndex()) ||
            // if the logs last entries with different terms
            lastLogIndex >= _entry_logs_ptr->GetLastLogIndex()) { // then the log with the later term is more up-to-date
            log_up_to_date = true;// if the logs end with the same term, then whichever log is longer is more
        }
        // if votedFor is null or candidateId, and candidate's log is at
        // least as up-to-date as receive's log, grant vote
        voted = node_state.votedFor == -1 || node_state.votedFor == candidateId;
        if (log_up_to_date && voted) {
            node_state.currentTerm = term;
            node_state.votedFor = candidateId;
            response_result.term = term;
            response_result.success = true;
        }
    }

    if (!response_result.success) {
        LOG(LogType::info, "Reject RequestVote RPC. [term: %lu, log: %i, voted: %i]", node_state.currentTerm,
            log_up_to_date,
            voted);
    }

    return response_result;
}

void RaftNode::HandleRequestVoteRPCResponse(unsigned long term, bool voteGranted, int src_node_id) {

    if (node_state.role != Role::candidate) {
        LOG(LogType::debug, "Node is not candidate, ignore this RequestVote response.");
        return;
    }

    if (term == node_state.currentTerm && voteGranted) {
        LOG(LogType::info, "Receive vote from node %i.", src_node_id);
        node_state.votedNodeSet.insert(src_node_id);
        if (node_state.votedNodeSet.size() > (peer_map.size() + 1) / 2) {
            LOG(LogType::info, "This node become leader.");
            BecomeLeader();
        }
    }
}

void RaftNode::BecomeLeader() {
    node_state.role = lraft::Role::leader;
    node_state.leaderId = node_config.node_id;
    // 停止自己的选举时钟
    election_.CancelTimer();

    // 开始自己的heartbeat
    heartbeat_.SetTimer(posix_time::milliseconds(raft_params_ptr->heartbeat_timeout), [this]() {
        this->AppendEntriesRPC();
    }, true);

    // 日志相关

    // 初始化所有的nextIndex值为自己最后一条日志的index加1

    /*
     * 对于集群崩了之后，加载原先日志，重新选举的情况。
     * 此时leader的lastApply为快照的index。
     * leader的index为最后一条日志的index。
     * 由于没有持久化lastApply，是不知道日志中commit到哪条了的
     * 只能重新一条条和follower校对后，再commit
     * 校对的过程从entryLogs的最后一条开始。
     * matchIndex预置为0，以防被误用，只有appendEntry成功了，才会更新
     * 在这段时间内，已经可以接收请求了，只是这个请求没能commit那么快，要等前边的log先commit
     */

    for (std::map<int, Peer>::iterator it = peer_map.begin(); it != peer_map.end(); it++) {
        it->second.SetNextIndex(_entry_logs_ptr->GetLastLogIndex() + 1);
        it->second.SetMatchIndex(0);
    }

    // 广播领导人信息
    AppendEntriesRPC();
}

void RaftNode::AppendEntriesRPC() {
    for (auto &kv : peer_map) {
        AppendEntriesRPC(kv.second);
    }
}

void RaftNode::AppendEntriesRPC(Peer &peer) {


    if (peer.BuildConnection()) {

        // 如果有快照，并且要求的index小于等于快照index，那就要发快照
        if (_snapshot_ptr != nullptr && peer.nextIndex <= _snapshot_ptr->_index) {
            SendInstallSnapshotRPC(peer);
            // 这里发完可以return了，靠之后的response继续操作
            return;
        }

        auto CalRes = [this](RaftConnectionClient *Ptr, unsigned long term, bool success, int src_node_id,
                             unsigned long index) {
            this->HandleAppendEntriesRPCResponse(term, success, src_node_id, index);
        };

        // prevLogIndex和prevLogTerm就是所谓的前一条日志，用于校对
        // std::vector<Entry> entries = entryLogs.get_entries_after_index(peer.nextIndex);
        Entry entry = _entry_logs_ptr->GetEntryByIndex(peer.nextIndex);

        // paper最简单的说法是每次RPC只发一条entry
        // 发多条属于优化，如果发多条的话，回复中就要包括nextIndex或是entry size了
        // 暂时先只发一条吧
        peer.connection->CallRPC(CalRes, RPCID::APPENDENTRIES,
                                 node_state.currentTerm, node_config.node_id, peer.nextIndex - 1,
                                 _entry_logs_ptr->TermAt(peer.nextIndex - 1), entry,
                                 node_state.commitIndex);
        LOG(LogType::info,
            "Send AppendEntries RPC to node %i. [term: %lu, leaderId: %i, prevLogIndex: %lu, prevLogTerm: %lu, entry_type: %s, leaderCommit: %lu]",
            peer.node_config.node_id, node_state.currentTerm, node_config.node_id, peer.nextIndex - 1,
            _entry_logs_ptr->TermAt(peer.nextIndex - 1), entry._type.c_str(), node_state.commitIndex);
    }
}

ResponseResult RaftNode::HandleAppendEntriesRPC(unsigned long term, int leaderId, unsigned long prevLogIndex,
                                                unsigned long prevLogTerm,
                                                Entry entry, unsigned long leaderCommit) {

    LOG(LogType::debug,
        "Receive AppendEntries RPC. [prevLogIndex: %lu, prevLogTerm: %lu, entry_type: %s, leaderCommit: %lu]",
        prevLogIndex, prevLogTerm, entry._type.c_str(), leaderCommit);

    TermCompare(term);

    if (node_state.catch_up) {
        election_.CancelTimer();
        election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
            this->HandleElectionTimeout();
        }, false);
        node_state._election_timeout_src = true;
    }

    node_state.leaderId = leaderId;

    ResponseResult response_result(node_state.currentTerm, false, node_config.node_id,
                                   _entry_logs_ptr->GetLastLogIndex());

    // 按照之前说的，先做最基本的每次只发一条entry的情况

    // 1. Reply false if term < currentTerm
    if (term < node_state.currentTerm) {
        LOG(LogType::info, "Leader's term is smaller than follower's term.");
        return response_result;
    }

    // 2. Reply false if log doesn't contain an entry at prevLogIndex
    // whose term matches prevLogTerm
    // 情况1：一开始的情况，prevLogIndex == 0，查找过去，entry的term确实也是0，那不需要返回，走下面的
    // 情况2：这个follower没跟上，导致不包含prevLogIndex。此时应该是找不到这条，即找回的term == 0
    // 情况3：存在冲突日志的情况，即能找到prevLogIndex，但是term不同，也返回去，让leader把index倒退
    bool find = true;
    Entry prev_entry = _entry_logs_ptr->GetEntryByIndex(prevLogIndex);
    if (prevLogIndex == 0) {

    } else if (prev_entry._term == 0) {
        find = false; // 因为现在留了一条日志，所以做了快照，也能找到这条
    } else if (prev_entry._term != prevLogTerm)
        find = false;
    // 经过上述判断后，prevLogIndex和prevLogTerm都对上了，肯定会接受。
    if (!find) {
        LOG(LogType::debug, "Follower can not find prevLogIndex.");
        return response_result;
    }


    // 3. If an existing entry conflicts with a new one(same index
    // buf different terms), delete the existing entry and all that
    // follow it
    response_result.success = true;
    if (entry._term > 0) { // 大于0，表示这条是真的entry
        Entry current_entry = _entry_logs_ptr->GetEntryByIndex(prevLogIndex + 1);
        // 情况1：entry._term == 0的时候，表示没有这条日志，很正常，不做处理，跳4
        // 情况2：entry._term != 0，且term不匹配（而且是在prevLogIndex和prevLogTerm都匹配之后，那就要删除这个index及之后的，再添加）
        // 情况3：这条日志已经存在的情况。注意这种，因为有时没及时回复，会两条重复
        if (current_entry._term == 0) {

        } else if (current_entry._term != entry._term) {
            _entry_logs_ptr->DeleteEntryAfterIndex(prevLogIndex + 1);
        }

        // 4. Append any new entries not already in the log
//        for (auto it = entries.cbegin(); it != entries.cend(); it++) {
//            entryLogs.append_entry(*it);
//        }
        if (current_entry._term == entry._term) {
            // 已经存在时，不做处理
        } else {
            _entry_logs_ptr->AppendEntry(entry);
        }
    }

    // 5. If leaderCommit > commitIndex, set commitIndex =
    // min(leaderCommit, index of last new entry)
    // 把需要commitIndex是需要commit到的log的index
    // 如果说last new entry还没到这个index，那就设last new entry
    int entry_size = 0;
    if (entry._term != 0)
        entry_size = 1;
    unsigned long min_commit = std::min(leaderCommit, prevLogIndex + entry_size);
    node_state.commitIndex = min_commit;
    Commit();

    response_result._index = _entry_logs_ptr->GetLastLogIndex();
    return response_result;
}

void RaftNode::BecomeFollower() {
    LOG(LogType::debug, "From candidate to follow.");
    election_.CancelTimer();
    election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
        this->HandleElectionTimeout();
    }, false);
}

void RaftNode::HandleAppendEntriesRPCResponse(unsigned long term, bool success, int src_node_id, unsigned long index) {

    LOG(LogType::debug, "Receive AppendEntries RPC response from node %i. [term: %lu, success: %i, index: %lu]",
        src_node_id,
        term,
        success, index);

    // 处理要添加的server
    if (node_to_join != nullptr && src_node_id == node_to_join->node_config.node_id) {

        add_node_.CancelTimer();

        if (success) {
            node_to_join->nextIndex = index + 1;
            node_to_join->matchIndex = index;

            int gap = (int) (_entry_logs_ptr->GetLastLogIndex() - node_to_join->nextIndex);
            if (gap < 3) {
                // 表示追上了
                LOG(LogType::info, "Node %i has catch up, gap is %i.", src_node_id, gap);

                std::vector<NodeConfig> ncv;
                ncv.insert(ncv.end(), _cluster_config.begin(), _cluster_config.end());
                ncv.push_back(node_to_join->node_config);
                Entry entry("config", _entry_logs_ptr->GetLastLogIndex() + 1, node_state.currentTerm, ncv,
                            add_node_conn_hash, add_node_SId);
                _entry_logs_ptr->AppendEntry(entry);

                AppendEntriesRPC();

            } else {
                // 没追上，继续发同步
                AppendEntriesRPC(*node_to_join);
                add_node_.SetTimer(posix_time::milliseconds(raft_params_ptr->add_node_timeout), [this]() {
                    this->HandleAddServerTimeout();
                }, false);
            }
        }
        return;
    }

    auto it = peer_map.find(src_node_id);
    if (it == peer_map.end()) {
        LOG(LogType::warning, "Receive response comes from an unknow node%i.", src_node_id);
        return;
    }

    if (success) { // If successful: update nextIndex and matchIndex for follower
        it->second.matchIndex = index;
        it->second.nextIndex = index + 1;

        // 遍历所有peer，看matchIndex大于等于这条的有多少个，大于等于一半时，带上leader，就可以commit了
        unsigned int count = 1; // leader自己算一个
        for (auto peer = peer_map.cbegin(); peer != peer_map.cend(); peer++) {
            if (peer->second.matchIndex >= it->second.matchIndex)
                count++;
        }
        if (count > (peer_map.size() + 1) / 2) {
            node_state.commitIndex = it->second.matchIndex;
            Commit();
        }
    } else { // If AppendEntries fails because of log inconsistency:
        it->second.nextIndex--;
        AppendEntriesRPC(it->second); // 直接就给这个peer重发AppendEntry
    }
}

void RaftNode::Commit() {
    for (unsigned int i = node_state.lastApplied + 1; i <= node_state.commitIndex; i++) {
        Entry entry = _entry_logs_ptr->GetEntryByIndex(i);

        LOG(LogType::debug, "Commit entry. [index: %i]", i);

        if (entry._type == "normal") {
            sm_ptr->Commit(_node_rpc_ptr->GetConnection(entry._conn_hash).get(), _SM_RPCService, entry._SId,
                           entry._command);
            if (node_state.role == Role::leader) {
                // ClientResult clientResult(true, response, node_config.host, node_config.port);
                // ResponseToClient(entry._SId, clientResult);
            }
        } else if (entry._type == "config") {
            // 把SId传到函数里，回复客户端
            ClusterMembershipChanges(entry._conn_hash, entry._SId, entry._node_config_vector);
        }
        node_state.lastApplied = i;

        TakeSnapshot();

        if (node_state.role == Role::leader) {
            AppendEntriesRPC();
        }
    }
}

void RaftNode::TermCompare(unsigned long term) {
    if (term > node_state.currentTerm) {
        node_state.currentTerm = term;
        if (node_state.role != Role::follow) {
            node_state.role = Role::follow;
            BecomeFollower();
        }
        // 清空选举信息，这部分在论文里边好像没看到，在demo里边看到的
        node_state.votedFor = -1;
        node_state.votedNodeSet.clear();
    }
}

int RaftNode::GetRandomElectionTimeout() {
    return rand() %
           (raft_params_ptr->election_timeout_upper_bound - raft_params_ptr->election_timeout_lower_bound) +
           raft_params_ptr->election_timeout_lower_bound;
}

void RaftNode::HandleAddServerRPC(int conn_hash, uint16_t SId, lraft::NodeConfig nodeConfig) {
    if (node_state.leaderId == 0) { // 正在选举
        RPCResponse(conn_hash, SId, false, "ELECTION_NOW");
        return;
    }

    if (node_state.role != Role::leader) { // 不是leader
        RPCResponse(conn_hash, SId, false, "NOT_LEADER");
        return;
    }

    if (node_state.config_changing) { // 集群正在变更
        RPCResponse(conn_hash, SId, false, "CHANGING_NOW");
        return;
    }

    LOG(LogType::info, "Start to add server. [node_id: %i, host: %s, port: %i].", nodeConfig.node_id, nodeConfig.host,
        nodeConfig.port);


    //    集群中包含A B C，A为Leader，现在添加节点D。
    //    1）清空D节点上的所有数据，避免有脏数据。
    //    2）Leader将存量的日志通过AppendEntry RPC同步到D，使D的数据跟上其他节点。
    //    3）待D的日志追上后，Leader A创建一条Config Entry，其中集群信息包含ABCD。
    //    4）Leader A将Config Entry同步给B C D，Follower收到后应用，之后所有节点的集群信息都变为ABCD，添加完成。
    //    注：在步骤2过程中,Leader仍在不断接收客户请求生成Entry，所以只要D与A日志相差不大即认为D已追上。

    node_state.config_changing = true; // 这里就要置为true了，因为node_to_join只有1个
    add_node_SId = SId;
    add_node_conn_hash = conn_hash;
    node_to_join = std::make_shared<Peer>(_io_ptr, nodeConfig, RPCService);
    node_to_join->nextIndex = 1;
    AppendEntriesRPC(*node_to_join);

    // 开一个定时器，如果超过时间，新node还没回应，就不添加了
    add_node_.SetTimer(posix_time::milliseconds(raft_params_ptr->add_node_timeout), [this]() {
        this->HandleAddServerTimeout();
    }, false);
}

void RaftNode::HandleAddServerTimeout() {
    LOG(LogType::info, "Add node%i timeout.", node_to_join->node_config.node_id);
    node_state.config_changing = false;
    node_to_join = nullptr;
    RPCResponse(add_node_conn_hash, add_node_SId, true, "Add node timeout.");
}

void RaftNode::ClusterMembershipChanges(int conn_hash, uint16_t SId, std::vector<NodeConfig> &node_config_vector) {
    node_state.config_changing = false;

    if (!node_state.catch_up) { // 说明是新服务器
        node_state.catch_up = true;
        srand((unsigned) time(nullptr));

        election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
            this->HandleElectionTimeout();
        }, false);
        node_state._election_timeout_src = false;

        for (auto it = node_config_vector.cbegin(); it != node_config_vector.cend(); it++) {
            int node_id = (*it).node_id;

            if (node_id == node_config.node_id) { // 是自己的配置
                continue;
            } else { // 是同伴的配置
                Peer peer(_io_ptr, (*it), RPCService);
                peer_map.insert(std::pair<int, Peer>(node_id, peer));
            }
        }
    } else {
        // 增加服务器
        if (node_config_vector.size() > _cluster_config.size()) {
            // 找到需要添加的node
            NodeConfig node_to_add;
            for (auto &nc:node_config_vector) {
                bool to_add = true;
                for (auto &cc:_cluster_config) {
                    if (nc.node_id == cc.node_id) {
                        to_add = false;
                        break;
                    }
                }
                if (to_add) {
                    node_to_add = nc;
                    break;
                }
            }
            Peer peer(_io_ptr, node_to_add, RPCService);
            // peer_map[node_to_add.node_id] = peer; // 注意不能这样写，出错的
            peer_map.insert(std::pair<int, Peer>(node_to_add.node_id, peer));
            _cluster_config.push_back(node_to_add);

            if (node_state.role == Role::leader) {
                node_state.config_changing = false;
                peer_map.find(node_to_add.node_id)->second.nextIndex = node_to_join->nextIndex;
                node_to_join = nullptr;

                // 回复客户端
                std::string response = "Add node" + to_string(node_to_add.node_id) + " success.";
                RPCResponse(conn_hash, SId, true, response);
            }

        } else if (node_config_vector.size() < _cluster_config.size()) { // 移除服务器
            // 找到需要移除的node
            int id_to_remove;

            for (auto it = _cluster_config.cbegin(); it != _cluster_config.cend(); it++) {
                bool remove = true;
                for (auto &nc:node_config_vector) {
                    if (nc.node_id == it->node_id) {
                        remove = false;
                        break;
                    }
                }
                if (remove) {
                    id_to_remove = it->node_id;
                    _cluster_config.erase(it);
                    break;
                }
            }
            if (id_to_remove == node_config.node_id) { // 要被移除的是自己
                LOG(LogType::info, "This node is removed.");
                election_.CancelTimer();
                _io_ptr->stop();
                exit(0); // 这个要好好写下退出过程
            } else {

                if (node_state.role == Role::leader) {
                    // 从map中移除了，就发不了信息了，所以在移除之前，发一条commit过去
                    AppendEntriesRPC(peer_map.find(id_to_remove)->second);

                    // 回复客户端
                    std::string response = "Remove node" + to_string(id_to_remove) + " success.";
                    RPCResponse(conn_hash, SId, true, response);

                }
                if (peer_map.find(id_to_remove)->second.connection != nullptr)
                    peer_map.find(id_to_remove)->second.connection->Stop();

                peer_map.erase(id_to_remove);
                LOG(LogType::info, "Node %i is removed.", id_to_remove);
            }
        }
    }
}

void RaftNode::HandleRemoveServerRPC(int conn_hash, uint16_t SId, int node_id) {

    if (node_state.leaderId == 0) { // 正在选举
        RPCResponse(conn_hash, SId, false, "ELECTION_NOW");
        return;
    }

    if (node_state.role != Role::leader) { // 不是leader
        RPCResponse(conn_hash, SId, false, "NOT_LEADER");
        return;
    }

    if (node_state.config_changing) { // 集群正在变更
        RPCResponse(conn_hash, SId, false, "CHANGING_NOW");
        return;
    }

    //    集群中原来包含A B C D，A为Leader，现在剔除节点D。
    //    1) Leader A创建一条Config Entry，其中集群信息为ABC。
    //    2) A将日志通过AppendEntry RPC同步给节点B C。
    //    3) A B C在应用该日志后集群信息变为ABC，A不再发送AppendEntry给D，D从集群中移除。
    //    4) 此时D的集群信息依旧为ABCD，在选举超时到期后，发起选举，为了防止D的干扰，引入额外机制：所有节点在正常接收Leader的AppendEntry时，拒绝其他节点发来的选举请求。
    //    5) 将D的数据清空并下线。
    node_state.config_changing = true;

    LOG(LogType::info, "Start to remove node %i.", node_id);

    std::vector<NodeConfig> ncv;
    ncv.insert(ncv.end(), _cluster_config.begin(), _cluster_config.end());
    for (auto it = ncv.cbegin(); it != ncv.cend(); it++) {
        if (it->node_id == node_id) {
            ncv.erase(it);
            break;
        }
    }
    Entry entry("config", _entry_logs_ptr->GetLastLogIndex() + 1, node_state.currentTerm, ncv, conn_hash, SId);
    _entry_logs_ptr->AppendEntry(entry);
    AppendEntriesRPC();
}

void RaftNode::TakeSnapshot() {
    if ((int) (node_state.lastApplied - _entry_logs_ptr->GetStartIndex()) < raft_params_ptr->_max_log_length) {
        return; // 如果未到，直接返回
    }

    // 这里应该要考虑一个并发的问题
    LOG(LogType::info, "Take a snapshot. [last_applied: %lu, start_index: %lu]", node_state.lastApplied,
        _entry_logs_ptr->GetStartIndex());

    // 现在在同一个进程里边做，可以考虑得简单些


    // 1. Raft first stores the state it needs for a restart: the index
    // and term of the last entry included in the snapshot and the latest configuration as of that index.
    // 这里应该说的不是日志的lastIndex，是这个lastApplied的index，以及对应的term
    auto snapshot_ptr = std::make_shared<Snapshot>(node_state.lastApplied,
                                                   _entry_logs_ptr->TermAt(node_state.lastApplied), _cluster_config,
                                                   sm_ptr->Serialize());

    // 2. Then it discards the prefix of its log up through that index.
    _entry_logs_ptr->DeleteEntryBeforeIndex(snapshot_ptr->_index);

    // 3. Any previous snapshots can also be discarded
    // discard分2部分，第一部分是memory中的snapshot对象，第二部分是磁盘中的snapshot文件，两部分都是覆盖掉就好了
    _snapshot_ptr = snapshot_ptr;
    SaveSnapshot();
}

void RaftNode::SendInstallSnapshotRPC(lraft::Peer &peer) {
    auto CalRes = [this](RaftConnectionClient *Ptr, unsigned long term, bool success, int src_node_id,
                         unsigned long index) {
        this->HandleAppendEntriesRPCResponse(term, success, src_node_id, index);
    };

    // 现在snapshot是把data都存在内存里了，可能比较占内存
    // 以后如果有必要，就不存data，要用到的时候，再从磁盘读

    if (peer.BuildConnection()) {
        peer.connection->CallRPC(CalRes, RPCID::INSTALLSNAPSHOT, node_state.currentTerm,
                                 node_config.node_id, _snapshot_ptr->_index,
                                 _snapshot_ptr->_term, _snapshot_ptr->_cluster_config, _snapshot_ptr->_data);

        LOG(LogType::info,
            "Send InstallSnapshot RPC to node %i. [term: %lu, leaderId: %i, lastIndex: %lu, lastTerm: %lu, lastConfig_size: %i]",
            peer.node_config.node_id, node_state.currentTerm, node_config.node_id, _snapshot_ptr->_index,
            _snapshot_ptr->_term, _snapshot_ptr->_cluster_config.size());

    }
}


ResponseResult RaftNode::HandleInstallSnapshotRPC(unsigned long term, int leaderId, unsigned long lastIndex,
                                                  unsigned long lastTerm, std::vector<NodeConfig> &lastConfig,
                                                  const std::string &data) {

    LOG(LogType::info,
        "Receive InstallSnapshot RPC from node %i. [term: %lu, leaderId: %i, lastIndex: %lu, lastTerm: %lu, lastConfig_size: %i]",
        leaderId, term, leaderId, lastIndex, lastTerm, lastConfig.size());

    if (node_state.catch_up) {
        election_.CancelTimer();
        // 为了调试，选举时钟先注释掉
        election_.SetTimer(posix_time::milliseconds(GetRandomElectionTimeout()), [this]() {
            this->HandleElectionTimeout();
        }, false);
        node_state._election_timeout_src = true;
    }

    node_state.leaderId = leaderId;
    TermCompare(term);

    ResponseResult responseResult(node_state.currentTerm, false, node_config.node_id, node_state.lastApplied);

    // if lastIndex is larger than snapshot's, save snapshot file and Raft state(lastIndex, lastTerm, lastConfig). Discard any existing or partial snapshot
    _snapshot_ptr = std::make_shared<Snapshot>(lastIndex, lastTerm, lastConfig,
                                               data); // 不单单是把这个快照应用，也把这个node的快照，替换为传来的
    SaveSnapshot();

    // If existing log entry has same index and term as lastIndex and lastTerm, discard log up through lastIndex( but retain any following entries) and reply
    _entry_logs_ptr->DeleteEntryBeforeIndex(lastIndex);
    // 要假如一条lastindex和lastterm的，和其他服务器一样，填充一条进去
    if (_entry_logs_ptr->GetLogsSize() == 0) {
        Entry entry("null", lastIndex, lastTerm, "", 0, 0);
        _entry_logs_ptr->AppendEntry(entry);
        _entry_logs_ptr->SetStartIndex(lastIndex);
    }


//    7. discard the entire log


//    8. reset state machine using snapshot contents(and load lastConfig as cluster configuration)
    sm_ptr->ResetStateMachine(data);
    _cluster_config = lastConfig; // 这里还要重新配置一下，如果有必要
    node_state.lastApplied = lastIndex;
    responseResult.success = true;
    responseResult._index = node_state.lastApplied;
    return responseResult;
}

void RaftNode::SaveSnapshot() {
    std::string filename = _folder + "/snapshot";
    std::fstream file;
    file.open(filename, std::fstream::binary | std::fstream::out);

    std::stringstream buffer;
    msgpack::pack(buffer, *_snapshot_ptr);
    buffer.seekg(0);
    std::string str(buffer.str());

    file.seekp(0);
    file.write(str.c_str(), str.size());
    file.close();
}

void RaftNode::HandleClientRequest(int conn_hash, unsigned short SId, const std::string &command) {
    if (node_state.leaderId == 0) { // 正在选举
        RPCResponse(conn_hash, SId, false, "ELECTION_NOW");
        return;
    }

    if (node_state.role != Role::leader) { // 不是leader
        RPCResponse(conn_hash, SId, false, "NOT_LEADER");
        return;
    }

    LOG(LogType::debug, "Receive request from server. [msg: %s]", command.c_str());

    Entry entry("normal", _entry_logs_ptr->GetLastLogIndex() + 1, node_state.currentTerm, command, conn_hash, SId);
    _entry_logs_ptr->AppendEntry((entry));

    AppendEntriesRPC();
}

void RaftNode::HandleClientQuery(int conn_hash, unsigned short SId, const std::string &query) {
    if (node_state.leaderId == 0) { // 正在选举
        RPCResponse(conn_hash, SId, false, "ELECTION_NOW");
        return;
    }

    if (node_state.role != Role::leader) { // 不是leader
        RPCResponse(conn_hash, SId, false, "NOT_LEADER");
        return;
    }

    _SM_RPCService.UnpackCmd(_node_rpc_ptr->GetConnection(conn_hash).get(), SId, query.c_str(), query.size());
}

void RaftNode::RegisterCallback() {
    _callback_function.RequestVoteFun = std::bind(&RaftNode::HandleRequestVoteRPC, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

    _callback_function.AppendEntriesFun = std::bind(&RaftNode::HandleAppendEntriesRPC, this, std::placeholders::_1,
                                                    std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
                                                    std::placeholders::_5, std::placeholders::_6);

    _callback_function.InstallSnapshotFun = std::bind(&RaftNode::HandleInstallSnapshotRPC, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3,
                                                      std::placeholders::_4,
                                                      std::placeholders::_5, std::placeholders::_6);

    _callback_function.AddServerFun = std::bind(&RaftNode::HandleAddServerRPC, this, std::placeholders::_1,
                                                std::placeholders::_2, std::placeholders::_3);

    _callback_function.RemoveServerFun = std::bind(&RaftNode::HandleRemoveServerRPC, this, std::placeholders::_1,
                                                   std::placeholders::_2, std::placeholders::_3);


    _callback_function.ClientRequestFun = std::bind(&RaftNode::HandleClientRequest, this, std::placeholders::_1,
                                                    std::placeholders::_2, std::placeholders::_3);

    _callback_function.ClientQueryFun = std::bind(&RaftNode::HandleClientQuery, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3);
}


// 这个回复是commit之后，或者query请求已经到达leader的情况下，由用户调用的，所以只要是leader的服务器回复就好
void
RaftNode::RPCResponse(RaftConnectionServer *ConnPtr, unsigned short SId, bool status, const std::string &response) {
    if (node_state.role != Role::leader) {
        return;
    }
    int conn_hash = std::hash<RaftConnectionServer *>{}(ConnPtr);
    auto conn = _node_rpc_ptr->GetConnection(conn_hash);
    if (conn != nullptr)
        conn->RPCResponse(SId, status, response, node_config.host, node_config.port);
}

// 这个回复是收到请求时，由于非leader，或是此时无法执行请求时，Node直接回复的，要告诉客户端leader信息
void RaftNode::RPCResponse(int conn_hash, unsigned short SId, bool status, const std::string &response) {
    std::string host = "";
    int port = 0;
    if (node_state.leaderId != 0) {
        for (auto &cc:_cluster_config) {
            if (cc.node_id == node_state.leaderId) {
                host = cc.host;
                port = cc.port;
            }
        }
    }

    auto conn = _node_rpc_ptr->GetConnection(conn_hash);
    if (conn != nullptr)
        conn->RPCResponse(SId, status, response, host, port);
}

void RaftNode::Run() {
    _io_ptr->run();
}

