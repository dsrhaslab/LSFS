#ifndef P2PFS_DATA_HANDLER_LISTENER_MT_H
#define P2PFS_DATA_HANDLER_LISTENER_MT_H

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>

#include <df_communication/udp_async_server.h>
#include "data_handler_listener.h"

class data_handler_listener_mt : public data_handler_listener{
private:
    int nr_worker_threads = 3;
    boost::asio::io_service io_service;
    boost::thread_group thread_pool;

public:
    data_handler_listener_mt(std::string ip, int kv_port, long id, float chance, pss *pss, group_construction* group_c, anti_entropy* anti_ent, std::shared_ptr<kv_store<std::string>> store, bool smart);
    ~data_handler_listener_mt() = default;
    void operator ()();
    void stop_thread();
};

#endif //P2PFS_DATA_HANDLER_LISTENER_MT_H
