#ifndef P2PFS_ANTI_ENTROPY_H
#define P2PFS_ANTI_ENTROPY_H

#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <utility>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <kv_message.pb.h>
#include <spdlog/spdlog.h>
#include <exceptions/custom_exceptions.h>

#include "df_pss/pss.h"
#include "df_tcp_client_server_connection/tcp_client_server_connection.h"


class anti_entropy{

    enum Phase
    {
        Starting, Recovering, Operating
    };

private:
    pss* pss_ptr;
    group_construction* group_c;
    std::shared_ptr<kv_store<std::string>> store;
    std::atomic<bool> running;
    long sleep_interval;
    int sender_socket;
    std::string ip;
    int kv_port;
    int recover_port;
    long id;
    double pos;
    bool recover_database;
    int total_packet_size_percentage;

    std::mutex phase_mutex;
    std::condition_variable phase_cv;
    Phase phase;

public:
    anti_entropy(std::string ip, int kv_port, int recover_port, long id, double pos, pss *pss_ptr, group_construction* group_c, std::shared_ptr<kv_store<std::string>> store, long sleep_interval, bool recover_database, int total_packet_size_percentage);
    void operator()();
    void stop_thread();
    void wait_while_recovering();
    bool has_recovered();

private:
    void phase_starting();
    void phase_recovering();
    void phase_operating();
    bool recover_state(tcp_client_server_connection::tcp_server_connection&, int* socket);
    int send_recover_request(peer_data& target_peer);
    void send_peer_keys(std::vector<peer_data>& target_peers, proto::kv_message &msg);
};

#endif //P2PFS_ANTI_ENTROPY_H
