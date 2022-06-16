//
// Created by danielsf97 on 2/26/20.
//

#include "metadata_attr.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"

metadata_attr::metadata_attr(struct stat& stbuf): stbuf(stbuf){};

metadata_attr::metadata_attr(const metadata_attr& met): stbuf(met.stbuf){};


std::string metadata_attr::serialize_to_string(metadata_attr& met){
    std::string metadata_str;
    boost::iostreams::back_insert_device<std::string> inserter(metadata_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << met;
    s_s.flush();
    return std::move(metadata_str);
}

metadata_attr metadata_attr::deserialize_from_string(const std::string& serial_str) {
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);

    metadata_attr res;
    ia >> res;

    return std::move(res);
}

void metadata_attr::initialize_metadata(struct stat* stbuf, mode_t mode, nlink_t nlink, gid_t gid, uid_t uid){
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
