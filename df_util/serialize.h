#ifndef P2PFS_SERIALIZE_H
#define P2PFS_SERIALIZE_H

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

template<class T> 
std::string serialize_to_string(const T& object){
    std::string value;
    boost::iostreams::back_insert_device<std::string> inserter(value);
    boost::iostreams::stream<boost::iostreams::back_insert_device<std::string> > s_s(inserter);
    boost::archive::binary_oarchive oa(s_s);
    oa << object;
    s_s.flush();
    return std::move(value);  
};

template<class T> 
T deserialize_from_string(const std::string& serial_str){
    boost::iostreams::basic_array_source<char> device(serial_str.data(), serial_str.size());
    boost::iostreams::stream<boost::iostreams::basic_array_source<char> > s_u(device);
    boost::archive::binary_iarchive ia(s_u);
    T object;
    ia >> object;

    return std::move(object);
};

#endif //P2PFS_SERIALIZE_H
