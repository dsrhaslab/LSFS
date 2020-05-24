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

size_t convert_string_size_to_num_bytes(const std::string &format_size) {
    boost::cmatch match;
    auto res = boost::regex_search(format_size.c_str(), match, string_capacity_size_regex);

    size_t size;
    char unit;


    try{
        if(match.size() > 2){
            size = std::stoi(match[1].str());
            unit = (char) toupper((unsigned char) match[2].str()[0]);
        }else{
            throw "Bad read/write parallelization config format!!";
        }
    }catch(std::invalid_argument& e){
        throw "Bad read/write parallelization config format!!";
    }

    switch(unit){
        case 'B':
            break;
        case 'K':
            size *= 1024;
            break;
        case 'M':
            size *= (1024 * pow (10, 3));
            break;
        case 'G':
            size *= (1024 * pow (10, 6));
            break;
        default:
            break;
    }

    if(size % BLK_SIZE != 0)
        throw "Read/write parallelization value should be a multiple of Block Size!!";

    return size;

}