//
// Created by danielsf97 on 1/16/20.
//

#ifndef P2PFS_DATA_HANDLER_LISTENER_H
#define P2PFS_DATA_HANDLER_LISTENER_H


#include <build/kv_message.pb.h>
#include "df_pss/pss.h"
#include "df_store/kv_store.h"

class data_handler_listener {
protected:
    std::string ip;
    //int port;
    long id;
    std::shared_ptr<kv_store<std::string>> store;
    pss* pss_ptr;
    float chance;
    int socket_send;
    std::mutex socket_send_mutex;
    bool smart_forward;
    std::atomic<long> anti_entropy_req_count = 0;

public:
    data_handler_listener(std::string ip/*, int port*/, long id, float chance, pss* pss, std::shared_ptr<kv_store<std::string>> store, bool smart);
    void reply_client(proto::kv_message& message, const std::string& sender_ip);
    void forward_message(const std::vector<peer_data>& view_to_send, proto::kv_message& message);
    void process_get_message(const proto::kv_message &msg);
    void process_get_reply_message(const proto::kv_message &msg);
    void process_put_message(const proto::kv_message &msg);
    void process_put_with_merge_message(const proto::kv_message &msg);
    long get_anti_entropy_req_count();
    void process_anti_entropy_message(const proto::kv_message &msg);
    void process_get_latest_version_msg(proto::kv_message msg);

    virtual void operator ()() = 0;
    virtual void stop_thread() = 0;
};


#endif //P2PFS_DATA_HANDLER_LISTENER_H
