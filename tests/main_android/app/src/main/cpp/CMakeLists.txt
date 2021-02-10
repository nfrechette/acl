cmake_minimum_required(VERSION 3.2)
project(acl_unit_tests CXX)

set(CMAKE_CXX_STANDARD 11)

# Project root is <acl-dir>\tests\main_android
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}/../../../..")

include_directories("${PROJECT_ROOT_DIR}/../../includes")
include_directories("${PROJECT_ROOT_DIR}/../../external/rtm/includes")
include_directories("${PROJECT_ROOT_DIR}/../../external/catch2/single_include")

if(USE_SJSON)
	include_directories("${PROJECT_ROOT_DIR}/../../external/sjson-cpp/includes")
endif()

# Grab all of our test source files
file(GLOB_RECURSE ALL_TEST_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_ROOT_DIR}/../sources/*.h
	${PROJECT_ROOT_DIR}/../sources/*.cpp)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

add_library(${PROJECT_NAME} SHARED ${ALL_TEST_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

# Enable exceptions
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

# Enable debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE -g)

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DRTM_ON_ASSERT_THROW)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Enable SJSON when needed
if(USE_SJSON)
	add_definitions(-DACL_USE_SJSON)
	add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m log)
