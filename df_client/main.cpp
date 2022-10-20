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
    if(argc < 2){
        exit(1);
    }

    
    std::string ip = "";
    std::string boot_ip = argv[1];
    char *p;
    std::string config_path = argv[2];
    int pss_port = 12377;
    int kv_port = 12378;
    long id = 9999;
    try{
            ip = get_local_ip_address();
    }catch(const char* e){
        std::cerr << "Error Obtaining IP Address: " << e << std::endl;
        exit(1);
    }

    client cli = client(boot_ip, ip, kv_port, pss_port, id, config_path);

    std::string file = "";

    for(int i = 0; i < BLK_SIZE; i++){
        file = file +"a";
    }

    std::string key;
    //Populate for read
    for(int i = 0; i < 250; i++){
        key = "file/ola/ll:" + std::to_string(i);
        cli.dummy_msg(file, key, true);
    }
    bool is_write = false;
    // bool is_write = true;

    double stop_time = 600.0;

    long total_send = 0;
    time_t start = time(NULL);
    bool terminate = true;
    int i = 0;
    while (terminate) {
        time_t end = time(NULL);
        double elapsed = difftime(end, start);
        if(i >= 250);
            i = 0;

        key = "file/ola/ll:" +  std::to_string(i);
        cli.dummy_msg(file, key, is_write);
        total_send += BLK_SIZE;

        if (elapsed >= stop_time /* seconds */)
            terminate = false;
        i++;
    }
    std::cout << "Sent:" << total_send << "k in " << stop_time << "s" << std::endl;
    double mb = total_send/1000000;
    double mbs = mb/stop_time;
    std::cout << mbs << "mb/s" << std::endl;



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

     return 0;
}