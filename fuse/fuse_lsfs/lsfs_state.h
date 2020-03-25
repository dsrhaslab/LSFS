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

/* ================ MACROS ==================*/

#define BLK_SIZE (size_t) 4096

/* ==========================================*/

namespace FileAccess{
    enum FileAccess {CREATED, MODIFIED, ACCESSED};
}

class lsfs_state {

public:
    static const int max_working_directories_cache = 5;

public:
    std::mutex version_tracker_mutex;
    std::unordered_map<std::string, long> version_tracker; // path => version
    std::recursive_mutex open_files_mutex;
    std::unordered_map<std::string, std::pair<FileAccess::FileAccess ,std::shared_ptr<struct stat>>> open_files; // path => version
    std::recursive_mutex working_directories_mutex;
    std::vector<std::pair<std::string, std::unique_ptr<metadata>>> working_directories;
    std::shared_ptr<client> df_client;
    std::shared_ptr<spdlog::logger> logger;

public:
    lsfs_state(std::shared_ptr<client> df_client, std::shared_ptr<spdlog::logger> logger);
    long increment_version_and_get(std::string path);
    long get_version(std::string path);
    int open_and_read_size(const char *path, size_t* size);
    int add_child_to_parent_dir(const char *path, bool is_dir);
    std::unique_ptr<metadata> add_child_to_working_dir_and_retreive(const char* parent_path, const char* child_name, bool is_dir);
    int put_block(const char* path, const char* buf, size_t size, bool timestamp_version = false);
    int put_metadata(metadata& met, const char* path, bool timestamp_version = false);
    int put_with_merge_metadata(metadata& met, const char* path);
    std::unique_ptr<metadata> get_metadata(const char* path);
    void add_open_file(const char* path, struct stat& stbuf, FileAccess::FileAccess access);
    bool is_file_opened(const char* path);
    bool is_working_directory(const char* path);
    bool update_open_file_metadata(const char* path, struct stat& stbuf);
    bool update_file_size_if_opened(const char* path, size_t size);
    bool update_file_time_if_opened(const char* path, const struct timespec ts[2]);
    bool get_metadata_if_file_opened(const char* path, struct stat* stbuf);
    bool get_metadata_if_dir_opened(const char* path, struct stat* stbuf);
    int flush_open_file(const char* path);
    int flush_and_release_open_file(const char* path);
    void add_or_refresh_working_directory(const char* path, metadata& met);
    void clear_working_directories_cache();
    void reset_working_directory_add_remove_log(const char* path);
};


#endif //P2PFS_LSFS_STATE_H
