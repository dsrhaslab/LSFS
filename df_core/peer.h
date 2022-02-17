//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_PEER_H
#define DATAFLASKSCPP_PEER_H

#include "df_pss/pss.h"
#include "df_pss/pss_listener.h"
#include "df_pss/view_logger.h"
#include "df_data_handlers/data_handler_listener.h"
#include "group_construction.h"
#include <memory>
#include <thread>
#include "df_data_handlers/anti_entropy.h"
#include "df_store/kv_store.h"
#include <spdlog/logger.h>

class peer {
public:
    inline const static int pss_listener_thread_loop_size = 3;
    inline const static int pss_port = 12344;
    inline const static int kv_port = 12348;
    inline const static int recover_port = 12349;

private:
    long id;
    int data_port;
    std::string ip;
    double position;
    bool view_logger_enabled;
    std::shared_ptr<kv_store<std::string>> store;
    pss cyclon;
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
    std::unique_ptr<data_handler_listener> data_handler;
    std::thread data_handler_th;

    //Anti Entropy Thread
    anti_entropy anti_ent;
    std::thread anti_ent_th;

    //LOGGER
    std::shared_ptr<spdlog::logger> logger;


public:
    peer(long id, std::string ip, std::string boot_ip, double position, long pss_boot_time, int pss_view_size, int pss_sleep_interval, int pss_gossip_size, bool view_logger_enabled,
            int logging_interval, int anti_entropy_interval, std::string logging_dir, std::string database_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval,
            float reply_chance, bool smart, bool mt_data_handler , std::shared_ptr<spdlog::logger> logger, long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at, bool recover_database);
    void print_view();
    void start(int warmup_interval, bool restart_database_after_warmup);
    void stop();
    void join();
};


#endif //DATAFLASKSCPP_PEER_H
