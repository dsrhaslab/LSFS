//
// Created by danielsf97 on 5/24/20.
//

#ifndef P2PFS_DATA_HANDLER_LISTENER_ST_H
#define P2PFS_DATA_HANDLER_LISTENER_ST_H


#include "data_handler_listener.h"

class data_handler_listener_st : public data_handler_listener{
private:
    int running;
    int socket_rcv;

public:
    data_handler_listener_st(std::string ip, long id, float chance, pss *pss, std::shared_ptr<kv_store<std::string>> store, bool smart);
    ~data_handler_listener_st() = default;
    void operator ()();
    void stop_thread();
};


#endif //P2PFS_DATA_HANDLER_LISTENER_ST_H
