#include "metadata/metadata.h"
#include "util.h"

/*
    Merge function of directory metadata structure.
    First argument (string bytes) has priority over the second (string new_bytes).

    Returns serialized merged string.
*/
std::string merge_metadata(const std::string& bytes, const std::string& new_bytes){
    metadata met1 = deserialize_from_string<metadata>(bytes);
    metadata met2 = deserialize_from_string<metadata>(new_bytes);

    std::map<std::string, std::pair<FileType::FileType, Status::Status>> temp;

    std::set<std::string> priority_remove;

    for(auto const& [path, val]: met1.childs.childs){
        if(val.second == Status::NONE || val.second == Status::ADDED){
            temp.insert({path, val});
        }
        else if(val.second == Status::REMOVED){
            priority_remove.insert(path);
        }
    }

    for(auto const& [path, val]: met2.childs.childs){
        if(val.second == Status::NONE){
            temp.insert({path, val});
        }
        else if(val.second == Status::ADDED){
            auto it = priority_remove.find(path);
            if(it == priority_remove.end()){
                temp.insert({path, val});
            }
        }
        else if(val.second == Status::REMOVED){
            auto it = temp.find(path);
            if(it != temp.end()){
                if(it->second.second == Status::NONE){
                    temp.erase(path);
                }
            }
        }
    }

    met1.childs.childs = temp;

    met1.childs.reset_add_remove_log();


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

