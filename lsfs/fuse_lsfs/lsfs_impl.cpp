//
// Created by danielsf97 on 2/9/20.
//

#include <spdlog/sinks/basic_file_sink.h>
#include "lsfs_impl.h"
#include "yaml-cpp/yaml.h"


std::unique_ptr<lsfs_state> state;
std::shared_ptr<client> df_client;

lsfs_impl::lsfs_impl(const std::string& boot_ip, const std::string& ip, int kv_port, int pss_port, long client_id, const std::string& config_filename){

    df_client = std::make_shared<client>(boot_ip, ip, kv_port, pss_port, client_id, config_filename);

    YAML::Node config = YAML::LoadFile(config_filename);
    auto main_confs = config["main_confs"];
    auto client = main_confs["client"];
    auto max_parallel_write_size = client["limit_write_paralelization_to"].as<std::string>();
    auto max_parallel_read_size = client["limit_read_paralelization_to"].as<std::string>();
    auto cache = client["cache"];
    bool use_cache = cache["use_cache"].as<bool>();
    int refresh_cache_time = cache["refresh_cache_time"].as<int>();
    int max_directories_in_cache = cache["max_directories_in_cache"].as<int>();
    int direct_io = client["direct_io"].as<int>();
    int max_nr_requests_timeout = client["max_nr_requests_timeouts"].as<int>();
    int cache_max_nr_requests_timeout = cache["max_nr_requests_timeout"].as<int>();
    

    try
    {
        size_t max_parallel_write_size_bytes = convert_string_size_to_num_bytes(max_parallel_write_size);
        size_t max_parallel_read_size_bytes = convert_string_size_to_num_bytes(max_parallel_read_size);
        state = std::make_unique<lsfs_state>(df_client, max_parallel_read_size_bytes, max_parallel_write_size_bytes, use_cache, refresh_cache_time, max_directories_in_cache, max_nr_requests_timeout, cache_max_nr_requests_timeout, direct_io);


        if(use_cache){
            cache_maintainer_thr = std::thread([](){
                while(true){
                    state->refresh_dir_cache();
                    std::this_thread::sleep_for(std::chrono::milliseconds(state->refresh_cache_time)); 
                }
            });
        }

    }catch (const char* msg){
        std::cerr << msg << std::endl;
        exit(1);
    }
}

