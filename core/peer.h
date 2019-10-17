//
// Created by danielsf97 on 10/8/19.
//

#ifndef DATAFLASKSCPP_PEER_H
#define DATAFLASKSCPP_PEER_H

#include "../pss/pss.h"
#include "../pss/pss_listener.h"
#include <memory>
#include <thread>

class peer {
public:
    inline const static char* boot_ip = "127.0.0.1";
    inline const static int boot_port = 12345;

private:
    long id;
    int port;
    std::string ip;
    double position;
    pss cyclon;                     //thread functor que envia msgs
    pss_listener listener;
    std::thread pss_th;
    std::thread pss_listener_th;

    //PSS
    long pss_sleep_interval;
    long pss_boot_time;
    int pss_view_size;

public:
    peer(long id, std::string ip, int port, double position);
    peer(long id, std::string ip, int port, double position, long pss_sleep_interval, long pss_boot_time, int pss_view_size);
    void print_view();
    void start();

    void stop();
};


#endif //DATAFLASKSCPP_PEER_H
