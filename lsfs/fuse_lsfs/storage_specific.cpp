//
// Created by danielsf97 on 3/25/20.
//

#include "metadata_childs.h"
#include "util.h"

std::string merge_metadata(const std::string& bytes, const std::string& new_bytes){
    metadata_childs met1 = metadata_childs::deserialize_from_string(bytes);
    metadata_childs met2 = metadata_childs::deserialize_from_string(new_bytes);

    for(auto& child_pair: met2.added_childs){
        auto res_pair = met1.childs.emplace(child_pair.second);
        if(res_pair.second){
            // if child was inserted (não há repetidos)
            if(child_pair.first == FileType::DIRECTORY){
                met1.stbuf.st_nlink++;
            }
        }
    }
    for(auto& child_pair: met2.removed_childs){
        auto it = met1.childs.find(child_pair.second);
        if(it != met1.childs.end()){
            met1.childs.erase(it);
            if(child_pair.first == FileType::DIRECTORY){
                met1.stbuf.st_nlink > 0 ? met1.stbuf.st_nlink-- : 0;
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

    return metadata_childs::serialize_to_string(met1);
}

