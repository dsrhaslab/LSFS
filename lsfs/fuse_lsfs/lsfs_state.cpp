//
// Created by danielsf97 on 3/11/20.
//

#include "lsfs_state.h"
#include <ctime>
#include <utility>
#include <spdlog/spdlog.h>

lsfs_state::lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size, bool benchmark_performance, bool maximize_cache,
     int refresh_cache_time, int max_directories_in_cache, int percentage_of_entries_to_remove_if_cache_full):

        df_client(std::move(df_client)), max_parallel_read_size(max_parallel_read_size),
        max_parallel_write_size(max_parallel_write_size), benchmark_performance(benchmark_performance), maximize_cache(maximize_cache),
        refresh_cache_time(refresh_cache_time), max_directories_in_cache(max_directories_in_cache), percentage_of_entries_to_remove_if_cache_full(percentage_of_entries_to_remove_if_cache_full) 
{}

int lsfs_state::put_fixed_size_blocks_from_buffer_limited_paralelization(const char *buf, size_t size,
                                                                         size_t block_size, const char *base_path, size_t current_blk) {

    size_t write_off = 0;
    while(write_off < size){
        size_t write_size = std::min(this->max_parallel_write_size, size - write_off);
        int res = put_fixed_size_blocks_from_buffer(&buf[write_off], write_size, block_size, base_path, current_blk);
        if(res == 0){
            write_off += write_size;
            current_blk += (write_size / block_size);
        }else{
            return -1;
        }
    }

    return 0;
}

int lsfs_state::put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk){
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

    //Ate ter os blocos todos prontos
    while (read_off < size) {
        size_t write_size = (read_off + BLK_SIZE) > size ? (size - read_off) : BLK_SIZE;
        current_blk++;
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(base_path).append(":").append(std::to_string(current_blk));


        client_reply_handler::Response response = client_reply_handler::Response::Init;
        std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(blk_path, &response);
        
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
        size_t blk_write_size = std::min((data_blk->size()), (size - read_off));
        data_blk->copy(&buf[read_off], blk_write_size);
        read_off += blk_write_size;
    }

    return read_off;
}

int lsfs_state::put_block(const std::string& path, const char* buf, size_t size, bool is_merge) {
    int return_value;

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_key_version version; 
    if(last_v != nullptr)
        version = *last_v;        
    
    if(!is_merge) df_client->put(path, version, buf, size);
    else df_client->put_with_merge(path, version, buf, size);
    
    return_value = 0;

    return return_value;
}

int lsfs_state::put_metadata_as_dir(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);
     return this->put_block(path, metadata_str.data(), metadata_str.size(), true);
}


int lsfs_state::put_metadata(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);
     return this->put_block(path, metadata_str.data(), metadata_str.size());
}

int lsfs_state::put_metadata_stat(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = metadata_attr::serialize_to_string(met.attr);

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_key_version version; 
    if(last_v != nullptr){
        version = *last_v; 
        df_client->put_metadata_stat(path, version, metadata_str.data(), metadata_str.size());
    }else{
        errno = ENOENT;
        return -1;
    }    
     return 0;
}

int lsfs_state::put_metadata_child(const std::string& path, const std::string& child_path, bool is_create, bool is_dir){

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_key_version version; 
    if(last_v != nullptr){
        version = *last_v;  
        df_client->put_child(path, version, child_path, is_create, is_dir);
    }else{
        errno = ENOENT;
        return -1;
    }    
    return 0;  
}

int lsfs_state::put_with_merge_metadata(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = metadata::serialize_to_string(met);
    int return_value;

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_key_version version; 
    if(last_v != nullptr)
        version = *last_v; 

    df_client->put_with_merge(path, version, metadata_str.data(), metadata_str.size());
    
    return_value = 0;

    return return_value;
}


int lsfs_state::delete_file_or_dir(const std::string& path){
    int return_value = 0;

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_key_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_key_version version; 
    if(last_v != nullptr){
        version = *last_v; 

        df_client->del(path, version);

    }else{
        errno = ENOENT;
        return_value = -1;
    }
    

    return return_value;
}

std::unique_ptr<metadata> lsfs_state::get_metadata(const std::string& path){
    std::unique_ptr<metadata> res = nullptr;

    //Get size of metadata
    kv_store_key_version last_version;
    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::shared_ptr<std::string> data  = df_client->get_latest_metadata_size(path, &response, &last_version);
    
    if(response == client_reply_handler::Response::Deleted || data == nullptr){
        errno = ENOENT;
        return res;
    }
    size_t metadata_size = stol(*data);
    
    res = std::make_unique<metadata>(request_metadata(path, metadata_size, last_version));
    
    return res;
}

std::unique_ptr<metadata> lsfs_state::get_metadata_stat(const std::string& path){
    std::unique_ptr<metadata> res = nullptr;

    //Get size of metadata
    kv_store_key_version last_version;
    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::shared_ptr<std::string> data  = df_client->get_latest_metadata_stat(path, &response, &last_version);
              
    if(response == client_reply_handler::Response::Deleted || data == nullptr){
        errno = ENOENT;
        return res;
    }
    metadata met(metadata_attr::deserialize_from_string(*data));
    res = std::make_unique<metadata>(met);

    return res;
}

void lsfs_state::add_to_dir_cache(const std::string& path, metadata met) {
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    auto it = dir_cache.find(path);
    if(it != dir_cache.end()){
        it->second.metadata_p = std::make_unique<metadata>(met);
        it->second.last_update = now;
    }else{
        directory dir = {.metadata_p = std::make_unique<metadata>(met), .last_update = now};
        this->dir_cache.insert(std::make_pair(path, std::move(dir)));
    }
}

void lsfs_state::remove_old_dirs(){
    std::vector<std::pair<std::string, struct timespec>> old_dirs;
    int entries_to_remove_if_cache_full = max_directories_in_cache * (percentage_of_entries_to_remove_if_cache_full/100);

    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);

    auto it = dir_cache.begin();

    while(it != dir_cache.end() && old_dirs.size() <= entries_to_remove_if_cache_full){
        old_dirs.push_back(std::make_pair(it->first, it->second.last_update));
        it++;
    }

    while(it != dir_cache.end()){
        for(int i = 0; i < old_dirs.size(); i++){
            float sbt = it->second.last_update.tv_sec - old_dirs.at(i).second.tv_sec;
            sbt += (it->second.last_update.tv_nsec - old_dirs.at(i).second.tv_nsec) / 1000000000.0;
            if(sbt < 0){
                old_dirs.at(i) = std::make_pair(it->first, it->second.last_update);
            }
        }
        it++;
    }
}

bool lsfs_state::check_if_cache_full(){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);

    return dir_cache.size() >= max_directories_in_cache;
}

void lsfs_state::refresh_dir_cache() {
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    struct timespec now;
        
    for(auto it = dir_cache.begin(); it != dir_cache.end(); it++){
        
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        float t_dir = it->second.last_update.tv_sec + (refresh_cache_time / 1000.0) + (it->second.last_update.tv_nsec / 1000000000.0);
        float t_now = now.tv_sec + (now.tv_nsec / 1000000000.0);
        
        if(t_now >= t_dir){
            it->second.metadata_p = get_metadata(it->first);
            it->second.last_update = now;
        }
    }
}


void lsfs_state::remove_from_dir_cache(const std::string& path) {
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(path);
    if( it != dir_cache.end()){
            it = dir_cache.erase(it);
    }
}


void lsfs_state::add_child_to_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(parent_path);
    if( it != dir_cache.end()){
        it->second.metadata_p->childs.add_child(child_name, is_dir);
    }
}


int lsfs_state::add_child_to_parent_dir(const std::string& path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    std::unique_ptr<std::string> child_name = get_child_name(path);

    if(parent_path != nullptr){
        //std::cout << "Parent Path: " << *parent_path << std::endl;
        bool was_not_in_cache = false;

        this->add_child_to_dir_cache(*parent_path, *child_name, is_dir);
        
        // if(!benchmark_performance && maximize_cache && met == nullptr && *parent_path != "/"){
        //     met = get_metadata(*parent_path);
        //     add_or_refresh_working_directory(*parent_path, *met);
        //     was_not_in_cache = true;
        // }

        int res = put_metadata_child(*parent_path, *child_name, true, is_dir);
        if(res != 0){
            return -errno;
        }

        // if(was_not_in_cache){
        //     this->add_child_to_working_dir_and_retreive(*parent_path, *child_name, is_dir);
        // }
        //on successful put metadata clear add remove childs log
        this->reset_dir_cache_add_remove_log(*parent_path);
    }

    // root directory doesn't have a parent
    return 0;
}

void lsfs_state::remove_child_from_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(parent_path);
    if( it != dir_cache.end()){
        it->second.metadata_p->childs.remove_child(child_name, is_dir);
    }
}

int lsfs_state::remove_child_from_parent_dir(const std::string& path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    std::unique_ptr<std::string> child_name = get_child_name(path);

    if(parent_path != nullptr){
        bool was_not_in_cache = false;

        this->remove_child_from_dir_cache(*parent_path, *child_name, is_dir);

        // if(!benchmark_performance && maximize_cache && met == nullptr && *parent_path != "/"){
        //     met = get_metadata(*parent_path);
        //     add_or_refresh_working_directory(*parent_path, *met);
        //     was_not_in_cache = true;
        // }

        std::cout << "Trying to update metadata of parent path " << std::endl;
                
        int res = put_metadata_child(*parent_path, *child_name, false, is_dir);
        if(res != 0){
            return -errno;
        }

        // if(was_not_in_cache){
        //     this->remove_child_from_working_dir_and_retreive(*parent_path, *child_name, is_dir);
        // }

        //on successful put metadata clear add remove childs log
        this->reset_dir_cache_add_remove_log(*parent_path);
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

bool lsfs_state::is_dir_cached(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(path);
    if(it != dir_cache.end()){
        return true;
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

bool lsfs_state::get_metadata_if_dir_cached(const std::string& path, struct stat* stbuf){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(path);
    if(it != dir_cache.end()){
        memcpy(stbuf, &(it->second.metadata_p->attr.stbuf), sizeof(struct stat));
        return true;
    }
    return false;
}


std::unique_ptr<metadata> lsfs_state::get_metadata_if_dir_cached(const std::string& path){
    std::unique_ptr<metadata> res(nullptr);
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(path);
    if(it != dir_cache.end()){
        res = std::make_unique<metadata>(*(it->second.metadata_p));
    }
    return res;
}



int lsfs_state::flush_open_file(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()){
        if(it->second.first == FileAccess::CREATED){
            // create metadata object
            metadata to_send(*(it->second.second));
            // serialize metadata object
            int res = put_metadata(to_send, path);
            if(res != 0){
                return res;
            }
            
            // If the file was created for the first time, we have to add it to parent folder
            res = add_child_to_parent_dir(path, false);
            if(res != 0){
                return res;
            }
                       
        } else if(it->second.first == FileAccess::MODIFIED){
            // If the file was modified we must update its metadata

            // create metadata object
            metadata to_send(*(it->second.second));
            // serialize metadata object
            int res = put_metadata_stat(to_send, path);
            if(res != 0){
                return res;
            }
        }

        it->second.first = FileAccess::ACCESSED;

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

void lsfs_state::clear_all_dir_cache() {
    std::scoped_lock<std::recursive_mutex> lk(dir_cache_mutex);
    this->dir_cache.clear();
}

void lsfs_state::reset_dir_cache_add_remove_log(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache.find(path);
    if(it != dir_cache.end()) {
        it->second.metadata_p->childs.reset_add_remove_log();
    }
    
}


metadata lsfs_state::request_metadata(const std::string &base_path, size_t total_s, const kv_store_key_version& last_version){

    size_t NR_BLKS = (total_s / BLK_SIZE) + 1;
    
    std::vector<kv_store_key<std::string>> keys;
    std::vector<std::shared_ptr<std::string>> data_strs;

    keys.reserve(NR_BLKS);
    data_strs.reserve(NR_BLKS);
    

    for(int i = 1; i <= NR_BLKS; i++){
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(base_path).append(":").append(std::to_string(i));

        kv_store_key<std::string> key = {blk_path, last_version, false};

        keys.emplace_back(std::move(key));
        data_strs.emplace_back(nullptr);
    }
    df_client->get_metadata_batch(keys, data_strs);
    
    std::string met;
    
    for(const std::shared_ptr<std::string>& data_blk: data_strs){
        met += *data_blk;
    }
    
    return metadata::deserialize_from_string(met);
}
