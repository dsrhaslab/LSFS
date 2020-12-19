//
// Created by danielsf97 on 3/25/20.
//

#include "metadata.h"
#include "util.h"

std::string merge_metadata(const std::string& bytes, const std::string& new_bytes){
    metadata met1 = metadata::deserialize_from_string(bytes);
    metadata met2 = metadata::deserialize_from_string(new_bytes);

    for(auto& child_pair: met2.added_childs){
        auto res_pair = met1.childs.emplace(child_pair.second);
        if(res_pair.second){
            // if child was inserted (não há repetidos)
            if(child_pair.first == FileType::DIRECTORY){
                met1.stbuf.st_nlink++;
            }
        }
    }

    if(met1.stbuf.st_atim < met2.stbuf.st_atim){
        met1.stbuf.st_atim = met2.stbuf.st_atim;
    }
    if(met1.stbuf.st_mtim < met2.stbuf.st_mtim){
        met1.stbuf.st_mtim = met2.stbuf.st_mtim; //or of permission
    }
    if(met1.stbuf.st_ctim < met2.stbuf.st_ctim){
        met1.stbuf.st_ctim = met2.stbuf.st_ctim; //or of permissions
    }

    return metadata::serialize_to_string(met1);
}

