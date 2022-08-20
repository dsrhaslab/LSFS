//
// Created by danielsf97 on 2/26/20.
//

#include "metadata_childs.h"
//#include "lsfs/fuse_lsfs/lsfs_impl.h"


metadata_childs::metadata_childs(const metadata_childs& met): childs(met.childs){};


void metadata_childs::add_child(std::string path, bool is_dir) {
    std::pair<FileType::FileType, Status::Status> obj = std::make_pair(is_dir? FileType::DIRECTORY : FileType::FILE, Status::ADDED);
    this->childs.insert({path, obj});
}

void metadata_childs::remove_child(std::string path, bool is_dir) {
    auto it = this->childs.find(path);
    if (it != this->childs.end()){
        it->second.second = Status::REMOVED;
    }
}


std::string metadata_childs::serialize_to_string(metadata_childs& met){
    std::string metadata_str;
    boost::iostreams::back_insert_device<std::string> inserter(metadata_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << met;
    s_s.flush();
    return std::move(metadata_str);
}

metadata_childs metadata_childs::deserialize_from_string(const std::string& serial_str) {
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);

    metadata_childs res;
    ia >> res;

    return std::move(res);
}

void metadata_childs::reset_add_remove_log(){
    std::vector<std::string> to_erase;
    for(auto & [key, val]: this->childs){
        //if is_removed == true
        if(val.second == Status::REMOVED){
            to_erase.push_back(key);
        }
        else if(val.second == Status::ADDED){
            val.second = Status::NONE;
        }
    }

    for(auto path: to_erase){
        this->childs.erase(path);
    }
}

void metadata_childs::reset_status(){
    for(auto & [key, val]: this->childs){
        val.second = Status::NONE;
    }
}

bool metadata_childs::is_empty() {
    bool is_empty = true;
    for(auto const& [key, val]: this->childs){
        //if is_removed == False
        if(val.second == Status::ADDED || val.second == Status::NONE){
            is_empty = false;
            break;
        }
    }
    return is_empty;
}
