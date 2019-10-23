cmake_minimum_required(VERSION 3.15)
project(p2pfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(CapnProto CONFIG REQUIRED)
capnp_generate_cpp(packetSources packetHeaders serializer/capnp/packet.capnp)
find_package(nlohmann_json 3.2.0 REQUIRED)

find_package(Boost COMPONENTS thread REQUIRED)

set(INCLUDE_DIRECTORY include)
set(INCLUDE_HEADERS ${INCLUDE_DIRECTORY}/ctpl_stl.h)

#add_executable(bootstrapper bootstrapper/bootstrapper.h bootstrapper/bootstrapper_impl.h  ${packetSources} bootstrapper/bootstrapper_impl.cpp  tcp_client_server_connection/tcp_client_server_connection.h tcp_client_server_connection/tcp_server_connection.cpp tcp_client_server_connection/tcp_client_connection.cpp)
add_executable(bootstrapper bootstrapper/bootstrapper_impl.cpp ${packetSources}  tcp_client_server_connection/tcp_server_connection.cpp serializer/serializer.h serializer/capnp/capnp_serializer.h serializer/capnp/capnp_serializer.cpp ${INCLUDE_HEADERS})
target_link_libraries(bootstrapper PRIVATE CapnProto::capnp)
target_include_directories(bootstrapper PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(bootstrapper PRIVATE ${INCLUDE_DIRECTORY})

#add_executable(peer ${packetSources}  tcp_client_server_connection/tcp_client_server_connection.h tcp_client_server_connection/tcp_server_connection.cpp tcp_client_server_connection/tcp_client_connection.cpp core/peer.cpp core/peer.h pss/pss.cpp pss/pss.h)
add_executable(peer core/peer.cpp ${packetSources} pss/pss.h pss/pss.cpp  tcp_client_server_connection/tcp_client_connection.cpp tcp_client_server_connection/tcp_server_connection.cpp serializer/serializer.h serializer/capnp/capnp_serializer.h serializer/capnp/capnp_serializer.cpp pss/pss_listener.cpp pss/pss_listener.h pss/pss_message.h core/peer_data.h ${INCLUDE_HEADERS} pss/view_logger.h)
target_link_libraries(peer PRIVATE CapnProto::capnp)
target_link_libraries(peer PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer PRIVATE Boost::thread)
target_include_directories(peer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(peer PRIVATE ${INCLUDE_DIRECTORY})

