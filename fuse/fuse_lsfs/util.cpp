/* -------------------------------------------------------------------------- */



#include "util.h"


bool is_temp_file(std::string path){
    std::smatch match;
    return std::regex_search(path, match, temp_extensions);
}

std::unique_ptr<std::string> get_parent_dir(std::string path){
    std::smatch match;
    auto res = std::regex_search(path, match, parent_dir_pattern);

    if(match.size() > 1){
        return std::make_unique<std::string>(std::string(match[1].str()));
    }

    return nullptr;
}

std::unique_ptr<std::string> get_child_name(std::string path){
    std::smatch match;
    auto res = std::regex_search(path, match, child_name_pattern);

    if(match.size() > 1){
        return std::make_unique<std::string>(std::string(match[1].str()));
    }

    return nullptr;
}

bool operator <(const timespec& lhs, const timespec& rhs)
{
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}