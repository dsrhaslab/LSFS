/* -------------------------------------------------------------------------- */



#include "util.h"

bool is_temp_file(const std::string& path){
    boost::cmatch match;
    return boost::regex_search(path.c_str(), match, temp_extensions);
    //std::smatch match;
    //return std::regex_search(path, match, temp_extensions);
}

std::unique_ptr<std::string> get_parent_dir(const std::string& path){
    //std::smatch match;
    boost::cmatch match;

    if(path == "/") return nullptr;

    auto res = boost::regex_search(path.c_str(), match, parent_dir_pattern);

    if(match.size() > 1){
        std::string parent_dir = std::string(match[1].str());
        parent_dir = (parent_dir == "") ? "/" : parent_dir;
        return std::make_unique<std::string>(std::move(parent_dir));
    }

    return nullptr;
}

std::unique_ptr<std::string> get_child_name(const std::string& path){
    //std::smatch match;
    boost::cmatch match;

    auto res = boost::regex_search(path.c_str(), match, child_name_pattern);

    if(match.size() > 1){
        return std::make_unique<std::string>(std::string(match[1].str()));
    }

    return nullptr;
}

bool operator <(const timespec& lhs, const timespec& rhs){
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}
