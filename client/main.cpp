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
#include <loadbalancing/load_balancer_listener.h>
#include "client.h"
#include "loadbalancing/dynamic_load_balancer.h"
#include "core/peer.h"

int main(int argc, char **argv) {
    if(argc < 3){
        exit(1);
    }

    int lb_port = atoi(argv[1]);
    int kv_port = atoi(argv[2]);
    std::string ip = "127.0.0.1";
    long id = atol(argv[3]);
    const char* conf_filename = argv[4];


    YAML::Node config = YAML::LoadFile(conf_filename);
    auto main_confs = config["main_confs"];
    int nr_puts_required = main_confs["nr_puts_required"].as<int>();
    std::cout << 1 << std::endl;
    long wait_timeout = main_confs["client_wait_timeout"].as<long>();
    std::cout << 2 << std::endl;
    long lb_interval = main_confs["lb_interval"].as<long>();
    std::cout << 3 << std::endl;

    dynamic_load_balancer lb = dynamic_load_balancer(peer::boot_ip, peer::boot_port, ip, lb_port, lb_interval);
    load_balancer_listener lb_listener = load_balancer_listener(&lb, ip, lb_port);
    std::thread lb_listener_th = std::thread (std::ref(lb_listener));
    std::thread lb_th = std::thread (std::ref(lb));

    client cli = client(ip, id, kv_port, &lb, nr_puts_required, wait_timeout);
//
//    cli.put(1,1,"KV ALMOST FINISHED");
//    cli.put(2,1,"KV ALMOST FINISHED");
//    cli.put(3,1,"KV ALMOST FINISHED");
//    cli.put(4,1,"KV ALMOST FINISHED");
    std::shared_ptr<const char []> data = cli.get(id,1,1);
    std::cout << data.get() << std::endl;

    lb.stop();
    std::cout << "stopped load balancer" << std::endl;
    lb_listener.stop();
    std::cout << "stopped load balancer listener" << std::endl;
    lb_th.join();
    std::cout << "stopped load balancer thread" << std::endl;
    lb_listener_th.join();
    std::cout << "stopped load balancer listener thread" << std::endl;
    cli.stop();
    std::cout << "stopped client" << std::endl;

    return 0;
}