#include "metadata.h"
#include "lsfs/fuse_lsfs/util.h"

metadata::metadata(metadata_attr attr): attr(attr){};

metadata::metadata(const metadata& met): attr(met.attr), childs(met.childs){};


std::string metadata::merge_attr(metadata met1, metadata met2){
    if(met1.attr.stbuf.st_atim < met2.attr.stbuf.st_atim){
        met1.attr.stbuf.st_atim = met2.attr.stbuf.st_atim;
    }
    if(met1.attr.stbuf.st_mtim < met2.attr.stbuf.st_mtim){
        met1.attr.stbuf.st_mtim = met2.attr.stbuf.st_mtim; //or of permission
    }
    if(met1.attr.stbuf.st_ctim < met2.attr.stbuf.st_ctim){
        met1.attr.stbuf.st_ctim = met2.attr.stbuf.st_ctim; //or of permissions
    }

    return serialize_to_string<metadata>(met1);
}