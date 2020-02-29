//
// Created by danielsf97 on 2/26/20.
//

#include "metadata.h"

metadata::metadata(struct stat& stbuf): stbuf(stbuf){};

std::string metadata::serialize_to_string(metadata& met){
    std::string metadata_str;
    boost::iostreams::back_insert_device<std::string> inserter(metadata_str);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << met;
    s_s.flush();
    return std::move(metadata_str);
}

metadata metadata::deserialize_from_string(std::string& serial_str) {
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);

    metadata res;
    ia >> res;

    return std::move(res);
}