cmake_minimum_required(VERSION 3.15)
project(p2pfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH "/usr/local/lib64/cmake")

find_package(CapnProto CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS system thread serialization REQUIRED)
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
capnp_generate_cpp(packetSources packetHeaders df_serializer/capnp/packet.capnp)

set(INCLUDE_DIRECTORY include)
set(INCLUDE_HEADERS ${INCLUDE_DIRECTORY}/ctpl_stl.h)

add_executable(bootstrapper_exe
        df_bootstrapper/bootstrapper_impl.cpp
        ${packetSources}
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_serializer/serializer.h
        df_serializer/capnp/capnp_serializer.h
        df_serializer/capnp/capnp_serializer.cpp
        ${INCLUDE_HEADERS}
        df_communication/udp_async_server.h
        )
target_link_libraries(bootstrapper_exe PRIVATE CapnProto::capnp)
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
        df_serializer/serializer.h
        df_serializer/capnp/capnp_serializer.h
        df_serializer/capnp/capnp_serializer.cpp
        df_pss/pss_listener.cpp
        df_pss/pss_listener.h
        df_pss/pss_message.h
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
        df_store/kv_store_memory.h
        df_data_handlers/anti_entropy.cpp
        df_data_handlers/anti_entropy.h
        df_core/peer.h
        df_core/peer.cpp
        df_store/kv_store_wiredtiger.h
        exceptions/wiredtiger_error.h
        df_store/kv_store_leveldb.h
        exceptions/leveldb_error.h
        df_util/util.cpp
        df_util/util.h
        df_store/kv_store_memory_v2.h
        )
target_link_libraries(peer_exe PRIVATE CapnProto::capnp)
target_link_libraries(peer_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer_exe PRIVATE yaml-cpp)
target_link_libraries(peer_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(peer_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(peer_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(peer_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(peer_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(peer_exe PRIVATE spdlog::spdlog)
target_link_libraries(peer_exe PUBLIC wiredtiger)
target_link_libraries(peer_exe PUBLIC leveldb)
target_link_libraries(peer_exe PUBLIC stdc++fs) # for creating directories

add_executable( client_exe
        ${packetSources}
        df_tcp_client_server_connection/tcp_client_connection.cpp
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_serializer/serializer.h
        df_serializer/capnp/capnp_serializer.h
        df_serializer/capnp/capnp_serializer.cpp
        df_pss/pss_message.h
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
        df_client/main.cpp exceptions/empty_view_exception.h exceptions/concurrent_writes_same_key.h exceptions/custom_exceptions.h df_client/client_reply_handler_mt.cpp df_client/client_reply_handler_mt.h df_client/client_reply_handler.cpp df_client/client_reply_handler.h)

target_link_libraries(client_exe PRIVATE CapnProto::capnp)
target_link_libraries(client_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(client_exe PRIVATE yaml-cpp)
target_link_libraries(client_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(client_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(client_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(client_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(client_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})


add_executable( lsfs_exe
        ${packetSources}
        df_tcp_client_server_connection/tcp_client_connection.cpp
        df_tcp_client_server_connection/tcp_server_connection.cpp
        df_serializer/serializer.h
        df_serializer/capnp/capnp_serializer.h
        df_serializer/capnp/capnp_serializer.cpp
        df_pss/pss_message.h
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
        fuse/fuse_common/fuse_wrapper.cpp
        fuse/fuse_common/fuse_wrapper.h
        fuse/fuse_lsfs/lsfs_impl.h
        fuse/main.cpp
        fuse/fuse_lsfs/ops_createdelete.cpp
        fuse/fuse_lsfs/ops_dir.cpp
        fuse/fuse_lsfs/ops_file.cpp
        fuse/fuse_lsfs/ops_filesystem.cpp
        fuse/fuse_lsfs/ops_initdestroy.cpp
        fuse/fuse_lsfs/ops_metadata.cpp
        fuse/fuse_lsfs/ops_symlink.cpp
        fuse/fuse_lsfs/ops_xattr.cpp
        fuse/fuse_lsfs/static_checks.cpp
        fuse/fuse_lsfs/util.h
        fuse/fuse_lsfs/util.cpp
        fuse/fuse_common/fuse35.h fuse/fuse_lsfs/lsfs_impl.cpp
        exceptions/empty_view_exception.h
        exceptions/concurrent_writes_same_key.h
        exceptions/custom_exceptions.h
        fuse/fuse_lsfs/metadata.h
        fuse/fuse_lsfs/metadata.cpp
        df_client/client_reply_handler_mt.cpp
        df_client/client_reply_handler_mt.h
        exceptions/timeout_exception.h fuse/fuse_common/fuse_utils.cpp fuse/fuse_common/fuse_utils.h fuse/fuse_lsfs/lsfs_state.cpp fuse/fuse_lsfs/lsfs_state.h df_client/client_reply_handler.cpp df_client/client_reply_handler.h)

target_link_libraries(lsfs_exe PRIVATE CapnProto::capnp)
target_link_libraries(lsfs_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(lsfs_exe PRIVATE yaml-cpp)
target_link_libraries(lsfs_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(lsfs_exe PUBLIC ${Boost_LIBRARIES})
target_link_libraries(lsfs_exe PRIVATE spdlog::spdlog)
target_link_libraries(lsfs_exe PUBLIC fuse3)
target_link_libraries(lsfs_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(lsfs_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(lsfs_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(lsfs_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(lsfs_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)
