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

int put_metadata(metadata& met, const char* path){
    int return_value;

    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);

    long version = increment_version_and_get(path);
    try{
        df_client->put(path, version, metadata_str.data(), metadata_str.size());
        return_value = 0;
    }catch(EmptyViewException& e){
        // empty view -> nothing to do
        e.what();
        errno = EAGAIN; //resource unavailable
        return_value = -1;
    }catch(ConcurrentWritesSameKeyException& e){
        e.what();
        errno = EPERM; //operation not permitted
        return_value = -1;
    }

    return return_value;
}

std::unique_ptr<metadata> get_metadata(const char* path){
    long version = get_version(path);
    std::unique_ptr<metadata> res = nullptr;
    if(version == -1){
        errno = ENOENT;
    }else{
        //Fazer um get de metadados ao dataflasks
        std::shared_ptr<std::string> data = df_client->get(1, path, version);

        if (data == nullptr){
            errno = EHOSTUNREACH; // Not Reachable Host
        }

        // reconstruir a struct stat com o resultado
        res = std::make_unique<metadata>(metadata::deserialize_from_string(*data));
    }

    return res;
}

int add_child_to_parent_dir(const char *path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    if(parent_path != nullptr){
        // parent is not root directory
        long version = get_version(parent_path->c_str());
        if(version == -1){
            errno = ENOENT;
            return -errno;
        }else{
            //Fazer um get de metadados ao dataflasks
            std::shared_ptr<std::string> data = df_client->get(1, parent_path->c_str(), version);

            if (data == nullptr){
                errno = EHOSTUNREACH; // Not Reachable Host
                return -errno;
            }

            // reconstruir a struct stat com o resultado
            metadata met = metadata::deserialize_from_string(*data);
            // increase parent hard links if its a directory
            if(is_dir){
                met.stbuf.st_nlink++;
            }
            // add child path to metadata
            met.add_child(std::string(path));
            // put metadata
            int res = put_metadata(met, parent_path->c_str());
            if(res != 0){
                return -errno;
            }
            return 0;
        }
    }

    //TODO handle root directory case
    return 0;
}