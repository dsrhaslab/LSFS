//
// Created by danielsf97 on 3/11/20.
//

#include "lsfs_state.h"

#include <ctime>
#include <utility>
#include <spdlog/spdlog.h>

lsfs_state::lsfs_state(std::shared_ptr<client> df_client, std::shared_ptr<spdlog::logger> logger):
        df_client(std::move(df_client)), logger(std::move(logger))
{}

int lsfs_state::put_block(const std::string& path, const char* buf, size_t size, bool timestamp_version) {
    int return_value;

    try{
        long version;
        if(timestamp_version){
            version = std::time(nullptr);
        }else{
            version = df_client->get_latest_version(path) + 1;
        }
        df_client->put(path, version, buf, size);
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
    }catch(TimeoutException& e){
        e.what();
        errno = EHOSTUNREACH;
        return_value = -1;
    }

    return return_value;
}

int lsfs_state::put_metadata(metadata& met, const std::string& path, bool timestamp_version){
    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);
    return this->put_block(path, metadata_str.data(), metadata_str.size(), timestamp_version);
}

int lsfs_state::put_with_merge_metadata(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);
    int return_value;

    try{
        long version  = df_client->get_latest_version(path) + 1;
        df_client->put_with_merge(path, version, metadata_str.data(), metadata_str.size());
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
    }catch(TimeoutException& e){
        e.what();
        errno = EHOSTUNREACH;
        return_value = -1;
    }

    return return_value;
}

std::unique_ptr<metadata> lsfs_state::get_metadata(const std::string& path){
//    long version = get_version(path);
    std::unique_ptr<metadata> res = nullptr;
    try{
        long version = df_client->get_latest_version(path);
        if(version == -1){
            errno = ENOENT;
        }else{
            //Fazer um get de metadados ao dataflasks
            std::shared_ptr<std::string> data = df_client->get(path, 1, &version);
            res = std::make_unique<metadata>(metadata::deserialize_from_string(*data));
        }
    }catch(TimeoutException& e){
        errno = EHOSTUNREACH; // Not Reachable Host
    }

    return res;
}

void lsfs_state::add_or_refresh_working_directory(const std::string& path, metadata &met) {
    std::scoped_lock<std::recursive_mutex> lk (working_directories_mutex);


    for(auto it = working_directories.begin(); it != working_directories.end(); ++it){
        if(it->first == path){
            it = working_directories.erase(it);
            break;
        }
    }
    if(this->working_directories.size() == this->max_working_directories_cache){
        this->working_directories.erase(this->working_directories.begin());
    }
    this->working_directories.emplace_back(path, std::make_unique<metadata>(met));
}

std::unique_ptr<metadata> lsfs_state::add_child_to_working_dir_and_retreive(const std::string& parent_path, const std::string& child_name, bool is_dir){
    std::unique_ptr<metadata> res(nullptr);
    std::scoped_lock<std::recursive_mutex> lk (working_directories_mutex);
    for(auto & working_directory : working_directories){
        if(working_directory.first == parent_path){
            working_directory.second->add_child(child_name, is_dir);
            res = std::make_unique<metadata>(*working_directory.second);
            this->add_or_refresh_working_directory(parent_path, *res);
        }
    }
    return res;
}


int lsfs_state::add_child_to_parent_dir(const std::string& path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    std::unique_ptr<std::string> child_name = get_child_name(path);
    if(parent_path != nullptr){
        // parent is not root directory
//        long version = get_version(parent_path->c_str());
//        if(version == -1){
//            errno = ENOENT;
//            return -errno;
//        }else{
        //Fazer um get de metadados ao dataflasks

        std::unique_ptr<metadata> met(nullptr);
        met = this->add_child_to_working_dir_and_retreive(*parent_path, *child_name, is_dir);
        if(met == nullptr){
            //parent folder is not a working directory
            try {
                std::unique_ptr<std::string> data = df_client->get(*parent_path /*, &version*/);
                // reconstruir a struct stat com o resultado
                met = std::make_unique<metadata>(metadata::deserialize_from_string(*data));
                // remove last version added/removed child log
                met->reset_add_remove_log();
                // add child path to metadata
                met->add_child(*child_name, is_dir);

                if(*parent_path != "/"){
                    add_or_refresh_working_directory(*parent_path, *met);
                }
            }catch(TimeoutException& e){
                errno = EHOSTUNREACH; // Not Reachable Host
                return -errno;
            }
        }

        if(is_working_directory(*parent_path)){
            // put metadata
            int res = put_metadata(*met, *parent_path, true);
            if(res != 0){
                return -errno;
            }
            //on successful put metadata clear add remove childs log
            this->reset_working_directory_add_remove_log(*parent_path);
        }else{
            int res = put_with_merge_metadata(*met, *parent_path);
            if(res != 0){
                return -errno;
            }
        }

        return 0;
//        }
    }

    // root directory doesn't have a parent
    return 0;
}

void lsfs_state::add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it == open_files.end()) {
        open_files.emplace(path, std::make_pair(access, std::make_shared<struct stat>(stbuf)));
    }
}

bool lsfs_state::is_file_opened(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        return true;
    }
    return false;
}

bool lsfs_state::is_working_directory(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (working_directories_mutex);
    for(auto & working_directory : working_directories){
        if(working_directory.first == path){
            return true;
        }
    }
    return false;
}

//std::shared_ptr<struct stat> get_open_file_if_exists(const char* path){
//    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
//    auto it = open_files.find(path);
//    if(it != open_files.end()) {
//        return it->second.second;
//    }
//
//    return nullptr;
//}

bool lsfs_state::update_open_file_metadata(const std::string& path, struct stat& stbuf){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        std::shared_ptr<struct stat> open_stbuf = it->second.second;

        bool modified = false;
        if(open_stbuf->st_nlink != stbuf.st_nlink){
            open_stbuf->st_nlink = stbuf.st_nlink;
            modified = true;
        }
        if(open_stbuf->st_mode != stbuf.st_mode){
            open_stbuf->st_mode |= stbuf.st_mode; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_size != stbuf.st_size){
            open_stbuf->st_size = stbuf.st_size; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_blksize != stbuf.st_blksize){
            open_stbuf->st_blksize = stbuf.st_blksize; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_blocks != stbuf.st_blocks){
            open_stbuf->st_blocks = stbuf.st_blocks; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_size != stbuf.st_size){
            open_stbuf->st_size = stbuf.st_size; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_atim < stbuf.st_atim){
            open_stbuf->st_atim = stbuf.st_atim; //or of permissions
            modified = true;
        }
        if(open_stbuf->st_mtim < stbuf.st_mtim){
            open_stbuf->st_mtim = stbuf.st_mtim; //or of permission
            modified = true;
        }
        if(open_stbuf->st_ctim < stbuf.st_ctim){
            open_stbuf->st_ctim = stbuf.st_ctim; //or of permissions
            modified = true;
        }

        if(modified){
            if(it->second.first == FileAccess::ACCESSED){
                it->second.first = FileAccess::MODIFIED;
            }
        }

        return true;
    }

    return false;
}

bool lsfs_state::update_file_size_if_opened(const std::string& path, size_t size){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    struct stat stbuf;
    bool file_is_opened = get_metadata_if_file_opened(path, &stbuf);

    if(file_is_opened){
        int nr_b_blks = size / BLK_SIZE;
        size_t off_blk = size % BLK_SIZE;

        stbuf.st_size = size;
        stbuf.st_blocks = (off_blk != 0) ? (nr_b_blks + 1) : nr_b_blks;
        clock_gettime(CLOCK_REALTIME, &(stbuf.st_ctim));
        stbuf.st_mtim = stbuf.st_ctim;

        return update_open_file_metadata(path, stbuf);
    }else{
        return false;
    }
}

bool lsfs_state::update_file_time_if_opened(const std::string& path, const struct timespec ts[2]){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    struct stat stbuf;
    bool file_is_opened = get_metadata_if_file_opened(path, &stbuf);

    if(file_is_opened){
        stbuf.st_atim = ts[0];
        stbuf.st_mtim = ts[1];

        return update_open_file_metadata(path, stbuf);
    }else{
        return false;
    }
}

bool lsfs_state::get_metadata_if_file_opened(const std::string& path, struct stat* stbuf){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        memcpy(stbuf, it->second.second.get(), sizeof(struct stat));
        return true;
    }

    return false;
}

bool lsfs_state::get_metadata_if_dir_opened(const std::string& path, struct stat* stbuf){
    std::scoped_lock<std::recursive_mutex> lk (working_directories_mutex);
    for(auto & working_directorie : working_directories){
        if(working_directorie.first == path){
            memcpy(stbuf, &working_directorie.second->stbuf, sizeof(struct stat));
            return true;
        }
    }

    return false;
}

int lsfs_state::flush_open_file(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()){
        if(it->second.first == FileAccess::CREATED || it->second.first == FileAccess::MODIFIED){
            // Se o ficheiro foi modificado é necessário atualizar os metadados do mesmo

            // create metadata object
            metadata to_send(*(it->second.second));
            // serialize metadata object
            int res = put_metadata(to_send, path, true);
            if(res != 0){
                return res;
            }

            if(it->second.first == FileAccess::CREATED){
                // se o ficheiro foi criado pela primeira vez é necessário
                // atualizar o pai com o path do mesmo
                res = add_child_to_parent_dir(path, false);
                if(res != 0){
                    return res;
                }
            }

            it->second.first = FileAccess::ACCESSED;
        }
        return 0;
    }

    errno = ENOENT;
    return -errno;
}

int lsfs_state::flush_and_release_open_file(const std::string& path) {
    std::scoped_lock<std::recursive_mutex> lk(open_files_mutex);
    int res = flush_open_file(path);
    if(res != 0){
        return res;
    }
    auto it = open_files.find(path);
    if (it != open_files.end()) {
        open_files.erase(it);
    }
    return 0;
}

void lsfs_state::clear_working_directories_cache() {
    std::scoped_lock<std::recursive_mutex> lk(working_directories_mutex);
    this->working_directories.clear();
}

void lsfs_state::reset_working_directory_add_remove_log(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (working_directories_mutex);
    for(auto & working_directory : working_directories){
        if(working_directory.first == path){
            working_directory.second->added_childs.clear();
            working_directory.second->removed_childs.clear();
            break;
        }
    }
}
