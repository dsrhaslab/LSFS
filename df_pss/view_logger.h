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

#define LOG(X) std::cout << X << std::endl;


using json = nlohmann::json;


class view_logger {

private:
    std::atomic<bool> running;
    pss* cyclon_ptr;
    int port;
    int logging_interval;
    std::string logging_dir;

public:

    view_logger(int port, pss* pss, int logging_interval, std::string logging_dir)
    {
        this->cyclon_ptr = pss;
        this->running = true;
        this->port = port;
        this->logging_interval = logging_interval;
        this->logging_dir = logging_dir;
    }

    void operator ()(){
        json j = {};
        //j["peer"] = this->port;
        int cycles = 0;
        std::string folder_to_create = this->logging_dir + std::to_string(this->port);
        mkdir(folder_to_create.c_str() ,0777);
        std::string base_filename = this->logging_dir + std::to_string(this->port) + "/" + std::to_string(this->port) + "_";
        std::string extension = ".json";

        time_t now = time(nullptr);
        struct tm *localTime;
        localTime = localtime(&now);
        std::time_t tt = std::mktime(localTime);
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t (tt);
        int seconds_to_wait = (this->logging_interval - (localTime->tm_sec % this->logging_interval)) % this->logging_interval;
        tp += std::chrono::seconds(seconds_to_wait);
        tt = std::chrono::system_clock::to_time_t(tp);
        localtime_r(&tt, localTime);
        std::this_thread::sleep_until(tp);

        do{

            std::ofstream file;
            file.open(base_filename + std::to_string(cycles) +  extension);
            if(file.is_open()){
                j["time"] = std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min) + ":" + std::to_string(localTime->tm_sec);
                j["view"] = this->cyclon_ptr->get_peers_from_view();
                j["group_view"] = this->cyclon_ptr->get_group_view();
                j["group"] = this->cyclon_ptr->get_my_group();
                j["nr_groups"] = this->cyclon_ptr->get_nr_groups();
                j["position"] = this->cyclon_ptr->get_position();
                file << j;
                file.close();
            }

            ++cycles;
            tp += std::chrono::seconds(this->logging_interval);
            time_t tt = std::chrono::system_clock::to_time_t(tp);
            localtime_r(&tt, localTime);
            std::this_thread::sleep_until(tp);
        }while(running);
        LOG("END Logger Thread")
    }

    void stop_thread() {
        this->running = false;
    }
};

#endif //P2PFS_VIEW_LOGGER_H
