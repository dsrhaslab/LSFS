//
// Created by danielsf97 on 2/9/20.
//

#include <spdlog/sinks/basic_file_sink.h>
#include "lsfs_impl.h"

std::unique_ptr<client> df_client;

std::shared_ptr<spdlog::logger> logger;

std::mutex version_tracker_mutex;

std::unordered_map<std::string, long> version_tracker; // path => version

lsfs_impl::lsfs_impl(){

    df_client = std::make_unique<client>("127.0.0.1", 0, 1235 /*kv_port*/, 1234 /*lb_port*/);

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

long increment_version_and_get(std::string path){
    std::scoped_lock<std::mutex> lk (version_tracker_mutex);
    auto it = version_tracker.find(path);
    if(it == version_tracker.end()){
        //não existe o path
        version_tracker.emplace(std::move(path), 1);
        return 1;
    }else{
        it->second += 1;
        return it->second;
    }
}

long get_version(std::string path){
    std::scoped_lock<std::mutex> lk (version_tracker_mutex);
    auto it = version_tracker.find(path);
    if(it == version_tracker.end()){
        //não existe o path
        return -1;
    }else{
        return it->second;
    }
}

int open_and_read_size(
        const char *path,
        size_t* size
)
{
    //open file descriptor
    int fh = open(path, O_RDONLY);

    if (fh == -1)
        return -errno;

    //read previously written size of data in kv
    char buf [20];
    const int result_read = pread(fh, buf, 20, 0);

    //close file descriptor
    if(close(fh) != 0) return -errno;

    if (result_read == -1)
        return -errno;

    *size = atol(buf);

    return 0;
}