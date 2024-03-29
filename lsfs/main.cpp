//
// Created by danielsf97 on 2/4/20.
//

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include "lsfs/fuse_lsfs/lsfs_impl.h"
#include "df_util/util.h"

int main(int argc, char *argv[])
{

    if(argc < 7){
        exit(1);
    }

    const char* boot_ip = argv[1];
    long client_id = atol(argv[2]);
    const char* config_filename = argv[3];

    YAML::Node config = YAML::LoadFile(config_filename);
    auto main_confs = config["main_confs"];
    bool use_localhost = main_confs["use_localhost"].as<bool>();
    
    { // Setting Log Level
        
        auto client = main_confs["client"];
        std::string log_level = client["log_level"].as<std::string>();

        static std::unordered_map<std::string, spdlog::level::level_enum> const levels = {
                {"trace",    spdlog::level::trace},
                {"debug",    spdlog::level::debug},
                {"info",     spdlog::level::info},
                {"warn",     spdlog::level::warn},
                {"err",      spdlog::level::err},
                {"critical", spdlog::level::critical},
                {"off",      spdlog::level::off}
        };
        if (auto it = levels.find(log_level); it != levels.end()) {
            spdlog::set_level(it->second);
        } else {
            spdlog::set_level(spdlog::level::off);
        }
        spdlog::set_pattern( "%v");
    }

    std::string ip;

    if(use_localhost){
        ip = "127.0.0.1";
    }else{
        try{
            ip = get_local_ip_address();
        }catch(const char* e){
            std::cerr << "Error Obtaining IP Address: " << e << std::endl;
            exit(1);
        }
    }

    int pss_port = 12357;
    int kv_port = 12358;

    lsfs_impl fs(boot_ip, ip, kv_port, pss_port, client_id, config_filename);
    int status = fs.run(argc - 4, argv + 4, NULL);

    return status;
}