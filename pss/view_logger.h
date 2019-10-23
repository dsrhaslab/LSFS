//
// Created by danielsf97 on 10/18/19.
//

#ifndef P2PFS_VIEW_LOGGER_H
#define P2PFS_VIEW_LOGGER_H

#include <atomic>
#include "pss.h"
#include <thread>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

using json = nlohmann::json;


class view_logger {

private:
    std::atomic<bool> running;
    pss* cyclon_ptr;
    int port;


public:

    view_logger(int port, pss* pss)
    {
        this->cyclon_ptr = pss;
        this->running = true;
        this->port = port;
    }

    void operator ()(){
        json j = {};
        //j["peer"] = this->port;
        int cycles = 0;
        std::string folder_to_create = "../logging/" + std::to_string(this->port);
        mkdir(folder_to_create.c_str() ,0777);
        std::string base_filename = "../logging/" + std::to_string(this->port) + "/" + std::to_string(this->port) + "_";
        std::string extension = ".json";

        time_t now = time(nullptr) + 1*60;
        struct tm *localTime;
        localTime = localtime(&now);
        localTime->tm_sec = 0;
        std::time_t tt = std::mktime(localTime);
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t (tt);
        std::this_thread::sleep_until(tp);

        do{

            std::ofstream file;
            file.open(base_filename + std::to_string(cycles) +  extension);
            if(file.is_open()){
                j["time"] = std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min) + ":" + std::to_string(localTime->tm_sec);
                j["view"] = this->cyclon_ptr->get_peers_from_view();
                file << j;
                file.close();
            }

            ++cycles;
            tp += std::chrono::minutes(1);
            time_t tt = std::chrono::system_clock::to_time_t(tp);
            localtime_r(&tt, localTime);
            std::this_thread::sleep_until(tp);
        }while(running);
    }

    void stop_thread() {
        this->running = false;
    }
};

#endif //P2PFS_VIEW_LOGGER_H
