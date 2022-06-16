//
// Created by danielsf97 on 2/26/20.
//

#include "metadata.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"


metadata::metadata(metadata_attr attr): attr(attr){};

metadata::metadata(const metadata& met): attr(met.attr), childs(met.childs){};


std::string metadata::serialize_to_string(metadata& met){
    std::string metadata_str;
    boost::iostreams::back_insert_device<std::string> inserter(metadata_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << met;
    s_s.flush();
    return std::move(metadata_str);
}

metadata metadata::deserialize_from_string(const std::string& serial_str) {
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);

    metadata res;
    ia >> res;

    return std::move(res);
}

std::string metadata::merge_metadata(metadata met1, metadata met2){
    if(met1.attr.stbuf.st_atim < met2.attr.stbuf.st_atim){
        met1.attr.stbuf.st_atim = met2.attr.stbuf.st_atim;
    }
    if(met1.attr.stbuf.st_mtim < met2.attr.stbuf.st_mtim){
        met1.attr.stbuf.st_mtim = met2.attr.stbuf.st_mtim; //or of permission
    }
    if(met1.attr.stbuf.st_ctim < met2.attr.stbuf.st_ctim){
        met1.attr.stbuf.st_ctim = met2.attr.stbuf.st_ctim; //or of permissions
    }

    return metadata::serialize_to_string(met1);
}