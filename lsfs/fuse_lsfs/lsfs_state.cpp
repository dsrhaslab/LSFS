//
// Created by danielsf97 on 3/11/20.
//

#include "lsfs_state.h"
#include <ctime>
#include <utility>
#include <spdlog/spdlog.h>

lsfs_state::lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size):
        df_client(std::move(df_client)), max_parallel_read_size(max_parallel_read_size),
        max_parallel_write_size(max_parallel_write_size)
{}

int lsfs_state::put_fixed_size_blocks_from_buffer_limited_paralelization(const char *buf, size_t size,
                                                                         size_t block_size, const char *base_path, size_t current_blk, bool timestamp_version) {

    size_t write_off = 0;
    while(write_off < size){
        size_t write_size = std::min(this->max_parallel_write_size, size - write_off);
        int res = put_fixed_size_blocks_from_buffer(&buf[write_off], write_size, block_size, base_path, current_blk, timestamp_version);
        if(res == 0){
            write_off += write_size;
            current_blk += (write_size / block_size);
        }else{
            return -1;
        }
    }

    return 0;
}

int lsfs_state::put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk, bool timestamp_version){
    int return_value;
    size_t read_off = 0;
    size_t nr_blocks = (size / block_size) + (size % block_size == 0 ? 0 : 1);
    // array for the keys
    std::vector<kv_store_key<std::string>> keys;
    keys.reserve(nr_blocks);
    
    // array for buffer pointers
    std::vector<const char*> buffers;
    buffers.reserve(nr_blocks);
    // array for sizes
    std::vector<size_t> sizes;
    sizes.reserve(nr_blocks);


    try{
        //Ate ter os blocos todos prontos
        while (read_off < size) {
            size_t write_size = (read_off + BLK_SIZE) > size ? (size - read_off) : BLK_SIZE;
            current_blk++;
            std::string blk_path;
            blk_path.reserve(100);
            blk_path.append(base_path).append(":").append(std::to_string(current_blk));
            
            std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(blk_path);
            
            kv_store_key_version version;
            if(last_v != nullptr)
                version = *last_v;

            kv_store_key<std::string> kv = {blk_path, version, false};
            keys.emplace_back(kv);
            buffers.emplace_back(&buf[read_off]);
            sizes.emplace_back(write_size);

            read_off += BLK_SIZE;
        }

        df_client->put_batch(keys, buffers, sizes);

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

size_t lsfs_state::read_fixed_size_blocks_to_buffer_limited_paralelization(char *buf, size_t size, size_t block_size, const char *base_path, size_t current_blk) {
    size_t read_off = 0;
    while(read_off < size){
        size_t read_size = std::min(this->max_parallel_read_size, size - read_off);
        size_t actually_read = read_fixed_size_blocks_to_buffer(&buf[read_off], read_size, block_size, base_path, current_blk);
        read_off += actually_read;
        current_blk += (actually_read / block_size);
    }
    return read_off;
}


size_t lsfs_state::read_fixed_size_blocks_to_buffer(char *buf, size_t size, size_t block_size, const char *base_path, size_t current_blk) {
    size_t read_off = 0;
    size_t nr_blocks = (size / block_size) + (size % block_size == 0 ? 0 : 1);
    // array for the keys
    std::vector<std::string> keys;
    keys.reserve(nr_blocks);
    // array for versions
    std::vector<std::shared_ptr<std::string>> data_strs;
    data_strs.reserve(nr_blocks);

    for(int i = 0; i < nr_blocks; i++){
        current_blk++;
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(base_path).append(":").append(std::to_string(current_blk));

        keys.emplace_back(std::move(blk_path));
        data_strs.emplace_back(nullptr);
    }
    
    df_client->get_latest_batch(keys, data_strs);

    for(const std::shared_ptr<std::string>& data_blk: data_strs){
        if(data_blk == nullptr) std::cout << "\n\n\n\n\n\n\n\n\n\n Um dos blocos retornou null \n\n\n\n\n\n\n\n\n" << std::endl;

        size_t blk_write_size = std::min((data_blk->size()), (size - read_off));
        data_blk->copy(&buf[read_off], blk_write_size);
        read_off += blk_write_size;
    }

    return read_off;
}

int lsfs_state::put_block(const std::string& path, const char* buf, size_t size, bool timestamp_version) {
    int return_value;

    try{
        // if(timestamp_version){
        //     version = std::chrono::duration_cast< std::chrono::nanoseconds >( std::chrono::high_resolution_clock::now().time_since_epoch()).count() / 100u;
        
        std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path);
        kv_store_key_version version; 
        if(last_v != nullptr)
            version = *last_v;        
        
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
        std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path);
        kv_store_key_version version; 
        if(last_v != nullptr)
            version = *last_v; 

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
    std::unique_ptr<metadata> res = nullptr;
    try{
        std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path);

        if(last_v == nullptr){
            errno = ENOENT;
        }else{
            //Fazer um get de metadados ao dataflasks
            std::shared_ptr<std::string> data = df_client->get(path, 1, *last_v);
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
        std::cout << "Parent Path: " << *parent_path << std::endl;

        std::unique_ptr<metadata> met(nullptr);
        met = this->add_child_to_working_dir_and_retreive(*parent_path, *child_name, is_dir);
        if(met == nullptr){
            std::cout << "Parent folder is not a working directory " << std::endl;
            //parent folder is not a working directory
            try {                
                std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(*parent_path);
                //TODO check if this error is valid 
                if(last_v == nullptr){
                   std::cout << "Got latest but is nullptr " << std::endl;
                   return -errno;
                }
                else{
                    std::cout << "Got latest " << std::endl;
                    print_kv(*last_v);
                
                    std::unique_ptr<std::string> data = df_client->get(*parent_path, *last_v);
                    std::cout << "Got parent path metadata " << std::endl;
                    // reconstruir a struct stat com o resultado
                    met = std::make_unique<metadata>(metadata::deserialize_from_string(*data));
                    // remove last version added/removed child log
                    met->reset_add_remove_log();
                    // add child path to metadata
                    met->add_child(*child_name, is_dir);

                    if(*parent_path != "/"){
                        add_or_refresh_working_directory(*parent_path, *met);
                    }
                }
            }catch(TimeoutException& e){
                errno = EHOSTUNREACH; // Not Reachable Host
                return -errno;
            }
        }

        if(is_working_directory(*parent_path)){
            std::cout << "Trying to update metadata of parent path " << std::endl;
                
            // put metadata
            int res = put_metadata(*met, *parent_path, true);
            if(res != 0){
                return -errno;
            }
            //on successful put metadata clear add remove childs log
            this->reset_working_directory_add_remove_log(*parent_path);
        }else{
            std::cout << "Trying to update metadata -- put with merge -- of parent path " << std::endl;

            int res = put_with_merge_metadata(*met, *parent_path);
            if(res != 0){
                return -errno;
            }
        }

        return 0;
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
            // If the file was modified we must update its metadata

            // create metadata object
            metadata to_send(*(it->second.second));
            // serialize metadata object
            int res = put_metadata(to_send, path, true);
            if(res != 0){
                return res;
            }

            if(it->second.first == FileAccess::CREATED){
                // If the file was created for the first time, we have to add it to parent folder
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
            break;
        }
    }
}
