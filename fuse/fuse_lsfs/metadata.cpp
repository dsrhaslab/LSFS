//
// Created by danielsf97 on 2/26/20.
//

#include "metadata.h"
#include "fuse/fuse_lsfs/lsfs_impl.h"

metadata::metadata(struct stat& stbuf): stbuf(stbuf){};

void metadata::add_child(std::string path, bool is_dir) {
    this->childs.insert(path);
    this->added_childs.emplace(is_dir? FileType::DIRECTORY : FileType::FILE, path);
    // increase parent hard links if its a directory
    if(is_dir){
        this->stbuf.st_nlink++;
    }
}

std::string metadata::serialize_to_string(metadata& met){
    std::string metadata_str;
    boost::iostreams::back_insert_device<std::string> inserter(metadata_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << met;
    s_s.flush();
    return std::move(metadata_str);
}

metadata metadata::deserialize_from_string(std::string& serial_str) {
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);

    metadata res;
    ia >> res;

    return std::move(res);
}

void metadata::initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid){
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = mode;
    stbuf->st_gid = gid;
    stbuf->st_uid = uid;
    stbuf->st_nlink = nlink;
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    stbuf->st_atim = ts;
    stbuf->st_ctim = ts;
    stbuf->st_mtim = ts;
    stbuf->st_blksize = BLK_SIZE;
}

void metadata::reset_add_remove_log(){
    this->added_childs.clear();
    this->removed_childs.clear();
}