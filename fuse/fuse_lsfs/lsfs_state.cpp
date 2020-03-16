//
// Created by danielsf97 on 3/11/20.
//

#include "lsfs_state.h"

#include <utility>

lsfs_state::lsfs_state(std::shared_ptr<client> df_client, std::shared_ptr<spdlog::logger> logger):
        df_client(std::move(df_client)), logger(std::move(logger))
{}

long lsfs_state::increment_version_and_get(std::string path){
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

long lsfs_state::get_version(std::string path){
    std::scoped_lock<std::mutex> lk (version_tracker_mutex);
    auto it = version_tracker.find(path);
    if(it == version_tracker.end()){
        //não existe o path
        return -1;
    }else{
        return it->second;
    }
}

int lsfs_state::open_and_read_size(
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

int lsfs_state::put_metadata(metadata& met, const char* path){
    int return_value;

    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);

    try{
        long version = df_client->get_latest_version(path);
        df_client->put(path, version + 1, metadata_str.data(), metadata_str.size());
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

std::unique_ptr<metadata> lsfs_state::get_metadata(const char* path){
//    long version = get_version(path);
    std::unique_ptr<metadata> res = nullptr;
    try{
        long version = df_client->get_latest_version(path);
        if(version == -1){
            errno = ENOENT;
        }else{
            std::cout << "######################" << path << " VERSIONG: " << version << "#################################" << std::endl;
            //Fazer um get de metadados ao dataflasks
            std::shared_ptr<std::string> data = df_client->get(path, 1, &version);
            res = std::make_unique<metadata>(metadata::deserialize_from_string(*data));
        }
    }catch(TimeoutException& e){
        errno = EHOSTUNREACH; // Not Reachable Host
    }

    return res;
}

int lsfs_state::add_child_to_parent_dir(const char *path, bool is_dir) {
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
        std::shared_ptr<std::string> data;
        try {
             data = df_client->get(parent_path->c_str() /*, &version*/);
        }catch(TimeoutException& e){
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
        met.add_child(*child_name);
        // put metadata
        int res = put_metadata(met, parent_path->c_str());
        if(res != 0){
            return -errno;
        }
        return 0;
//        }
    }

    // root directory doesn't have a parent
    return 0;
}

void lsfs_state::add_open_file(const char* path, struct stat& stbuf, FileAccess::FileAccess access){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it == open_files.end()) {
        open_files.emplace(std::string(path), std::make_pair(access, std::make_shared<struct stat>(stbuf)));
    }
}

bool lsfs_state::is_file_opened(const char* path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        return true;
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

bool lsfs_state::update_open_file_metadata(const char* path, struct stat& stbuf){
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

bool lsfs_state::update_file_size_if_opened(const char* path, size_t size){
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

bool lsfs_state::update_file_time_if_opened(const char* path, const struct timespec ts[2]){
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

bool lsfs_state::get_metadata_if_file_opened(const char* path, struct stat* stbuf){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        memcpy(stbuf, it->second.second.get(), sizeof(struct stat));
        return true;
    }

    return false;
}


int lsfs_state::flush_open_file(const char* path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()){
        if(it->second.first == FileAccess::CREATED || it->second.first == FileAccess::MODIFIED){
            // Se o ficheiro foi modificado é necessário atualizar os metadados do mesmo

            // create metadata object
            metadata to_send(*(it->second.second));
            // serialize metadata object
            int res = put_metadata(to_send, path);
            if(res != 0){
                it->second.first = FileAccess::ACCESSED;
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
        }
        return 0;
    }

    errno = ENOENT;
    return -errno;
}

void lsfs_state::flush_and_release_open_file(const char* path) {
    std::scoped_lock<std::recursive_mutex> lk(open_files_mutex);
    flush_open_file(path);
    auto it = open_files.find(path);
    if (it != open_files.end()) {
        open_files.erase(it);
    }
}
