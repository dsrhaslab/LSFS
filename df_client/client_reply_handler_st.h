//
// Created by danielsf97 on 1/27/20.
//

#ifndef P2PFS_CLIENT_REPLY_HANDLER_ST_H
#define P2PFS_CLIENT_REPLY_HANDLER_ST_H


#include <string>
#include <unordered_map>
#include <set>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <regex>

#include "kv_message.pb.h"
#include <spdlog/spdlog.h>

#include "client_reply_handler.h"
#include "df_store/kv_store_key.h"
#include "exceptions/custom_exceptions.h"



class client_reply_handler_st : public client_reply_handler{
private:
    int running;
    int socket_rcv;

public:
    client_reply_handler_st(std::string ip, int kv_port, int pss_port, long wait_timeout);
    ~client_reply_handler_st();
    void operator ()();
    void stop();
};


#endif //P2PFS_CLIENT_REPLY_HANDLER_ST_H
