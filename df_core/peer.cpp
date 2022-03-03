//
// Created by danielsf97 on 10/8/19.
//

#include <argp.h>
#include <iostream>
#include "peer.h"
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include <stdlib.h>
#include <df_data_handlers/data_handler_listener_mt.h>
#include <df_data_handlers/data_handler_listener_st.h>
#include "df_store/kv_store_leveldb.h"

// definition
extern std::string merge_metadata(const std::string&, const std::string&);

std::shared_ptr<peer> g_peer_impl;

peer::peer(long id, std::string ip, std::string boot_ip, int kv_port, int pss_port, int recover_port, double position, long pss_boot_time, int pss_view_size, int pss_sleep_interval, int pss_gossip_size, bool view_logger_enabled,
        int logging_interval, int anti_entropy_interval, std::string logging_dir, std::string database_dir, int rep_max, int rep_min, int max_age, bool local_message, int local_interval,
        float reply_chance, bool smart, bool mt_data_handler, std::shared_ptr<spdlog::logger> logger, long seen_log_garbage_at, long request_log_garbage_at, long anti_entropy_log_garbage_at, bool recover_database)
    :   id(id), ip(ip), data_port(data_port), position(position),rep_min(rep_min), rep_max(rep_max), max_age(max_age), local_message(local_message), logger(logger),
        view_logger_enabled(view_logger_enabled), local_interval(local_interval), reply_chance(reply_chance),
        store(std::make_shared<kv_store_leveldb>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
//        store(std::make_shared<kv_store_memory<std::string>>(merge_metadata, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at)),
        group_c(ip, kv_port, pss_port, recover_port, id, position, rep_min, rep_max, max_age, local_message, local_interval, this->store, logger),
        cyclon(boot_ip.c_str(), ip, kv_port, pss_port, recover_port, id, position,pss_boot_time, pss_view_size, pss_sleep_interval, pss_gossip_size, &(this->group_c)),
        listener(&(this->cyclon)),
        anti_ent(ip, kv_port, recover_port, id, position, &(this->cyclon), &(this->group_c),this->store, anti_entropy_interval, recover_database),
        v_logger(id, &(this->cyclon), &(this->anti_ent), logging_interval, logging_dir)
{
    if(mt_data_handler){
        this->data_handler = std::make_unique<data_handler_listener_mt>(ip, kv_port, id, reply_chance, &(this->cyclon), &(this->group_c), &(this->anti_ent),this->store, smart);
    }else{
        this->data_handler = std::make_unique<data_handler_listener_st>(ip, kv_port, id, reply_chance, &(this->cyclon), &(this->group_c), &(this->anti_ent),this->store, smart);
    }

    std::string database_folder = database_dir + this->store->db_name() + "/";
    int res = this->store->init((void*) database_folder.c_str(), id);
    if(res != 0) {
        exit(1);
    }
}

void peer::print_view() {
    this->cyclon.print_view();
}

void peer::start(int warmup_interval, bool restart_database_after_warmup) {
    try{
        this->pss_th = std::thread (std::ref(this->cyclon));
        this->pss_listener_th = std::thread(std::ref(this->listener));
        if(view_logger_enabled) {
            this->v_logger_th = std::thread(std::ref(this->v_logger));
        }

        sleep(warmup_interval);

        if(restart_database_after_warmup){
            int res = this->store->restart_database();
            if(res != 0){
                std::cerr << "Database Restart Error" << std::endl;
                exit(1);
            }
        }

        this->anti_ent_th = std::thread(std::ref(this->anti_ent));
        this->data_handler_th = std::thread(std::ref(*this->data_handler));
    }catch(const std::system_error& e) {
        std::cerr << "System Error: Not avaliable resources to create peer (peer)!" << std::endl;
        exit(1);
    }
}

void peer::stop(){
    if(view_logger_enabled){
        this->v_logger.stop_thread();
    }
    this->cyclon.stop_thread();
    this->listener.stop_thread();
    this->data_handler->stop_thread();
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

//###########################################################################################
//                                Program Main
//###########################################################################################


static char doc[] =
        "LSFS Peer Node";

/* A description of the arguments we accept. */
static char args_doc[] = "NODE_ID NODE_POS CONFIG_FILE BOOT_IP";

/* The options we understand. */
static struct argp_option options[] = {
        {"no-recover",   'n', 0,     0, "Don't recover database"},
        { 0 }
};

struct arguments
{
    char *args[4];                /* arg1 & arg2 */
    bool no_recover;
};

/* Parse a single option. */
static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
    /* Get the input argument from argp_parse, which we
       know is a pointer to our arguments structure. */
    struct arguments *arguments = (struct arguments*) state->input;

    switch (key)
    {
        case 'n':
            arguments->no_recover = true;
            break;

        case ARGP_KEY_ARG:
            if (state->arg_num >= 4)
                /* Too many arguments. */
                argp_usage (state);

            arguments->args[state->arg_num] = arg;

            break;

        case ARGP_KEY_END:
            if (state->arg_num < 4)
                /* Not enough arguments. */
                argp_usage (state);
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char* argv []){

    struct arguments arguments;

    /* Default values. */
    arguments.no_recover = false;

    /* Parse our arguments; every option seen by parse_opt will
    be reflected in arguments. */
    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    signal(SIGTERM, term_handler);

    long id = atol(arguments.args[0]);
    double pos = atof(arguments.args[1]);
    const char* conf_filename = arguments.args[2];
    const char* boot_ip = arguments.args[3];
    bool recover_database = !arguments.no_recover;

    std::string ip;
    try{
        ip = get_local_ip_address();
    }catch(const char* e){
        std::cerr << "Error Obtaining IP Address: " << e << std::endl;
        exit(1);
    }

    std::cout << ip << std::endl;

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
    bool mt_data_handler = main_confs["mt_data_handler"].as<bool>();
    int warmup_interval = main_confs["warmup_interval"].as<int>();
    bool restart_database_after_warmup = main_confs["restart_database_after_warmup"].as<bool>();

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

    int pss_port = 12365;
    int kv_port = 12366;
    int recover_port = 12367;


    g_peer_impl = std::make_shared<peer>(id,"127.0.0.1",boot_ip, kv_port, pss_port, recover_port, pos, boot_time,view_size,sleep_interval,gossip_size, view_logger_enabled, logging_interval, anti_entropy_interval, logging_dir,
            database_dir, rep_max, rep_min, max_age, local_message, local_interval, reply_chance, smart, mt_data_handler, logger, seen_log_garbage_at, request_log_garbage_at, anti_entropy_log_garbage_at, recover_database);
    g_peer_impl->start(warmup_interval, restart_database_after_warmup);
    g_peer_impl->join();
}
