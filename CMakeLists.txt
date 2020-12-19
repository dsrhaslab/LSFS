cmake_minimum_required(VERSION 3.15)
project(p2pfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH "/usr/local/lib64/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")

find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS system thread serialization regex REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(spdlog REQUIRED)
find_package(leveldb REQUIRED)

# check if protobuf was found
if(PROTOBUF_FOUND)
    message ("protobuf found")
else()
    message (FATAL_ERROR "Cannot find Protobuf")
endif()

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS df_serializer/protos/pss_message.proto df_serializer/protos/kv_message.proto)

set(INCLUDE_DIRECTORY include)
set(INCLUDE_HEADERS ${INCLUDE_DIRECTORY}/ctpl_stl.h)

add_executable(bootstrapper_exe
        df_bootstrapper/bootstrapper_impl.cpp
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        ${packetSources}
        df_tcp_client_server_connection/tcp_server_connection.cpp
        ${INCLUDE_HEADERS}
        df_communication/udp_async_server.h
        exceptions/peer_disconnected.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        exceptions/peer_disconnected.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        df_util/randomizer.h
        )

target_link_libraries(bootstrapper_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_include_directories(bootstrapper_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(bootstrapper_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(bootstrapper_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(bootstrapper_exe PRIVATE ${INCLUDE_DIRECTORY})
target_link_libraries(bootstrapper_exe PUBLIC ${Boost_LIBRARIES})
target_link_libraries(bootstrapper_exe PRIVATE yaml-cpp)

add_executable( peer_exe
        ${packetSources}
        df_pss/pss.h
        df_pss/pss.cpp
        df_tcp_client_server_connection/tcp_client_connection.cpp
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_pss/pss_listener.cpp
        df_pss/pss_listener.h
        df_core/peer_data.h
        ${INCLUDE_HEADERS}
        df_pss/view_logger.h
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        df_communication/udp_async_server.h
        df_communication/udp_async_server.cpp
        df_core/group_construction.cpp
        df_core/group_construction.h
        df_data_handlers/data_handler_listener.cpp
        df_data_handlers/data_handler_listener.h
        df_store/kv_store.h df_store/kv_store_key.h
        df_data_handlers/anti_entropy.cpp
        df_data_handlers/anti_entropy.h
        df_core/peer.h
        df_core/peer.cpp
        df_store/kv_store_leveldb.h
        exceptions/leveldb_error.h
        df_util/util.cpp
        df_util/util.h
        df_store/kv_store_key_version.h

        #necessary for merge function
        lsfs/fuse_lsfs/storage_specific.cpp
        lsfs/fuse_lsfs/metadata.h
        lsfs/fuse_lsfs/metadata.cpp
        lsfs/fuse_lsfs/util.h
        lsfs/fuse_lsfs/util.cpp
        df_data_handlers/data_handler_listener_mt.cpp
        df_data_handlers/data_handler_listener_mt.h
        df_data_handlers/data_handler_listener_st.cpp
        df_data_handlers/data_handler_listener_st.h
        exceptions/peer_disconnected.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        exceptions/peer_disconnected.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        df_util/randomizer.h
        )

target_link_libraries(peer_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer_exe PRIVATE yaml-cpp)
target_link_libraries(peer_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(peer_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(peer_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(peer_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(peer_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(peer_exe PRIVATE spdlog::spdlog)
target_link_libraries(peer_exe PUBLIC leveldb)
target_link_libraries(peer_exe PUBLIC stdc++fs) # for creating directories
target_compile_definitions(peer_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)

add_executable( client_exe
        ${packetSources}
        df_tcp_client_server_connection/tcp_client_connection.cpp
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_core/peer_data.h
        ${INCLUDE_HEADERS}
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        df_communication/udp_async_server.h
        df_communication/udp_async_server.cpp
        df_loadbalancing/load_balancer.h
        df_loadbalancing/dynamic_load_balancer.cpp
        df_loadbalancing/dynamic_load_balancer.h
        df_loadbalancing/load_balancer_listener.cpp
        df_loadbalancing/load_balancer_listener.h
        df_client/client.cpp
        df_client/client.h
        df_client/client_reply_handler_st.cpp
        df_client/client_reply_handler_st.h
        df_client/main.cpp
        exceptions/empty_view_exception.h
        exceptions/concurrent_writes_same_key.h
        exceptions/custom_exceptions.h
        df_client/client_reply_handler_mt.cpp
        df_client/client_reply_handler_mt.h
        df_client/client_reply_handler.cpp
        df_client/client_reply_handler.h
        df_store/kv_store_key_version.h

        #necessary for merge function
        lsfs/fuse_lsfs/storage_specific.cpp
        lsfs/fuse_lsfs/metadata.h
        lsfs/fuse_lsfs/metadata.cpp
        lsfs/fuse_lsfs/util.h
        lsfs/fuse_lsfs/util.cpp
        df_loadbalancing/smart_load_balancer.cpp
        df_loadbalancing/smart_load_balancer.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        exceptions/peer_disconnected.h
        exceptions/socket_exception.h
        exceptions/socket_read_exception.h
        exceptions/socket_send_exception.h
        df_util/randomizer.h
        )

target_link_libraries(client_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(client_exe PRIVATE yaml-cpp)
target_link_libraries(client_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(client_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(client_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(client_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(client_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(client_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)

add_executable( lsfs_exe
        ${packetSources}
        df_tcp_client_server_connection/tcp_client_connection.cpp
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_core/peer_data.h
        ${INCLUDE_HEADERS}
        ${PROTO_SRCS}
        ${PROTO_HDRS}
        df_communication/udp_async_server.h
        df_communication/udp_async_server.cpp
        df_loadbalancing/load_balancer.h
        df_loadbalancing/dynamic_load_balancer.cpp
        df_loadbalancing/dynamic_load_balancer.h
        df_loadbalancing/load_balancer_listener.cpp
        df_loadbalancing/load_balancer_listener.h
        df_client/client.cpp
        df_client/client.h
        df_client/client_reply_handler_st.cpp
        df_client/client_reply_handler_st.h
        lsfs/fuse_common/fuse_wrapper.cpp
        lsfs/fuse_common/fuse_wrapper.h
        lsfs/fuse_lsfs/lsfs_impl.h
        lsfs/main.cpp
        lsfs/fuse_lsfs/ops_createdelete.cpp
        lsfs/fuse_lsfs/ops_dir.cpp
        lsfs/fuse_lsfs/ops_file.cpp
        lsfs/fuse_lsfs/ops_filesystem.cpp
        lsfs/fuse_lsfs/ops_initdestroy.cpp
        lsfs/fuse_lsfs/ops_metadata.cpp
        lsfs/fuse_lsfs/ops_symlink.cpp
        lsfs/fuse_lsfs/util.h
        lsfs/fuse_lsfs/util.cpp
        lsfs/fuse_common/fuse35.h lsfs/fuse_lsfs/lsfs_impl.cpp
        exceptions/empty_view_exception.h
        exceptions/concurrent_writes_same_key.h
        exceptions/custom_exceptions.h
        lsfs/fuse_lsfs/metadata.h
        lsfs/fuse_lsfs/metadata.cpp
        df_client/client_reply_handler_mt.cpp
        df_client/client_reply_handler_mt.h
        exceptions/timeout_exception.h
        lsfs/fuse_common/fuse_utils.cpp
        lsfs/fuse_common/fuse_utils.h
        lsfs/fuse_lsfs/lsfs_state.cpp
        lsfs/fuse_lsfs/lsfs_state.h
        df_client/client_reply_handler.cpp
        df_client/client_reply_handler.h
        lsfs/fuse_lsfs/storage_specific.cpp
        df_loadbalancing/smart_load_balancer.cpp
        df_loadbalancing/smart_load_balancer.h
        lsfs/fuse_common/macros.h
        df_util/randomizer.h
        )

target_link_libraries(lsfs_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(lsfs_exe PRIVATE yaml-cpp)
target_link_libraries(lsfs_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(lsfs_exe PUBLIC ${Boost_LIBRARIES})
target_link_libraries(lsfs_exe PRIVATE spdlog::spdlog)
target_link_libraries(lsfs_exe PUBLIC fuse3)
target_link_libraries(lsfs_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(lsfs_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(lsfs_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(lsfs_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(lsfs_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)
