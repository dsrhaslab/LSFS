cmake_minimum_required(VERSION 3.15)
project(p2pfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH "/usr/local/lib64/cmake")

find_package(CapnProto CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS system thread REQUIRED)
find_package(yaml-cpp REQUIRED)

# check if protobuf was found
if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS serializer/protos/pss_message.proto serializer/protos/kv_message.proto)
capnp_generate_cpp(packetSources packetHeaders serializer/capnp/packet.capnp)

set(INCLUDE_DIRECTORY include)
set(INCLUDE_HEADERS ${INCLUDE_DIRECTORY}/ctpl_stl.h)

add_executable(bootstrapper_exe
        bootstrapper/bootstrapper_impl.cpp
        ${packetSources}
        tcp_client_server_connection/tcp_server_connection.cpp
        serializer/serializer.h
        serializer/capnp/capnp_serializer.h
        serializer/capnp/capnp_serializer.cpp
        ${INCLUDE_HEADERS}
        communication/udp_async_server.h
        )
target_link_libraries(bootstrapper_exe PRIVATE CapnProto::capnp)
target_include_directories(bootstrapper_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(bootstrapper_exe PRIVATE ${INCLUDE_DIRECTORY})
target_link_libraries(bootstrapper_exe PUBLIC ${Boost_LIBRARIES})
target_link_libraries(bootstrapper_exe PRIVATE yaml-cpp)

add_executable( peer_exe
        core/peer.cpp
        ${packetSources}
        pss/pss.h
        pss/pss.cpp
        tcp_client_server_connection/tcp_client_connection.cpp
        tcp_client_server_connection/tcp_server_connection.cpp
        serializer/serializer.h
        serializer/capnp/capnp_serializer.h
        serializer/capnp/capnp_serializer.cpp
        pss/pss_listener.cpp
        pss/pss_listener.h
        pss/pss_message.h
        core/peer_data.h
        ${INCLUDE_HEADERS}
        pss/view_logger.h
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        communication/udp_async_server.h
        communication/udp_async_server.cpp
        core/group_construction.cpp
        core/group_construction.h
        data_handlers/data_handler_listener.cpp
        data_handlers/data_handler_listener.h
        store/kv_store.h store/kv_store_key.h
        store/kv_store_memory.cpp
        store/kv_store_memory.h
)
target_link_libraries(peer_exe PRIVATE CapnProto::capnp)
target_link_libraries(peer_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer_exe PRIVATE yaml-cpp)
target_link_libraries(peer_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(peer_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(peer_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(peer_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(peer_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

add_executable( client_exe
        ${packetSources}
        tcp_client_server_connection/tcp_client_connection.cpp
        tcp_client_server_connection/tcp_server_connection.cpp
        serializer/serializer.h
        serializer/capnp/capnp_serializer.h
        serializer/capnp/capnp_serializer.cpp
        pss/pss_message.h
        core/peer_data.h
        ${INCLUDE_HEADERS}
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        communication/udp_async_server.h
        communication/udp_async_server.cpp
        loadbalancing/load_balancer.h
        loadbalancing/dynamic_load_balancer.cpp
        loadbalancing/dynamic_load_balancer.h
        loadbalancing/load_balancer_listener.cpp
        loadbalancing/load_balancer_listener.h
        client/client.cpp
        client/client.h
        client/client_reply_handler.cpp
        client/client_reply_handler.h
        client/main.cpp)

target_link_libraries(client_exe PRIVATE CapnProto::capnp)
target_link_libraries(client_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(client_exe PRIVATE yaml-cpp)
target_link_libraries(client_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(client_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(client_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(client_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(client_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})


