//
// Created by danielsf97 on 2/4/20.
//

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include "fuse/fuse_lsfs/lsfs_impl.h"

// sudo ./lsfs_exe /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder/ /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder2/

int main(int argc, char *argv[])
{

    { // Setting Log Level
        YAML::Node config = YAML::LoadFile("../scripts/conf.yaml");
        auto main_confs = config["main_confs"];
        std::string log_level = main_confs["log_level"].as<std::string>();

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

    lsfs_impl fs;
    int status = fs.run(argc, argv, NULL);

    return status;
}