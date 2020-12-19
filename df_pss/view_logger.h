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
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include "df_data_handlers/anti_entropy.h"
#define LOG(X) std::cout << X << std::endl;

using json = nlohmann::json;

class view_logger {

private:
    std::atomic<bool> running;
    pss* cyclon_ptr;
    anti_entropy* anti_ent_ptr;
    long id;
    int logging_interval;
    std::string logging_dir;

public:

    view_logger(long id, pss* pss, anti_entropy* anti_ent, int logging_interval, std::string logging_dir)
    {
        this->cyclon_ptr = pss;
        this->anti_ent_ptr = anti_ent;
        this->running = true;
        this->id = id;
        this->logging_interval = logging_interval;
        this->logging_dir = logging_dir;
    }

    void operator ()(){
        int cycles = 0;

        std::filesystem::create_directories(this->logging_dir);

        std::string filename = this->logging_dir + "peer" + std::to_string(this->id) + ".txt";

        std::shared_ptr<spdlog::logger> logger;
        try
        {
            logger = spdlog::basic_logger_mt("view_logger", filename, true);
            logger->set_level(spdlog::level::info);
            logger->set_pattern("%v");
            spdlog::flush_every(std::chrono::seconds(60));
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            std::cout << "Log init failed: " << ex.what() << std::endl;
        }

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

            std::string timed_json = "{";
            timed_json += "\"group\":" + std::to_string(this->cyclon_ptr->get_my_group()) + ",";
            timed_json += "\"group_view\":[";
            int i = 0;
            for(auto& peer_id: this->cyclon_ptr->get_group_view()){
                if (i != 0) timed_json += ",";
                timed_json += std::to_string(peer_id);
                i++;
            }
            timed_json += "],";
            timed_json += "\"recovered\":" + std::to_string(this->anti_ent_ptr->has_recovered()) + ",";
            timed_json += "\"nr_groups\":" + std::to_string(this->cyclon_ptr->get_nr_groups()) + ",";
            timed_json += "\"position\":" + std::to_string(this->cyclon_ptr->get_position()) + ",";
            timed_json += "\"time\":\"" + std::to_string(localTime->tm_hour) + ":" + std::to_string(localTime->tm_min) + ":" + std::to_string(localTime->tm_sec) + "\",";
            timed_json += "\"view\":[";
            i = 0;
            for(auto& peer_id/*port*/: this->cyclon_ptr->get_peers_from_view()){
                if (i != 0) timed_json += ",";
                timed_json += std::to_string(peer_id);
                i++;
            }
            timed_json += "]}";
            logger->info(timed_json);

            ++cycles;
            tp += std::chrono::seconds(this->logging_interval);
            time_t tt = std::chrono::system_clock::to_time_t(tp);
            localtime_r(&tt, localTime);
            std::this_thread::sleep_until(tp);
        }while(running);

        spdlog::shutdown();
        LOG("END Logger Thread")
    }

    void stop_thread() {
        this->running = false;
    }
};

#endif //P2PFS_VIEW_LOGGER_H
