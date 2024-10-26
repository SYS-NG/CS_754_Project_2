cmake_minimum_required(VERSION 3.10)
project(grpc_server_client)

set(CMAKE_CXX_STANDARD 14)

# Add this line to help find packages installed by vcpkg
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "$ENV{HOME}/vcpkg/installed/x64-linux")

# Find required packages
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# Explicitly set FUSE3 paths
set(FUSE_INCLUDE_DIR "$ENV{HOME}/vcpkg/installed/x64-linux/include")
set(FUSE_LIBRARY "$ENV{HOME}/vcpkg/installed/x64-linux/lib/libfuse3.a")
# Generate gRPC and Protobuf files
set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/grpc_service.proto")
get_filename_component(PROTO_FILE_ABS "${PROTO_FILE}" ABSOLUTE)
get_filename_component(PROTO_FILE_NAME "${PROTO_FILE}" NAME_WE)

set(GENERATED_PROTOBUF_PATH "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

set(GENERATED_PROTO_SOURCES
    "${GENERATED_PROTOBUF_PATH}/${PROTO_FILE_NAME}.pb.cc"
    "${GENERATED_PROTOBUF_PATH}/${PROTO_FILE_NAME}.pb.h"
    "${GENERATED_PROTOBUF_PATH}/${PROTO_FILE_NAME}.grpc.pb.cc"
    "${GENERATED_PROTOBUF_PATH}/${PROTO_FILE_NAME}.grpc.pb.h"
)

add_custom_command(
    OUTPUT ${GENERATED_PROTO_SOURCES}
    COMMAND $<TARGET_FILE:protobuf::protoc>
    ARGS --grpc_out "${GENERATED_PROTOBUF_PATH}"
         --cpp_out "${GENERATED_PROTOBUF_PATH}"
         -I "${CMAKE_CURRENT_SOURCE_DIR}"
         --plugin=protoc-gen-grpc="$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
         "${PROTO_FILE_ABS}"
    DEPENDS "${PROTO_FILE_ABS}"
)

# Add the executables
add_executable(grpc_server 
    grpc_server.cpp
    ${GENERATED_PROTO_SOURCES}
)

add_executable(grpc_client 
    grpc_client.cpp
    ${GENERATED_PROTO_SOURCES}
)

add_executable(nfs_eval 
    nfs_eval.cpp
)

# Include generated files
target_include_directories(grpc_server PRIVATE ${GENERATED_PROTOBUF_PATH})

target_include_directories(grpc_client PRIVATE ${GENERATED_PROTOBUF_PATH})
target_include_directories(grpc_client PRIVATE ${FUSE_INCLUDE_DIR})

target_include_directories(nfs_eval PRIVATE ${FUSE_INCLUDE_DIR})

# Link against gRPC and Protobuf libraries
target_link_libraries(grpc_server
    PRIVATE gRPC::grpc++
    PRIVATE protobuf::libprotobuf
)

target_link_libraries(grpc_client
    PRIVATE gRPC::grpc++
    PRIVATE protobuf::libprotobuf
    PRIVATE ${FUSE_LIBRARY}
)

target_link_libraries(nfs_eval
    PRIVATE ${FUSE_LIBRARY}
)
