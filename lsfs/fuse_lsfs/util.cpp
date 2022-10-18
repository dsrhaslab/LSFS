#include "util.h"

/*
    Checks if a given path is a temporary file.

    Return true if it does, false otherwise.
*/
bool is_temp_file(const std::string& path){
    boost::cmatch match;
    return boost::regex_search(path.c_str(), match, temp_extensions);
}

/*
    Retrieves from given path the parent dir.
    Ex: /dir1/file => /dir1.

    Return unique_ptr for the string parent dir.
*/
std::unique_ptr<std::string> get_parent_dir(const std::string& path){
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

/*
    Retrieves from given path the child name.
    Ex: /dir1/file => file.

    Return unique_ptr for the string child name.
*/
std::unique_ptr<std::string> get_child_name(const std::string& path){
    boost::cmatch match;

    auto res = boost::regex_search(path.c_str(), match, child_name_pattern);

    if(match.size() > 1){
        return std::make_unique<std::string>(std::string(match[1].str()));
    }

    return nullptr;
}

/*
    Operator comparator for timespec.
    Used for storage_specific.cpp and metadata/metadata.cpp
*/
bool operator <(const timespec& lhs, const timespec& rhs){
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec < rhs.tv_nsec;
    else
        return lhs.tv_sec < rhs.tv_sec;
}

/*
    Converts string size to number in bytes.
    Must be multiple of BLK_SIZE.
    Ex: 1k => 1024 bytes.
    
    Returns size_t if valid string format argument.
*/
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