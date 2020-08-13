cmake_minimum_required(VERSION 3.2)
project(acl_decompressor CXX)

set(CMAKE_CXX_STANDARD 11)

# Project root is <acl-dir>\tools\decompressor\main_android
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "No need to run benchmark's tests" FORCE)
add_subdirectory("${PROJECT_ROOT_DIR}/../../../external/benchmark" google_benchmark)

include_directories("${PROJECT_ROOT_DIR}/../../../includes")
include_directories("${PROJECT_ROOT_DIR}/../../../external/benchmark/include")
include_directories("${PROJECT_ROOT_DIR}/../../../external/rtm/includes")
include_directories("${PROJECT_ROOT_DIR}/../../../external/sjson-cpp/includes")
include_directories("${PROJECT_ROOT_DIR}/../sources")

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_ROOT_DIR}/../sources/*.h
	${PROJECT_ROOT_DIR}/../sources/*.cpp)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

add_library(${PROJECT_NAME} SHARED ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

# Enable exceptions
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

# Enable debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE -g)

# Disable allocation tracking
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Enable SJSON
add_definitions(-DACL_USE_SJSON)

target_include_directories(${PROJECT_NAME} PUBLIC jni)

# Link system libraries
target_link_libraries(${PROJECT_NAME} m log android)

# Link Google Benchmark
target_link_libraries(${PROJECT_NAME} benchmark)
