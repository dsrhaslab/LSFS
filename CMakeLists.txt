cmake_minimum_required(VERSION 3.15)
project(p2pfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(CapnProto CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)

# check if protobuf was found
if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS serializer/protos/pss_message.proto)
capnp_generate_cpp(packetSources packetHeaders serializer/capnp/packet.capnp)

set(INCLUDE_DIRECTORY include)
set(INCLUDE_HEADERS ${INCLUDE_DIRECTORY}/ctpl_stl.h)

#add_executable(bootstrapper bootstrapper/bootstrapper.h bootstrapper/bootstrapper_impl.h  ${packetSources} bootstrapper/bootstrapper_impl.cpp  tcp_client_server_connection/tcp_client_server_connection.h tcp_client_server_connection/tcp_server_connection.cpp tcp_client_server_connection/tcp_client_connection.cpp)
add_executable(bootstrapper_exe bootstrapper/bootstrapper_impl.cpp ${packetSources}  tcp_client_server_connection/tcp_server_connection.cpp serializer/serializer.h serializer/capnp/capnp_serializer.h serializer/capnp/capnp_serializer.cpp ${INCLUDE_HEADERS})
target_link_libraries(bootstrapper_exe PRIVATE CapnProto::capnp)
target_include_directories(bootstrapper_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(bootstrapper_exe PRIVATE ${INCLUDE_DIRECTORY})
target_link_libraries(bootstrapper_exe PUBLIC ${Boost_LIBRARIES})

#add_executable(peer ${packetSources}  tcp_client_server_connection/tcp_client_server_connection.h tcp_client_server_connection/tcp_server_connection.cpp tcp_client_server_connection/tcp_client_connection.cpp core/peer.cpp core/peer.h pss/pss.cpp pss/pss.h)
add_executable(peer_exe core/peer.cpp ${packetSources} pss/pss.h pss/pss.cpp  tcp_client_server_connection/tcp_client_connection.cpp tcp_client_server_connection/tcp_server_connection.cpp serializer/serializer.h serializer/capnp/capnp_serializer.h serializer/capnp/capnp_serializer.cpp pss/pss_listener.cpp pss/pss_listener.h pss/pss_message.h core/peer_data.h ${INCLUDE_HEADERS} pss/view_logger.h ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(peer_exe PRIVATE CapnProto::capnp)
target_link_libraries(peer_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(peer_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(peer_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(peer_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(peer_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})






