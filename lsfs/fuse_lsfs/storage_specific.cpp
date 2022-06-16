//
// Created by danielsf97 on 3/25/20.
//

#include "metadata/metadata.h"
#include "util.h"

std::string merge_metadata(const std::string& bytes, const std::string& new_bytes){
    metadata met1 = metadata::deserialize_from_string(new_bytes);
    metadata met2 = metadata::deserialize_from_string(bytes);

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

