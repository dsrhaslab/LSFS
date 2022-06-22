//
// Created by danielsf97 on 3/25/20.
//

#include "metadata/metadata.h"
#include "util.h"

std::string merge_metadata(const std::string& bytes, const std::string& new_bytes){
    metadata met1 = metadata::deserialize_from_string(bytes);
    metadata met2 = metadata::deserialize_from_string(new_bytes);

    for(auto& child_pair: met2.childs.added_childs){
        auto res_pair = met1.childs.childs.emplace(child_pair.second);
        met1.childs.added_childs.emplace(child_pair);
    }
    for(auto& child_pair: met2.childs.removed_childs){
        auto it = met1.childs.childs.find(child_pair.second);
        if(it != met1.childs.childs.end()){
            met1.childs.childs.erase(it);
            met1.childs.removed_childs.emplace(child_pair);
        }
    }

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

