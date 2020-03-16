//
// Created by danielsf97 on 10/8/19.
//

#include <iostream>
#include "peer.h"
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "../df_store/kv_store_memory.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <stdlib.h>
#include <df_store/kv_store_wiredtiger.h>
#include <df_store/kv_store_leveldb.h>
#include <df_store/kv_store_memory_v2.h>

std::shared_ptr<peer> g_peer_impl;

peer::peer(long id, std::string ip, int pss_port, int data_port, double position, std::shared_ptr<spdlog::logger> logger):
    id(id), ip(ip), pss_port(pss_port), data_port(data_port), position(position), logger(logger),
    store(std::make_shared<kv_store_leveldb>()),
//    store(std::make_shared<kv_store_wiredtiger>()),
//    store(std::make_shared<kv_store_memory_v2<std::string>>()),
//    store(std::make_shared<kv_store_memory<std::string>>()),
    group_c(ip, pss_port, id, position, 5, 10, 40, true, 15, this->store, logger),
    cyclon(peer::boot_ip, peer::boot_port, ip, pss_port, id, position,2,8,10,7, &(this->group_c)),
    listener("127.0.0.1", this->pss_port, &(this->cyclon) ),
    v_logger(this->pss_port, &(this->cyclon),60, "../logging/"),
    data_handler(ip, data_port, id, 1, &(this->cyclon), this->store, false),
    anti_ent(ip, data_port, id, &(this->cyclon), this->store, 20)
{
    std::string database_folder = std::string("/home/danielsf97/Desktop/") + this->store->db_name() + "/";
    int res = this->store->init((void*) database_folder.c_str(), id);
    if(res != 0) {
        exit(1);
    }
}

peer::peer(long id, std::string ip, int pss_port, int data_port,double position, long pss_boot_time, int pss_view_size, int pss_sleep_interval, int pss_gossip_size,
        int logging_interval, int anti_entropy_interval, std::string logging_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval, float reply_chance, bool smart, std::shared_ptr<spdlog::logger> logger)
    :   id(id), ip(ip), pss_port(pss_port), data_port(data_port), position(position),rep_min(rep_min), rep_max(rep_max), max_age(max_age), local_message(local_message), logger(logger),
        local_interval(local_interval), reply_chance(reply_chance),
//        store(std::make_shared<kv_store_leveldb>()),
        store(std::make_shared<kv_store_wiredtiger>()),
//        store(std::make_shared<kv_store_memory_v2<std::string>>()),
//        store(std::make_shared<kv_store_memory<std::string>>()),
        data_handler(ip, data_port, id, reply_chance, &(this->cyclon), this->store, smart),
        anti_ent(ip, data_port, id, &(this->cyclon), this->store, anti_entropy_interval),
        group_c(ip, pss_port, id, position, rep_min, rep_max, max_age, local_message, local_interval, this->store, logger),
        cyclon(peer::boot_ip, peer::boot_port, ip, pss_port, id, position,pss_boot_time, pss_view_size, pss_sleep_interval, pss_gossip_size, &(this->group_c)),
        listener("127.0.0.1", pss_port, &(this->cyclon)),
        v_logger(pss_port, &(this->cyclon), logging_interval, logging_dir)
{
    std::string database_folder = std::string("/home/danielsf97/Desktop/") + this->store->db_name() + "/";
    int res = this->store->init((void*) database_folder.c_str(), id);
    if(res != 0) {
        exit(1);
    }
}

void peer::print_view() {
    this->cyclon.print_view();
}

void peer::start() {
    this->pss_th = std::thread (std::ref(this->cyclon));
    this->pss_listener_th = std::thread(std::ref(this->listener));
    this->v_logger_th = std::thread(std::ref(this->v_logger));
    this->data_handler_th = std::thread(std::ref(this->data_handler));
    this->anti_ent_th = std::thread(std::ref(this->anti_ent));
}

void peer::stop(){
    this->v_logger.stop_thread();
    this->cyclon.stop_thread();
    this->listener.stop_thread();
    this->data_handler.stop_thread();
    this->anti_ent.stop_thread();
    this->store->close();
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
    this->data_handler_th.join();
    this->anti_ent_th.join();
}

void peer::join(){
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
    this->data_handler_th.join();
    this->anti_ent_th.join();
}

void term_handler(int i){
    g_peer_impl->stop();
    std::cout << "Terminating with handler!!" << std::endl;
    exit(1);
}


int main(int argc, char* argv []){

    if(argc < 4){
        exit(1);
    }

    signal(SIGTERM, term_handler);

    int pss_port = atoi(argv[1]);
    int data_port = atoi(argv[2]);
    long id = atol(argv[3]);
    double pos = atof(argv[4]);
    const char* conf_filename = argv[5];


    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    int view_size = main_confs["view_size"].as<int>();
    int gossip_size = main_confs["gossip_size"].as<int>();
    int sleep_interval = main_confs["message_passing_interval_sec"].as<int>();
    int logging_interval = main_confs["log_interval_sec"].as<int>();
    std::string logging_dir = main_confs["logging_dir"].as<std::string>();
    int rep_max = main_confs["rep_max"].as<int>();
    int rep_min = main_confs["rep_min"].as<int>();
    int max_age = main_confs["max_age"].as<int>();
    int local_message = main_confs["local_message"].as<bool>();
    int local_interval = main_confs["local_interval_sec"].as<int>();
    int anti_entropy_interval = main_confs["anti_entropy_interval_sec"].as<int>();
    float reply_chance = main_confs["reply_chance"].as<float>();
    bool smart = main_confs["smart"].as<bool>();

    std::shared_ptr<spdlog::logger> logger;
    try
    {
        logger = spdlog::basic_logger_mt("basic_logger", "logs/" + std::to_string(pss_port) + ".txt");
        logger->set_level(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    srand (time(NULL));
    int boot_time = rand() % 10 + 2;

    g_peer_impl = std::make_shared<peer>(id,"127.0.0.1",pss_port,data_port,pos,boot_time,view_size,sleep_interval,gossip_size, logging_interval, anti_entropy_interval, logging_dir,
            rep_max, rep_min, max_age, local_message, local_interval, reply_chance, smart, logger);
    g_peer_impl->start();
    g_peer_impl->join();
}