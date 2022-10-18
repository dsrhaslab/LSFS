#ifndef P2PFS_DATA_HANDLER_LISTENER_H
#define P2PFS_DATA_HANDLER_LISTENER_H

#include <iostream>
#include <chrono>
#include <utility>
#include <cstdlib>
#include <ctime>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <kv_message.pb.h>

#include "df_pss/pss.h"
#include "df_store/kv_store.h"
#include "df_util/randomizer.h"
#include "df_util/util.h"
#include "anti_entropy.h"
#include "df_util/message_builder/message_builder.h"
#include "lsfs/fuse_common/macros.h"
#include "df_util/util_objects.h"
#include "df_util/serialize.h"


class data_handler_listener {
protected:
    std::string ip;
    int kv_port;
    long id;
    std::shared_ptr<kv_store<std::string>> store;
    group_construction* group_c_ptr;
    pss* pss_ptr;
    anti_entropy* anti_ent_ptr;
    float chance;
    int socket_send;
    std::mutex socket_send_mutex;
    bool smart_forward;
    std::atomic<long> anti_entropy_req_count = 0;


public:
    data_handler_listener(std::string ip, int kv_port, long id, float chance, pss *pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart);
    void reply_client(proto::kv_message& message, const std::string& sender_ip, int sender_port);
    void forward_message(const std::vector<peer_data>& view_to_send, proto::kv_message& message);
    void forward_decider(proto::kv_message &msg, const std::string& key);
    void process_get_message(proto::kv_message &msg);
    void process_get_latest_version_msg(proto::kv_message msg);
    void process_put_message(proto::kv_message &msg);
    void process_delete_message(proto::kv_message &msg);
    void process_put_child_message(proto::kv_message &msg);
    void process_get_latest_metadata_size_msg(proto::kv_message msg);
    void process_get_latest_metadata_stat_msg(proto::kv_message msg);
    void process_get_metadata_message(proto::kv_message &msg);

    long get_anti_entropy_req_count();
    void process_anti_entropy_message(proto::kv_message &msg);
    void process_anti_entropy_get_message(proto::kv_message& msg);
    void process_anti_entropy_get_metadata_message(proto::kv_message& msg);
    void process_anti_entropy_get_reply_message(proto::kv_message &msg);
    void process_anti_entropy_get_metadata_reply_message(proto::kv_message &msg);
    //void process_recover_request_msg(proto::kv_message& message);

    virtual void operator ()() = 0;
    virtual void stop_thread() = 0;
};


#endif //P2PFS_DATA_HANDLER_LISTENER_H
