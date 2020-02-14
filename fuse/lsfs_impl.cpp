//
// Created by danielsf97 on 2/9/20.
//

#include <spdlog/sinks/basic_file_sink.h>
#include "lsfs_impl.h"

std::unique_ptr<client> df_client;

std::shared_ptr<spdlog::logger> logger;

std::mutex fhs_mutex;

std::unordered_map<int, std::string> file_handlers;

lsfs_impl::lsfs_impl(){

    df_client = std::make_unique<client>("127.0.0.1", 0, 12551 /*kv_port*/, 12550 /*lb_port*/);

    try
    {
        logger =  spdlog::basic_logger_mt("lsfs_logger", "lsfs_logger.txt");
        logger->set_level(spdlog::level::info);
        logger->set_pattern("%v");
//            spdlog::flush_every(std::chrono::seconds(5));
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }
}