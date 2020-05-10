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
//#include "df_store/kv_store_memory.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <stdlib.h>
#include "df_store/kv_store_wiredtiger.h"
#include "df_store/kv_store_leveldb.h"
//#include "df_store/kv_store_memory_v2.h"

// definition
extern std::string merge_metadata(const std::string&, const std::string&);

std::shared_ptr<peer> g_peer_impl;

peer::peer(long id, std::string ip, std::string boot_ip/*, int pss_port, int data_port*/, double position, std::shared_ptr<spdlog::logger> logger):
    id(id), ip(ip)/*, pss_port(pss_port)*/, data_port(data_port), position(position), logger(logger), view_logger_enabled(false),
//    store(std::make_shared<kv_store_leveldb>(merge_metadata, 100, 100, 100)),
    store(std::make_shared<kv_store_wiredtiger>(merge_metadata, 100, 100, 100)),
//    store(std::make_shared<kv_store_memory_v2<std::string>>(merge_metadata, 100, 100, 100)),
//    store(std::make_shared<kv_store_memory<std::string>>(merge_metadata, 100, 100, 100)),
    group_c(ip/*, pss_port*/, id, position, 5, 10, 40, true, 15, this->store, logger),
    cyclon(boot_ip.c_str()/*, peer::boot_port*/, ip/*, pss_port*/, id, position,2,8,10,7, &(this->group_c)),
    listener(/*"127.0.0.1", this->pss_port,*/ &(this->cyclon) ),
    v_logger(/*this->pss_port,*/ &(this->cyclon),60, "../logging/"),
    data_handler(ip/*, data_port*/, id, 1, &(this->cyclon), this->store, false),
    anti_ent(ip/*, data_port*/, id, &(this->cyclon), this->store, 20)
{
    std::string database_folder = std::string("/home/danielsf97/Desktop/") + this->store->db_name() + "/";
    int res = this->store->init((void*) database_folder.c_str(), id);
    if(res != 0) {
        exit(1);
    }
}

peer::peer(long id, std::string ip, std::string boot_ip/*, int pss_port, int data_port*/,double position, long pss_boot_time, int pss_view_size, int pss_sleep_interval, int pss_gossip_size, bool view_logger_enabled,
        int logging_interval, int anti_entropy_interval, std::string logging_dir, std::string database_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval,
        float reply_chance, bool smart, std::shared_ptr<spdlog::logger> logger, long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at)
    :   id(id), ip(ip)/*, pss_port(pss_port)*/, data_port(data_port), position(position),rep_min(rep_min), rep_max(rep_max), max_age(max_age), local_message(local_message), logger(logger),
        view_logger_enabled(view_logger_enabled), local_interval(local_interval), reply_chance(reply_chance),
//        store(std::make_shared<kv_store_leveldb>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
        store(std::make_shared<kv_store_wiredtiger>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
//        store(std::make_shared<kv_store_memory_v2<std::string>>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
//        store(std::make_shared<kv_store_memory<std::string>>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
        group_c(ip/*, pss_port*/, id, position, rep_min, rep_max, max_age, local_message, local_interval, this->store, logger),
        cyclon(boot_ip.c_str()/*, peer::boot_port*/, ip/*, pss_port*/, id, position,pss_boot_time, pss_view_size, pss_sleep_interval, pss_gossip_size, &(this->group_c)),
        listener(/*"127.0.0.1", pss_port,*/ &(this->cyclon)),
        v_logger(/*pss_port,*/ &(this->cyclon), logging_interval, logging_dir),
        data_handler(ip/*, data_port*/, id, reply_chance, &(this->cyclon), this->store, smart),
        anti_ent(ip/*, data_port*/, id, &(this->cyclon), this->store, anti_entropy_interval)
{
    std::string database_folder = database_dir + this->store->db_name() + "/";
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
    if(view_logger_enabled) {
        this->v_logger_th = std::thread(std::ref(this->v_logger));
    }
    this->data_handler_th = std::thread(std::ref(this->data_handler));
    this->anti_ent_th = std::thread(std::ref(this->anti_ent));
}

void peer::stop(){
    if(view_logger_enabled){
        this->v_logger.stop_thread();
    }
    this->cyclon.stop_thread();
    this->listener.stop_thread();
    this->data_handler.stop_thread();
    this->anti_ent.stop_thread();
    this->store->close();
    this->pss_listener_th.join();
    this->pss_th.join();
    if(view_logger_enabled) {
        this->v_logger_th.join();
    }
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


std::string get_local_ip_address(){
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    sockaddr_in loopback;

    if (sock == -1) {
        throw "ERROR CREATING SOCKET";
    }

    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   // using loopback ip address
    loopback.sin_port = htons(9);                 // using debug port

    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        throw "ERROR COULD NOT CONNECT";
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        throw "ERROR COULD NOT GETSOCKNAME";
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
        throw "ERROR COULD NOT INET_NTOP";
    } else {
        return std::string(buf);
    }
}

int main(int argc, char* argv []){

    if(argc < 4){
        exit(1);
    }

    signal(SIGTERM, term_handler);

    //int pss_port = atoi(argv[1]);
    //int data_port = atoi(argv[2]);
    long id = atol(argv[1]);
    double pos = atof(argv[2]);
    const char* conf_filename = argv[3];
    const char* boot_ip = argv[4];
    std::string ip;

    try{
        ip = get_local_ip_address();
    }catch(const char* e){
        std::cerr << "Error Obtaining IP Address: " << e << std::endl;
        exit(1);
    }

    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    int view_size = main_confs["view_size"].as<int>();
    int gossip_size = main_confs["gossip_size"].as<int>();
    int sleep_interval = main_confs["message_passing_interval_sec"].as<int>();
    bool view_logger_enabled = main_confs["view_logger_enabled"].as<bool>();
    int logging_interval = main_confs["log_interval_sec"].as<int>();
    std::string logging_dir = main_confs["logging_dir"].as<std::string>();
    std::string database_dir = main_confs["database_base_path"].as<std::string>();
    int rep_max = main_confs["rep_max"].as<int>();
    int rep_min = main_confs["rep_min"].as<int>();
    int max_age = main_confs["max_age"].as<int>();
    int local_message = main_confs["local_message"].as<bool>();
    int local_interval = main_confs["local_interval_sec"].as<int>();
    int anti_entropy_interval = main_confs["anti_entropy_interval_sec"].as<int>();
    float reply_chance = main_confs["reply_chance"].as<float>();
    bool smart = main_confs["smart"].as<bool>();
    std::string log_level = main_confs["log_level"].as<std::string>();
    long seen_log_garbage_at = main_confs["seen_log_garbage_at"].as<long>();
    long request_log_garbage_at = main_confs["request_log_garbage_at"].as<long>();
    long anti_entropy_log_garbage_at = main_confs["anti_entropy_log_garbage_at"].as<long>();

    std::shared_ptr<spdlog::logger> logger;
    try
    {
        logger = spdlog::basic_logger_mt("basic_logger", "logs/" + std::string("debug_logs") /*std::to_string(pss_port)*/ + ".txt");
        logger->set_level(spdlog::level::info);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }

    {
        static std::unordered_map<std::string, spdlog::level::level_enum> const levels = {
                {"trace",    spdlog::level::trace},
                {"debug",    spdlog::level::debug},
                {"info",     spdlog::level::info},
                {"warn",     spdlog::level::warn},
                {"err",      spdlog::level::err},
                {"critical", spdlog::level::critical},
                {"off",      spdlog::level::off}
        };
        if (auto it = levels.find(log_level); it != levels.end()) {
            spdlog::set_level(it->second);
        } else {
            spdlog::set_level(spdlog::level::off);
        }
    }
    spdlog::set_pattern( "%v");

    srand (time(NULL));
    int boot_time = rand() % 10 + 2;

    g_peer_impl = std::make_shared<peer>(id,ip,boot_ip/*,pss_port,data_port*/,pos,boot_time,view_size,sleep_interval,gossip_size, view_logger_enabled, logging_interval, anti_entropy_interval, logging_dir,
            database_dir, rep_max, rep_min, max_age, local_message, local_interval, reply_chance, smart, logger, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at);
    g_peer_impl->start();
    g_peer_impl->join();
}
