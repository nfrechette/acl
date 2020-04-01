cmake_minimum_required(VERSION 3.2)
project(acl_regression_tester_android CXX)

set(CMAKE_CXX_STANDARD 11)

# Project root is <acl-dir>\tools\regression_tester_android
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

include_directories("${PROJECT_ROOT_DIR}/../../includes")
include_directories("${PROJECT_ROOT_DIR}/../../external/rtm/includes")
include_directories("${PROJECT_ROOT_DIR}/../../external/sjson-cpp/includes")
include_directories("${PROJECT_ROOT_DIR}/../acl_compressor/includes")
include_directories("${PROJECT_ROOT_DIR}/../acl_compressor/sources")

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_ROOT_DIR}/../acl_compressor/includes/*.h
	${PROJECT_ROOT_DIR}/../acl_compressor/sources/*.cpp)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

add_library(${PROJECT_NAME} SHARED ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

# Enable exceptions
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

# Enable debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE -g)

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DRTM_ON_ASSERT_THROW)
add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Disable allocation tracking since if we fail a regression test, we'll throw an exception
# and fail to free memory, leading to a crash in the allocator
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

# Enable SJSON
add_definitions(-DACL_USE_SJSON)

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m log android)
