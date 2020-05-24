//
// Created by danielsf97 on 2/9/20.
//

#include <spdlog/sinks/basic_file_sink.h>
#include "lsfs_impl.h"
#include "yaml-cpp/yaml.h"


std::unique_ptr<lsfs_state> state;
std::shared_ptr<client> df_client;
std::shared_ptr<spdlog::logger> logger;

lsfs_impl::lsfs_impl(const std::string& boot_ip, const std::string& ip, long client_id, const std::string& config_filename){

    df_client = std::make_shared<client>(boot_ip, ip, client_id, config_filename);

    YAML::Node config = YAML::LoadFile(config_filename);
    auto main_confs = config["main_confs"];
    auto max_parallel_write_size = main_confs["limit_write_paralelization_to"].as<std::string>();
    auto max_parallel_read_size = main_confs["limit_read_paralelization_to"].as<std::string>();

    try
    {
        size_t max_parallel_write_size_bytes = convert_string_size_to_num_bytes(max_parallel_write_size);
        size_t max_parallel_read_size_bytes = convert_string_size_to_num_bytes(max_parallel_read_size);
        state = std::make_unique<lsfs_state>(df_client, max_parallel_read_size_bytes, max_parallel_write_size_bytes);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }catch (const char* msg){
        std::cerr << msg << std::endl;
        exit(1);
    }
}

