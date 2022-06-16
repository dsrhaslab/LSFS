//
// Created by danielsf97 on 2/26/20.
//

#include "metadata_childs.h"
#include "lsfs/fuse_lsfs/lsfs_impl.h"


metadata_childs::metadata_childs(const metadata_childs& met): childs(met.childs), added_childs(met.added_childs), removed_childs(met.removed_childs){};


void metadata_childs::add_child(std::string path, bool is_dir) {
    this->childs.insert(path);
    this->added_childs.emplace(is_dir? FileType::DIRECTORY : FileType::FILE, path);
}

void metadata_childs::remove_child(std::string path, bool is_dir) {
    this->childs.erase(path);
    this->added_childs.emplace(is_dir? FileType::DIRECTORY : FileType::FILE, path);
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
    this->added_childs.clear();
    this->removed_childs.clear();
}

bool metadata_childs::is_empty() {
    return this->childs.empty();
}

void metadata_childs::print_metadata(metadata_childs& met){
    std::cout << "Printing Metadata: <";
    for(auto& child: met.childs)
        std::cout << child << ", ";
    
    std::cout << ">" << std::endl;

}