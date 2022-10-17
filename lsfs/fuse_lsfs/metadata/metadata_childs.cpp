#include "metadata_childs.h"

metadata_childs::metadata_childs(const metadata_childs& met): childs(met.childs){};


void metadata_childs::add_child(std::string path, bool is_dir) {
    std::pair<FileType::FileType, Status::Status> obj = std::make_pair(is_dir? FileType::DIRECTORY : FileType::FILE, Status::ADDED);
    auto it = this->childs.find(path);
    if (it != this->childs.end()){
        it->second = obj;
    }else{
        this->childs.insert({path, obj});
    }
}

void metadata_childs::remove_child(std::string path, bool is_dir) {
    auto it = this->childs.find(path);
    if (it != this->childs.end()){
        it->second.second = Status::REMOVED;
    }
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


void metadata_childs::print_childs() {
    std::cout << "Childs: <";
    for(auto const& [key, val]: this->childs){
        std::cout << key << ">,";
    }
    std::cout << std::endl;
    
}