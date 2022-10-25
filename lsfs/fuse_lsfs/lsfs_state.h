#ifndef P2PFS_LSFS_STATE_H
#define P2PFS_LSFS_STATE_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <cstddef>
#include <time.h>
#include <list>
#include <ctime>
#include <utility>

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

public:
    
    struct directory {
        std::string path;
        std::unique_ptr<metadata> metadata_p;
        struct timespec last_update; //last update to storage
    };

    struct file {
        FileAccess::FileAccess access_t;
        std::shared_ptr<struct stat> stat;
        kv_store_version version;
    };

    // Open Files temporary structure
    std::recursive_mutex open_files_mutex;
    std::unordered_map<std::string, file> open_files;
    
    // Global LRU mutex
    std::recursive_mutex dir_cache_mutex;
    
    // LRU (Least Recently Used) structure.
    std::list<std::shared_ptr<directory>> dir_cache_list;
    std::unordered_map<std::string, std::list<std::shared_ptr<directory>>::iterator> dir_cache_map;
    std::unordered_map<std::string, std::unique_ptr<std::mutex>> dir_cache_map_mutex;
    
    std::shared_ptr<client> df_client;
    
    size_t max_parallel_write_size;
    size_t max_parallel_read_size;
    bool use_cache;
    int refresh_cache_time;
    int max_directories_in_cache;

    int max_nr_requests_timeout;
    int cache_max_nr_requests_timeout;

    int direct_io;
    

public:
    lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size, bool use_cache, int refresh_cache_time, int max_directories_in_cache, int max_nr_requests_timeout, int cache_max_nr_requests_timeout, int direct_io);
    
    int put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk, const kv_store_version& version);
    int put_fixed_size_blocks_from_buffer_limited_paralelization(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk, const kv_store_version& version);
    size_t read_fixed_size_blocks_to_buffer(char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk);
    size_t read_fixed_size_blocks_to_buffer_limited_paralelization(char *buf, size_t size, size_t block_size, const char *base_path, size_t current_blk);

    int put_block(const std::string& path, const char* buf, size_t size, const kv_store_version& version, FileType::FileType f_type);
    int put_dir_metadata(metadata& met, const std::string& path);
    int put_file_metadata(metadata& met, const std::string& path);
    int put_file_metadata(metadata& met, const std::string& path, const kv_store_version& version);
    int put_dir_metadata_child(const std::string& path, const std::string& child_path, bool is_create, bool is_dir);
    int put_with_merge_metadata(metadata& met, const std::string& path);
    int delete_file(const std::string& path);
    int delete_dir(const std::string& path);
    int delete_(const std::string& path, FileType::FileType f_type);
    std::unique_ptr<metadata> get_dir_metadata(const std::string& path);
    std::unique_ptr<metadata> get_dir_metadata_for_cache(const std::string& path, client_reply_handler::Response* response);
    std::unique_ptr<metadata_attr> get_metadata_stat(const std::string& path);
    std::unique_ptr<metadata_attr> get_metadata_stat(const std::string& path, kv_store_version* last_version);

    void add_to_dir_cache(const std::string& path, metadata met);
    bool check_if_cache_full();
    void refresh_dir_cache();
    void remove_from_dir_cache(const std::string& path);
    void add_child_to_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir);
    int add_child_to_parent_dir(const std::string& path, bool is_dir);
    void remove_child_from_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir);
    int remove_child_from_parent_dir(const std::string& path, bool is_dir);
    bool get_metadata_if_dir_cached(const std::string& path, struct stat* stbuf);
    std::shared_ptr<directory> get_metadata_if_dir_cached(const std::string& path);
    bool is_dir_childs_empty(const std::string& path, bool* dir_cached);
    void clear_all_dir_cache();
    std::string print_cache();

    void add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access);
    void add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access, kv_store_version n_version);
    bool is_file_opened(const std::string& path);
    bool is_dir_cached(const std::string& path);
    bool update_open_file_metadata(const std::string& path, struct stat& stbuf);
    bool update_file_size_if_opened(const std::string& path, size_t size);
    bool update_file_time_if_opened(const std::string& path, const struct timespec ts[2]);
    bool get_metadata_if_file_opened(const std::string& path, struct stat* stbuf);
    bool get_version_if_file_opened(const std::string& path, kv_store_version* version);
    int flush_open_file(const std::string& path);
    int flush_and_release_open_file(const std::string& path);

    std::unique_ptr<metadata> request_metadata(const std::string &base_path, size_t total_s, const kv_store_version& last_version,
         client_reply_handler::Response* response, bool for_cache = false);

};

#endif //P2PFS_LSFS_STATE_H
