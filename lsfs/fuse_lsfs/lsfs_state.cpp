#include "lsfs_state.h"

lsfs_state::lsfs_state(std::shared_ptr<client> df_client, size_t max_parallel_read_size, size_t max_parallel_write_size, bool use_cache,
     int refresh_cache_time, int max_directories_in_cache, int max_nr_requests_timeout, int cache_max_nr_requests_timeout, int direct_io):

        df_client(std::move(df_client)), max_parallel_read_size(max_parallel_read_size),
        max_parallel_write_size(max_parallel_write_size), use_cache(use_cache),
        refresh_cache_time(refresh_cache_time), max_directories_in_cache(max_directories_in_cache), 
        max_nr_requests_timeout(max_nr_requests_timeout), cache_max_nr_requests_timeout(cache_max_nr_requests_timeout),
        direct_io(direct_io)
{
    if(use_cache){
        dir_cache_map.reserve(max_directories_in_cache);
        dir_cache_map_mutex.reserve(max_directories_in_cache);
    }

}

/*
    Put blocks organization in parallel_write_size blocks batch.
    Ex: max_parallel_write_size = 32k, aggregates 32k/BLK_SIZE blocks to send in batch.

    Return 0 if ok, -1 otherwise.
*/
int lsfs_state::put_fixed_size_blocks_from_buffer_limited_paralelization(const char *buf, size_t size,
                                                                         size_t block_size, const char *base_path, size_t current_blk, const kv_store_version& version) {                                                                      
    size_t write_off = 0;
    while(write_off < size){
        size_t write_size = std::min(this->max_parallel_write_size, size - write_off);
        int res = put_fixed_size_blocks_from_buffer(&buf[write_off], write_size, block_size, base_path, current_blk, version);
        if(res == 0){
            write_off += write_size;
            current_blk += (write_size / block_size);
        }else{
            return -1;
        }
    }

    return 0;
}

/*
    Put blocks from given buffer in structures to send to next client phase.

    Return 0.
*/
int lsfs_state::put_fixed_size_blocks_from_buffer(const char* buf, size_t size, size_t block_size, const char* base_path, size_t current_blk, const kv_store_version& version){
    
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

        kv_store_key<std::string> kv = {blk_path, version, FileType::FILE, false};
        keys.emplace_back(kv);
        buffers.emplace_back(&buf[read_off]);
        sizes.emplace_back(write_size);

        read_off += BLK_SIZE;
    }

    df_client->put_batch(keys, buffers, sizes);

    return 0;

}

/*
    Read blocks organization in parallel_read_size blocks.
    Ex: max_parallel_read_size = 32k, requests 32k/BLK_SIZE blocks in batch.

    Return bytes read.
*/
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

/*
    Read blocks to a given buffer.

    Return bytes read.
*/
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
        if(data_blk != nullptr){
            size_t blk_write_size = std::min((data_blk->size()), (size - read_off));
            data_blk->copy(&buf[read_off], blk_write_size);
            read_off += blk_write_size;
        }
    }

    return read_off;
}

/*
    Put single block.

    Return 0.
*/
int lsfs_state::put_block(const std::string& path, const char* buf, size_t size, const kv_store_version& version, FileType::FileType f_type) {
    int return_value = 0;

    df_client->put(path, version, f_type, buf, size);

    return return_value;
}

/*
    Put directory metadata. 
    Only used when creating directory (mkdir).
*/
int lsfs_state::put_dir_metadata(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = serialize_to_string<metadata>(met);

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_version version; 
    if(last_v != nullptr){
        version = *last_v; 
    }   

    return this->put_block(path, metadata_str.data(), metadata_str.size(), version, FileType::DIRECTORY);
}

/*
    Put file metadata.
    Checks if already exists path in storage nodes.
*/
int lsfs_state::put_file_metadata(metadata& met, const std::string& path){
    // serialize metadata object
    std::string metadata_str = serialize_to_string<metadata>(met);

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_version version; 
    if(last_v != nullptr){
        version = *last_v; 
    }
    
    return this->put_block(path, metadata_str.data(), metadata_str.size(), version, FileType::FILE);
}


/*
    Put file metadata given a known version.
*/
int lsfs_state::put_file_metadata(metadata& met, const std::string& path, const kv_store_version& version){
    // serialize metadata object
    std::string metadata_str = serialize_to_string<metadata>(met);
    return this->put_block(path, metadata_str.data(), metadata_str.size(), version, FileType::FILE);
}

/*
    Put dir metadata child on storage nodes.
*/
int lsfs_state::put_dir_metadata_child(const std::string& path, const std::string& child_path, bool is_create, bool is_dir){

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_version> last_v = df_client->get_latest_version(path, &response);
    
    kv_store_version version; 
    if(last_v != nullptr){
       version = *last_v;  
        df_client->put_child(path, version, child_path, is_create, is_dir);
    }else{
        errno = ENOENT;
        return -1;
    }    
    return 0;  
}

/*
    Deletes path.

    Return 0 if file exists, -1 otherwise.
*/
int lsfs_state::delete_(const std::string& path, FileType::FileType f_type){
    int return_value = 0;

    //Needs to get latest version for concurrency purposes.
    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::unique_ptr<kv_store_version> last_v = df_client->get_latest_version(path, &response);

    kv_store_version version; 
    if(last_v != nullptr){
        version = *last_v; 

        df_client->del(path, version, f_type);

    }else{
        errno = ENOENT;
        return_value = -1;
    }
    

    return return_value;
}

/*
    Deletes file path (unlink).

    Return 0 if file exists, -1 otherwise.
*/
int lsfs_state::delete_file(const std::string& path){
    return delete_(path, FileType::FILE);
}

/*
    Deletes directory path (rmdir).

    Return 0 if file exists, -1 otherwise.
*/
int lsfs_state::delete_dir(const std::string& path){
    return delete_(path, FileType::DIRECTORY);
}

/*
    Tries to obtain metadata from storage nodes.
    Tries to request "cache_max_nr_requests_timeout" times before given up.

    Return unique_ptr to metadata structure or null_ptr.
*/
std::unique_ptr<metadata> lsfs_state::get_dir_metadata_for_cache(const std::string& path, client_reply_handler::Response* response){
    std::unique_ptr<metadata> res = nullptr;

    for(int i=0; i<this->cache_max_nr_requests_timeout; i++){
        //Get size of metadata
        kv_store_version last_version;
        std::shared_ptr<std::string> data  = df_client->get_latest_metadata_size(path, response, &last_version);
        
        if(*response == client_reply_handler::Response::Deleted || data == nullptr){
            return res;
        }
        size_t metadata_size = stol(*data);
        
        res = std::move(request_metadata(path, metadata_size, last_version, response, true));

        if(res != nullptr || *response != client_reply_handler::Response::NoData_HigherVersion) break;
    }
    
    return res;
}

/*
    Tries to obtain metadata from storage nodes.
    Tries to request "max_nr_requests_timeout" times before given up.

    Return unique_ptr to metadata structure or null_ptr.
*/
std::unique_ptr<metadata> lsfs_state::get_dir_metadata(const std::string& path){
    std::unique_ptr<metadata> res = nullptr;
    client_reply_handler::Response response = client_reply_handler::Response::Init;

    for(int i=0; i<this->max_nr_requests_timeout; i++){
        //Get size of metadata
        kv_store_version last_version;
        std::shared_ptr<std::string> data  = df_client->get_latest_metadata_size(path, &response, &last_version);
        
        if(response == client_reply_handler::Response::Deleted || data == nullptr){
            errno = ENOENT;
            return res;
        }
        size_t metadata_size = stol(*data);
        
        res = std::move(request_metadata(path, metadata_size, last_version, &response));

        if(res != nullptr) 
            return res;

        if(response == client_reply_handler::Response::Deleted || response == client_reply_handler::Response::NoData){
            errno = ENOENT;
            return res;
        }
    }

    if(response == client_reply_handler::Response::NoData_HigherVersion)
        errno = ENOENT;

    return res;
}

/*
   Request latest metadata stat for a given path.

   Returns unique_ptr to metadata_attr structure or null_ptr.
*/
std::unique_ptr<metadata_attr> lsfs_state::get_metadata_stat(const std::string& path){

    kv_store_version last_version;
    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::shared_ptr<std::string> data  = df_client->get_latest_metadata_stat(path, &response, &last_version);
    
    if(response == client_reply_handler::Response::Deleted || data == nullptr){
        errno = ENOENT;
        return nullptr;
    }
    ;
    return std::make_unique<metadata_attr>(deserialize_from_string<metadata_attr>(*data));
}

/*
   Request latest metadata stat for a given path.

   Returns unique_ptr to metadata_attr structure or null_ptr,
    and respective version for the received data.
*/
std::unique_ptr<metadata_attr> lsfs_state::get_metadata_stat(const std::string& path, kv_store_version* last_version){

    client_reply_handler::Response response = client_reply_handler::Response::Init;
    std::shared_ptr<std::string> data  = df_client->get_latest_metadata_stat(path, &response, last_version);
              
    if(response == client_reply_handler::Response::Deleted || data == nullptr){
        errno = ENOENT;
        return nullptr;
    }

    return std::make_unique<metadata_attr>(deserialize_from_string<metadata_attr>(*data));
}

/*
   Adds directory metadata to cache.
*/
void lsfs_state::add_to_dir_cache(const std::string& path, metadata met) {
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    auto it = dir_cache_map.find(path);
    if(it != dir_cache_map.end()){

        auto it_mutex = dir_cache_map_mutex.find(path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);

        std::shared_ptr<directory> dir_p = std::move((*it->second));
        dir_p->metadata_p = std::make_unique<metadata>(met);

        dir_cache_list.erase(it->second);
        dir_cache_list.push_front(std::move(dir_p));
        
        it->second = dir_cache_list.begin();
        
        lk_dir.unlock();
        lk.unlock();

    }else{
        struct directory dir = {.path = path, .metadata_p = std::make_unique<metadata>(met), .last_update = now};
        
        dir_cache_list.push_front(std::make_shared<directory>(std::move(dir)));
        dir_cache_map.insert(std::make_pair(path, dir_cache_list.begin()));
        dir_cache_map_mutex.insert(std::make_pair(path, std::make_unique<std::mutex>()));

        lk.unlock();
    }
    lk.lock();
    
    if(dir_cache_map.size() > max_directories_in_cache){

        std::string path_to_del = dir_cache_list.back()->path;

        it = dir_cache_map.find(path_to_del);
        if(it != dir_cache_map.end()){

            auto it_mutex = dir_cache_map_mutex.find(path_to_del);

            std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
            
            dir_cache_list.pop_back();
            dir_cache_map.erase(it);

            lk_dir.unlock();
            
            dir_cache_map_mutex.erase(it_mutex);
        }
    }
    lk.unlock();
    
}

/*
   Print Cache.
   Not completed.

   Return string to be printed.
*/
std::string lsfs_state::print_cache(){
    std::unique_lock<std::recursive_mutex> lk(dir_cache_mutex);
    std::string res = "";
    for(auto it = dir_cache_list.begin(); it != dir_cache_list.end(); it++){
        
        
        auto it_map = dir_cache_map.find((*it)->path);
        auto it_mutex = dir_cache_map_mutex.find((*it)->path);
        
        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
        
        float time = (*it)->last_update.tv_sec + ((*it)->last_update.tv_nsec / 1000000000.0);
        
        lk_dir.unlock();
    }
    lk.unlock();
    return res;
}


/*
   Removes entry path from directory cache.
*/
void lsfs_state::remove_from_dir_cache(const std::string& path) {
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);
    
    auto it = dir_cache_map.find(path);
    if( it != dir_cache_map.end()){

        auto it_mutex = dir_cache_map_mutex.find(path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);

        dir_cache_list.erase(it->second);
        dir_cache_map.erase(it);

        lk_dir.unlock();

        dir_cache_map_mutex.erase(it_mutex);
        
    }
    lk.unlock();
}

/*
  Checks if cache is full.

  Return true if full, false otherwise.
*/
bool lsfs_state::check_if_cache_full(){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);

    return dir_cache_map.size() > max_directories_in_cache;
}

/*
    Refreshes cache entry if not updated in the last refresh_cache_time interval.
    refresh_cache_time in milliseconds.
*/
void lsfs_state::refresh_dir_cache() {
    struct timespec now;
    
    std::vector<std::string> keys;
    
    std::unique_lock<std::recursive_mutex> lk(dir_cache_mutex);
    
    keys.reserve(dir_cache_map.size());

    for(auto dir : dir_cache_map)
        keys.push_back(dir.first);
    
    lk.unlock();

    for(auto key: keys){
        std::unique_lock<std::recursive_mutex> lk(dir_cache_mutex);
        
        auto it = dir_cache_map.find(key);
        if(it != dir_cache_map.end()){
            auto it_mutex = dir_cache_map_mutex.find(key);
            std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
            
            lk.unlock();
            clock_gettime(CLOCK_MONOTONIC, &now);
            
            float t_dir = (*it->second)->last_update.tv_sec + (refresh_cache_time / 1000.0) + ((*it->second)->last_update.tv_nsec / 1000000000.0);
            float t_now = now.tv_sec + (now.tv_nsec / 1000000000.0);
            
            if(t_now >= t_dir){
                try{
                    client_reply_handler::Response response = client_reply_handler::Response::Init;
                    std::unique_ptr<metadata> meta = get_dir_metadata_for_cache(it->first, &response);
                    if(meta != nullptr){
                        (*it->second)->metadata_p = std::move(meta);
                        (*it->second)->last_update = now;
                    }                    
                                            
                }catch(TimeoutException& e){
                    e.what();
                }catch(EmptyViewException& e){
                    e.what();
                }
            }
            
            lk_dir.unlock();
            
        }
    }
    
}

/*
    Adds directory metadata child to cache if parent path cached.
*/
void lsfs_state::add_child_to_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir){
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);

    auto it = dir_cache_map.find(parent_path);
    if( it != dir_cache_map.end()){

        auto it_mutex = dir_cache_map_mutex.find(parent_path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
        lk.unlock();

        (*it->second)->metadata_p->childs.add_child(child_name, is_dir);

        lk_dir.unlock();
    }
}

/*
    Adds child to parent dir (cache and storage nodes).
    
    Return 0 if ok, errno otherwise.
*/
int lsfs_state::add_child_to_parent_dir(const std::string& path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    std::unique_ptr<std::string> child_name = get_child_name(path);

    if(parent_path != nullptr){

        if(use_cache) this->add_child_to_dir_cache(*parent_path, *child_name, is_dir);
        

        int res = put_dir_metadata_child(*parent_path, *child_name, true, is_dir);
        if(res != 0){
            return -errno;
        }
    }

    // root directory doesn't have a parent
    return 0;
}

/*
    Removes directory metadata child from cache if parent path cached.
*/
void lsfs_state::remove_child_from_dir_cache(const std::string& parent_path, const std::string& child_name, bool is_dir){
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);

    auto it = dir_cache_map.find(parent_path);
    if( it != dir_cache_map.end()){
         auto it_mutex = dir_cache_map_mutex.find(parent_path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
        lk.unlock();

        (*it->second)->metadata_p->childs.remove_child(child_name, is_dir);

        lk_dir.unlock();
    }
}

/*
    Removes child from parent dir (cache and storage nodes).
    
    Return 0 if ok, errno otherwise.
*/
int lsfs_state::remove_child_from_parent_dir(const std::string& path, bool is_dir) {
    std::unique_ptr<std::string> parent_path = get_parent_dir(path);
    std::unique_ptr<std::string> child_name = get_child_name(path);

    if(parent_path != nullptr){

        if(use_cache) this->remove_child_from_dir_cache(*parent_path, *child_name, is_dir);
        
        int res = put_dir_metadata_child(*parent_path, *child_name, false, is_dir);
        if(res != 0){
            return -errno;
        }
    }
    // root directory doesn't have a parent
    return 0;
}

/*
    Adds entry to open files structure.
    Used when file is created.
*/
void lsfs_state::add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it == open_files.end()) {
        kv_store_version version;
        struct file file_c = {.access_t = access, .stat = std::make_shared<struct stat>(stbuf), .version = version};
        open_files.emplace(path, file_c);
    }
}

/*
    Adds entry to open files structure, specifying the version.
    Used when file is opened.
*/
void lsfs_state::add_open_file(const std::string& path, struct stat& stbuf, FileAccess::FileAccess access, kv_store_version n_version){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it == open_files.end()) {
        struct file file_c = {.access_t = access, .stat = std::make_shared<struct stat>(stbuf), .version = n_version};
        open_files.emplace(path, file_c);
    }
}

/*
    Verifies in open files structure if a given path exists.

    Return true if exists, false otherwise.
*/
bool lsfs_state::is_file_opened(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        return true;
    }
    return false;
}

/*
    Verifies if a directory path is cached.

    Return true if cached, false otherwise.
*/
bool lsfs_state::is_dir_cached(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (dir_cache_mutex);
    auto it = dir_cache_map.find(path);
    if(it != dir_cache_map.end()){
        return true;
    }
    return false;
}

/*
    Updates cached file metadata given a stat struct.

    Return true if existes, false otherwise.
*/
bool lsfs_state::update_open_file_metadata(const std::string& path, struct stat& stbuf){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        std::shared_ptr<struct stat> open_stbuf = it->second.stat;

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
            if(it->second.access_t == FileAccess::ACCESSED){
                it->second.access_t = FileAccess::MODIFIED;
            }
        }

        return true;
    }

    return false;
}

/*
    Updates file size.

    Return true if existes, false otherwise.
*/
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

/*
    Updates file times.

    Return true if existes, false otherwise.
*/
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

/*
    Retrieves file metadata.

    Return true if existes, false otherwise.
*/
bool lsfs_state::get_metadata_if_file_opened(const std::string& path, struct stat* stbuf){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        memcpy(stbuf, it->second.stat.get(), sizeof(struct stat));
        return true;
    }

    return false;
}

/*
    Retrieves file version.

    Return true if existes, false otherwise.
*/
bool lsfs_state::get_version_if_file_opened(const std::string& path, kv_store_version* version){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()) {
        *version = it->second.version;
        return true;
    }

    return false;
}

/*
    Retrieves directory metadata_attr stat from cache.

    Return true if cached, false otherwise.
*/
bool lsfs_state::get_metadata_if_dir_cached(const std::string& path, struct stat* stbuf){
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);
    
    auto it = dir_cache_map.find(path);
    if(it != dir_cache_map.end()){
        
        auto it_mutex = dir_cache_map_mutex.find(path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
        
        std::shared_ptr<directory> dir_p = std::move(*it->second);

        *stbuf = dir_p->metadata_p->attr.stbuf;
 
        dir_cache_list.erase(it->second);
        dir_cache_list.push_front(std::move(dir_p)); //move necessario porque é local variavel dir_p
        
        it->second = dir_cache_list.begin();

        lk_dir.unlock();
        lk.unlock();
        return true;
    }
    return false;
}

/*
    Retrieves directory object from cache.
    
    Return shared_ptr of directory obj or nullptr.
*/
std::shared_ptr<lsfs_state::directory> lsfs_state::get_metadata_if_dir_cached(const std::string& path){
    std::shared_ptr<directory> res(nullptr);
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);
    
    auto it = dir_cache_map.find(path);
    if(it != dir_cache_map.end()){

         auto it_mutex = dir_cache_map_mutex.find(path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);

        std::shared_ptr<directory> dir_p = std::move(*it->second);

        res = dir_p;

        dir_cache_list.erase(it->second);
        dir_cache_list.push_front(std::move(dir_p));

        it->second = dir_cache_list.begin();

        lk_dir.unlock();
        lk.unlock();
    }
    return res;
}

/*
    Verifies if directory metadata childs are empty.
    
    Return true if empty, false otherwise.
    Return bool* true if cache, false otherwise.
*/
bool lsfs_state::is_dir_childs_empty(const std::string& path, bool* dir_cached){
    std::unique_lock<std::recursive_mutex> lk (dir_cache_mutex);
    bool res = false;
    auto it = dir_cache_map.find(path);
    if(it != dir_cache_map.end()){

         auto it_mutex = dir_cache_map_mutex.find(path);

        std::unique_lock<std::mutex> lk_dir(*it_mutex->second);
        lk.unlock();

        res = (*it->second)->metadata_p->childs.is_empty();

        lk_dir.unlock();

        *dir_cached = true;
    }else {
        *dir_cached = false;
    }
    return res;
}

/*
    Flushes open file to storage peers.

    Return 0 if file exists, errno (No such file or directory) otherwise.
*/
int lsfs_state::flush_open_file(const std::string& path){
    std::scoped_lock<std::recursive_mutex> lk (open_files_mutex);
    auto it = open_files.find(path);
    if(it != open_files.end()){
        if(it->second.access_t == FileAccess::CREATED){
            // create metadata object
            metadata to_send(*(it->second.stat));
            // serialize metadata object
            int res = put_file_metadata(to_send, path, it->second.version);
            if(res != 0){
                return res;
            }
            
            // If the file was created for the first time, we have to add it to parent folder
            res = add_child_to_parent_dir(path, false);
            if(res != 0){
                return res;
            }
                       
        } else if(it->second.access_t == FileAccess::MODIFIED){
            // If the file was modified we must update its metadata

            // create metadata object
            metadata to_send(*(it->second.stat));
            // serialize metadata object
            int res = put_file_metadata(to_send, path, it->second.version);
            if(res != 0){
                return res;
            }
        }

        it->second.access_t = FileAccess::ACCESSED;

        return 0;
    }

    errno = ENOENT;
    return -errno;
}

/*
    Flushes open file to storage peers and removes it from open files.

    Return 0 if file exists, errno (No such file or directory) otherwise.
*/
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

/*
    Clears direcotry cache.
*/
void lsfs_state::clear_all_dir_cache() {
    std::scoped_lock<std::recursive_mutex> lk(dir_cache_mutex);
    this->dir_cache_map.clear();
    this->dir_cache_list.clear();
    this->dir_cache_map_mutex.clear();
}

/*
    Request metadata blocks and constructs final metadata structure.

    Return unique_ptr to metadata or nullptr.
*/
std::unique_ptr<metadata> lsfs_state::request_metadata(const std::string &base_path, size_t total_s, const kv_store_version& last_version, client_reply_handler::Response* response, bool for_cache){

    size_t NR_BLKS = (total_s / BLK_SIZE);

    if(total_s % BLK_SIZE > 0) NR_BLKS = NR_BLKS + 1;
    
    std::vector<kv_store_key<std::string>> keys;
    std::vector<std::shared_ptr<std::string>> data_strs;

    keys.reserve(NR_BLKS);
    data_strs.reserve(NR_BLKS);
    

    for(int i = 1; i <= NR_BLKS; i++){
        std::string blk_path;
        blk_path.reserve(100);
        blk_path.append(base_path).append(":").append(std::to_string(i));

        kv_store_key<std::string> key = {blk_path, last_version, FileType::DIRECTORY, false};

        keys.emplace_back(std::move(key));
        data_strs.emplace_back(nullptr);
    }

    if(for_cache){
        df_client->get_metadata_batch(keys, data_strs, response, this->cache_max_nr_requests_timeout);
    }else{
        df_client->get_metadata_batch(keys, data_strs, response);
    }

    if(*response != client_reply_handler::Response::Ok){
        return nullptr;
    }
    
    std::string met;
    
    for(const std::shared_ptr<std::string>& data_blk: data_strs){
        met += *data_blk;
    }
    
    return std::make_unique<metadata>(deserialize_from_string<metadata>(met));
}
