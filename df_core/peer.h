//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_PEER_H
#define DATAFLASKSCPP_PEER_H

#include "../df_pss/pss.h"
#include "../df_pss/pss_listener.h"
#include "../df_pss/view_logger.h"
#include "../df_data_handlers/data_handler_listener.h"
#include "group_construction.h"
#include <memory>
#include <thread>
#include <df_data_handlers/anti_entropy.h>
#include <df_store/kv_store.h>
#include <spdlog/logger.h>

class peer {
public:
    inline const static char* boot_ip = "127.0.0.1";
    inline const static int boot_port = 12345;
    inline const static int pss_listener_thread_loop_size = 3;

private:
    long id;
    int pss_port;
    int data_port;
    std::string ip;
    double position;
    std::shared_ptr<kv_store<std::string>> store;
    pss cyclon;                     //thread functor que envia msgs
    group_construction group_c;
    pss_listener listener;
    view_logger v_logger;
    std::thread pss_th;
    std::thread pss_listener_th;
    std::thread v_logger_th;

    //GROUP CONSTRUCTION
    int rep_max;
    int rep_min;
    int max_age;
    bool local_message;
    int local_interval;

    //Data Threads
    float reply_chance;
    data_handler_listener data_handler;
    std::thread data_handler_th;

    //Anti Entropy Thread
    anti_entropy anti_ent;
    std::thread anti_ent_th;

    //LOGGER
    std::shared_ptr<spdlog::logger> logger;


public:
    peer(long id, std::string ip, int pss_port, int data_port, double position, std::shared_ptr<spdlog::logger> logger);
    peer(long id, std::string ip, int pss_port, int data_port, double position, long pss_boot_time, int pss_view_size, long pss_sleep_interval, int pss_gossip_size,
            int logging_interval, int anti_entropy_interval, std::string logging_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval, float reply_chance, bool smart, std::shared_ptr<spdlog::logger> logger);
    void print_view();
    void start();
    void stop();
    void join();
};


#endif //DATAFLASKSCPP_PEER_H
