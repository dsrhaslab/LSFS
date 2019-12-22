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

std::shared_ptr<peer> g_peer_impl;

peer::peer(long id, std::string ip, int port, double position):
    id(id), ip(ip), port(port), position(position),
    group_c(ip, port, id, position, 5, 10, 40, true, 15),
    cyclon(peer::boot_ip, peer::boot_port, ip, port,2,8,10,7, &(this->group_c)),
    listener("127.0.0.1", this->port, &(this->cyclon) ),
    v_logger(this->port, &(this->cyclon),60, "../logging/")
{
}

peer::peer(long id, std::string ip, int port, double position, long pss_boot_time, int pss_view_size, long pss_sleep_interval, int pss_gossip_size,
        int logging_interval, std::string logging_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval)
    :   id(id), ip(ip), port(port), position(position),rep_min(rep_min), rep_max(rep_max), max_age(max_age), local_message(local_message), local_interval(local_interval),
        group_c(ip, port, id, position, rep_min, rep_max, max_age,
                local_message, local_interval),
        cyclon(peer::boot_ip, peer::boot_port, ip, port, pss_boot_time, pss_view_size, pss_sleep_interval, pss_gossip_size, &(this->group_c)),
        listener("127.0.0.1", port, &(this->cyclon)),
        v_logger(port, &(this->cyclon), logging_interval, logging_dir)
{
}

void peer::print_view() {
    this->cyclon.print_view();
}

void peer::start() {
    this->pss_th = std::thread (std::ref(this->cyclon));
    this->pss_listener_th = std::thread(std::ref(this->listener));
    this->v_logger_th = std::thread(std::ref(this->v_logger));
}

void peer::stop(){
    this->v_logger.stop_thread();
    this->cyclon.stop_thread();
    this->listener.stop_thread();
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
}

void peer::join(){
    this->pss_listener_th.join();
    this->pss_th.join();
    this->v_logger_th.join();
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

    int port = atoi(argv[1]);
    long id = atol(argv[2]);
    double pos = atof(argv[3]);
    const char* conf_filename = argv[4];


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

    g_peer_impl = std::make_shared<peer>(id,"127.0.0.1",port,pos,2,view_size,sleep_interval,gossip_size, logging_interval, logging_dir,
            rep_max, rep_min, max_age, local_message, local_interval);
    g_peer_impl->start();
    g_peer_impl->join();
}