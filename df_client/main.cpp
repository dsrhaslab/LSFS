//
// Created by danielsf97 on 1/28/20.
//

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <time.h>
#include "yaml-cpp/yaml.h"
#include <fstream>
#include "df_loadbalancing/load_balancer_listener.h"
#include "client.h"
#include "df_loadbalancing/dynamic_load_balancer.h"
#include "df_util/util.h"


int main(int argc, char **argv) {
    // if(argc < 4){
    //     exit(1);
    // }

    
    // std::string ip = "";
    // std::string boot_ip = argv[1];
    // char *p;
    // int clock = strtol(argv[2], NULL, 10);
    // std::string config_path = argv[3];
    // int pss_port = 12377;
    // int kv_port = 12378;
    // long id = 9999;
    // try{
    //         ip = get_local_ip_address();
    // }catch(const char* e){
    //     std::cerr << "Error Obtaining IP Address: " << e << std::endl;
    //     exit(1);
    // }

    // client cli = client(boot_ip, ip, kv_port, pss_port, id, config_path);

    // std::ifstream infile(argv[4]);

    // std::vector<std::string> peers;
    // std::string peer_ip;
    // while (infile >> peer_ip)
    // {
    //     peers.push_back(peer_ip);
    // }

    // std::map<long, long> version ({{id, clock}});
    // kv_store_version vv;
    // vv.vv = version;
    // vv.client_id = id;

    // cli.del_db("delete_all_db", vv, peers);

    // cli.stop();   

    // return 0;
}