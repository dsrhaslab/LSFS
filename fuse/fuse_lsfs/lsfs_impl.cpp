//
// Created by danielsf97 on 2/9/20.
//

#include <spdlog/sinks/basic_file_sink.h>
#include "lsfs_impl.h"

std::unique_ptr<lsfs_state> state;
std::shared_ptr<client> df_client;
std::shared_ptr<spdlog::logger> logger;

lsfs_impl::lsfs_impl(){

    df_client = std::make_shared<client>("127.0.0.1", 0, 1235 /*kv_port*/, 1234 /*lb_port*/, "scripts/conf.yaml");

    try
    {
        logger = spdlog::basic_logger_mt("lsfs_logger", "lsfs_logger.txt");
        logger->set_level(spdlog::level::info);
        logger->set_pattern("%v");
//            spdlog::flush_every(std::chrono::seconds(5));

        state = std::make_unique<lsfs_state>(df_client, logger);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }
}

