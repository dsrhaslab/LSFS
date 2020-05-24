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
#include <sys/statvfs.h>
#include <spdlog/logger.h>
#include "df_client/client.h"
#include "metadata.h"
#include "util.h"
#include "exceptions/custom_exceptions.h"
#include "fuse/fuse_common/macros.h"

namespace FileAccess{
    enum FileAccess {CREATED, MODIFIED, ACCESSED};
}

class lsfs_state {

public:
    static const int max_working_directories_cache = 16;

public:
    std::mutex version_tracker_mutex;
    std::unordered_map<std::string, long> version_tracker; // path => version
    std::recursive_mutex open_files_mutex;
    std::unordered_map<std::string, std::pair<FileAccess::FileAccess ,std::shared_ptr<struct stat>>> open_files; // path => version
    std::recursive_mutex working_directories_mutex;
    std::vector<std::pair<std::string, std::unique_ptr<metadata>>> working_directories;
    std::shared_ptr<client> df_client;
    size_t max_parallel_write_size;
    size_t max_parallel_read_size;

public:
    lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size);
    int add_child_to_parent_dir(const std::string& path, bool is_dir);
    std::unique_ptr<metadata> add_child_to_working_dir_and_retreive(const std::string& parent_path, const std::string& child_name, bool is_dir);
    int put_block(const std::string& path, const char* buf, size_t size, bool timestamp_version = false);
    int put_metadata(metadata& met, const std::string& path, bool timestamp_version = false);
    int put_with_merge_metadata(metadata& met, const std::string& path);
    std::unique_ptr<metadata> get_metadata(const std::string& path);
    void add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access);
    bool is_file_opened(const std::string& path);
    bool is_working_directory(const std::string& path);
    bool update_open_file_metadata(const std::string& path, struct stat& stbuf);
    bool update_file_size_if_opened(const std::string& path, size_t size);
    bool update_file_time_if_opened(const std::string& path, const struct timespec ts[2]);
    bool get_metadata_if_file_opened(const std::string& path, struct stat* stbuf);
    bool get_metadata_if_dir_opened(const std::string& path, struct stat* stbuf);
    int flush_open_file(const std::string& path);
    int flush_and_release_open_file(const std::string& path);
    void add_or_refresh_working_directory(const std::string& path, metadata& met);
    void clear_working_directories_cache();
    void reset_working_directory_add_remove_log(const std::string& path);
    int put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk,  bool timestamp_version = false );
    int put_fixed_size_blocks_from_buffer_limited_paralelization(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk,  bool timestamp_version = false );
    size_t read_fixed_size_blocks_to_buffer(char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk);
    size_t read_fixed_size_blocks_to_buffer_limited_paralelization(char *buf, size_t size, size_t block_size, const char *base_path, size_t current_blk);
};

#endif //P2PFS_LSFS_STATE_H
