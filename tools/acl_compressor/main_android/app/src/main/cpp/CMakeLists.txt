cmake_minimum_required(VERSION 3.2)
project(acl_compressor CXX)

set(CMAKE_CXX_STANDARD 11)

# Project root is <acl-dir>\tools\compressor\main_android
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

include_directories("${PROJECT_ROOT_DIR}/../../../includes")
include_directories("${PROJECT_ROOT_DIR}/../../../external/rtm/includes")
include_directories("${PROJECT_ROOT_DIR}/../includes")

if(USE_SJSON)
	include_directories("${PROJECT_ROOT_DIR}/../../../external/sjson-cpp/includes")
endif()

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

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Enable SJSON when needed
if(USE_SJSON)
	add_definitions(-DACL_USE_SJSON)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m log)
