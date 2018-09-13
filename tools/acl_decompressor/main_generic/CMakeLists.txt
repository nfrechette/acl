cmake_minimum_required (VERSION 3.2)
project(acl_decompressor CXX)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../acl_compressor/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../acl_compressor/sources")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/sjson-cpp/includes")

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../../acl_compressor/includes/*.h
	${PROJECT_SOURCE_DIR}/../../acl_compressor/sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

setup_default_compiler_flags(${PROJECT_NAME})

# Disable allocation tracking
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
