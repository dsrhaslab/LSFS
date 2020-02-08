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
find_package(spdlog REQUIRED)

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
        df_core/peer.cpp
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
        df_store/kv_store_memory.cpp
        df_store/kv_store_memory.h
        df_data_handlers/anti_entropy.cpp df_data_handlers/anti_entropy.h)
target_link_libraries(peer_exe PRIVATE CapnProto::capnp)
target_link_libraries(peer_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(peer_exe PRIVATE yaml-cpp)
target_link_libraries(peer_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(peer_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(peer_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(peer_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(peer_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(peer_exe PRIVATE spdlog::spdlog)

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
        df_client/client_reply_handler.cpp
        df_client/client_reply_handler.h
        df_client/main.cpp)

target_link_libraries(client_exe PRIVATE CapnProto::capnp)
target_link_libraries(client_exe PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(client_exe PRIVATE yaml-cpp)
target_link_libraries(client_exe PUBLIC ${PROTOBUF_LIBRARIES})
target_link_libraries(client_exe PUBLIC ${Boost_LIBRARIES})
target_include_directories(client_exe PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(client_exe PRIVATE ${INCLUDE_DIRECTORY})
target_include_directories(client_exe PUBLIC ${PROTOBUF_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})


add_executable( lsfs_exe
        fuse/fuse.cpp
        fuse/fuse.h
        fuse/lsfs_impl.cpp
        fuse/lsfs_impl.h
        fuse/lsfs.cpp
#        fuse/fuse_ram_fs/inode.cpp
#        fuse/fuse_ram_fs/inode.h
#        fuse/fuse_ram_fs/directory.cpp
#        fuse/fuse_ram_fs/directory.h
#        fuse/fuse_ram_fs/file.cpp
#        fuse/fuse_ram_fs/file.h
#        fuse/fuse_ram_fs/special_inode.cpp
#        fuse/fuse_ram_fs/special_inode.h
#        fuse/fuse_ram_fs/symlink.cpp
#        fuse/fuse_ram_fs/symlink.h
        fuse/passthrough_helpers.h)

target_compile_definitions(lsfs_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)
target_link_libraries(lsfs_exe fuse)

add_executable( lsfs_haslab_exe
        fuse/passthrough_haslab/main.cpp
        fuse/passthrough_haslab/fuse31.h
        fuse/passthrough_haslab/ops.h
        fuse/passthrough_haslab/ops.cpp
        fuse/passthrough_haslab/ops_createdelete.h
        fuse/passthrough_haslab/ops_createdelete.cpp
        fuse/passthrough_haslab/ops_dir.h
        fuse/passthrough_haslab/ops_dir.cpp
        fuse/passthrough_haslab/ops_file.h
        fuse/passthrough_haslab/ops_file.cpp
        fuse/passthrough_haslab/ops_filesystem.h
        fuse/passthrough_haslab/ops_filesystem.cpp
        fuse/passthrough_haslab/ops_initdestroy.h
        fuse/passthrough_haslab/ops_initdestroy.cpp
        fuse/passthrough_haslab/ops_metadata.h
        fuse/passthrough_haslab/ops_metadata.cpp
        fuse/passthrough_haslab/ops_symlink.h
        fuse/passthrough_haslab/ops_symlink.cpp
        fuse/passthrough_haslab/ops_xattr.h
        fuse/passthrough_haslab/ops_xattr.cpp
        fuse/passthrough_haslab/static_checks.cpp
        fuse/passthrough_haslab/util.h
        fuse/passthrough_haslab/util.cpp
        )

target_compile_definitions(lsfs_haslab_exe PRIVATE FUSE_USE_VERSION=35 _FILE_OFFSET_BITS=64)
target_link_libraries(lsfs_haslab_exe fuse3)