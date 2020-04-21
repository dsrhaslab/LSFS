//
// Created by danielsf97 on 2/4/20.
//

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>
#include "fuse/fuse_lsfs/lsfs_impl.h"

// sudo ./lsfs_exe /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder/ /home/danielsf97/Desktop/Tese/P2P-Filesystem/Teste/InnerFolder2/

std::string get_local_ip_address(){
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    sockaddr_in loopback;

    if (sock == -1) {
        throw "ERROR CREATING SOCKET";
    }

    std::memset(&loopback, 0, sizeof(loopback));
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = INADDR_LOOPBACK;   // using loopback ip address
    loopback.sin_port = htons(9);                 // using debug port

    if (connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
        close(sock);
        throw "ERROR COULD NOT CONNECT";
    }

    socklen_t addrlen = sizeof(loopback);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
        close(sock);
        throw "ERROR COULD NOT GETSOCKNAME";
    }

    close(sock);

    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
        throw "ERROR COULD NOT INET_NTOP";
    } else {
        return std::string(buf);
    }
}

int main(int argc, char *argv[])
{

    if(argc < 6){
        exit(1);
    }

    const char* boot_ip = argv[1];
    long client_id = atol(argv[2]);
    const char* config_filename = argv[3];

    { // Setting Log Level
        YAML::Node config = YAML::LoadFile(config_filename);
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

    std::string ip;
    try{
        ip = get_local_ip_address();
    }catch(const char* e){
        std::cerr << "Error Obtaining IP Address: " << e << std::endl;
        exit(1);
    }

    lsfs_impl fs(boot_ip, ip, client_id, config_filename);
    int status = fs.run(argc - 3, argv + 3, NULL);

    return status;
}