//
// Created by danielsf97 on 3/11/20.
//

#ifndef P2PFS_LSFS_STATE_H
#define P2PFS_LSFS_STATE_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstddef>
#include <time.h>
#include <sys/statvfs.h>
#include <spdlog/logger.h>
#include <list>
#include "df_client/client.h"
#include "df_client/client_reply_handler.h"
#include "metadata/metadata.h"
#include "util.h"
#include "exceptions/custom_exceptions.h"
#include "lsfs/fuse_common/macros.h"    
#include "df_util/util.h"

namespace FileAccess{
    enum FileAccess {CREATED, MODIFIED, ACCESSED};
}

class lsfs_state {

struct directory {
    std::string path;
    std::unique_ptr<metadata> metadata_p;
    struct timespec last_update; //last update to storage
};

public:
    std::recursive_mutex open_files_mutex;
    std::unordered_map<std::string, std::pair<FileAccess::FileAccess ,std::shared_ptr<struct stat>>> open_files;
    
    std::recursive_mutex dir_cache_mutex;
    //LRU
    std::list<directory> dir_cache_list;
    std::unordered_map<std::string, std::list<directory>::iterator> dir_cache_map;
    std::unordered_map<std::string, std::mutex> dir_cache_map_mutex;
    
    std::shared_ptr<client> df_client;
    
    size_t max_parallel_write_size;
    size_t max_parallel_read_size;
    bool benchmark_performance;
    bool maximize_cache;
    int refresh_cache_time;
    int max_directories_in_cache;
    int percentage_of_entries_to_remove_if_cache_full;
    

public:
    lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size, bool benchmark_performance, bool maximize_cache, int refresh_cache_time, int max_directories_in_cache, int percentage_of_entries_to_remove_if_cache_full);
    
    int put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk);
    int put_fixed_size_blocks_from_buffer_limited_paralelization(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk);
    size_t read_fixed_size_blocks_to_buffer(char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk);
    size_t read_fixed_size_blocks_to_buffer_limited_paralelization(char *buf, size_t size, size_t block_size, const char *base_path, size_t current_blk);

    int put_block(const std::string& path, const char* buf, size_t size, bool is_merge = false);
    int put_metadata_as_dir(metadata& met, const std::string& path);
    int put_metadata(metadata& met, const std::string& path);
    int put_metadata_stat(metadata& met, const std::string& path);
    int put_metadata_child(const std::string& path, const std::string& child_path, bool is_create, bool is_dir);
    int put_with_merge_metadata(metadata& met, const std::string& path);
    int delete_file_or_dir(const std::string& path);
    std::unique_ptr<metadata> get_metadata(const std::string& path);
    std::unique_ptr<metadata> get_metadata_stat(const std::string& path);

    void add_to_dir_cache(const std::string& path, metadata met);
    bool check_if_cache_full();
    void refresh_dir_cache();
    void remove_from_dir_cache(const std::string& path);
    void add_child_to_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir);
    int add_child_to_parent_dir(const std::string& path, bool is_dir);
    void remove_child_from_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir);
    int remove_child_from_parent_dir(const std::string& path, bool is_dir);
    bool get_metadata_if_dir_cached(const std::string& path, struct stat* stbuf);
    std::unique_ptr<metadata> get_metadata_if_dir_cached(const std::string& path);
    void clear_all_dir_cache();

    void add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access);
    bool is_file_opened(const std::string& path);
    bool is_dir_cached(const std::string& path);
    bool update_open_file_metadata(const std::string& path, struct stat& stbuf);
    bool update_file_size_if_opened(const std::string& path, size_t size);
    bool update_file_time_if_opened(const std::string& path, const struct timespec ts[2]);
    bool get_metadata_if_file_opened(const std::string& path, struct stat* stbuf);
    int flush_open_file(const std::string& path);
    int flush_and_release_open_file(const std::string& path);
    void reset_dir_cache_add_remove_log(const std::string& path);

    metadata request_metadata(const std::string &base_path, size_t total_s, const kv_store_key_version& last_version);

};

#endif //P2PFS_LSFS_STATE_H
