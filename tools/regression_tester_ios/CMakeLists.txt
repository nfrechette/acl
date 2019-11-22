cmake_minimum_required(VERSION 3.2)
project(acl_regression_tester_ios)

# iOS cmake toolchain does not support CMAKE_CXX_STANDARD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Force enable debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Enable optimizations in Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set(MACOSX_BUNDLE_EXECUTABLE_NAME ${PROJECT_NAME})
set(MACOSX_BUNDLE_INFO_STRING "com.acl.acl-regression-tester")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.acl.acl-regression-tester")
set(MACOSX_BUNDLE_BUNDLE_NAME "acl-regression-tester")

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../acl_compressor/includes")
include_directories("${PROJECT_SOURCE_DIR}/../acl_compressor/sources")
include_directories("${PROJECT_SOURCE_DIR}/../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp/includes")

# Setup resources
if(EXISTS ${TEST_DATA_DIR})
	set(TEST_DATA_CONFIG_DIR_SRC "${TEST_DATA_DIR}/../configs")
	set(TEST_DATA_DIR_DST "${PROJECT_BINARY_DIR}/assets")
	file(GLOB_RECURSE ALL_TEST_DATA_FILES LIST_DIRECTORIES false ${TEST_DATA_DIR}/*.*)
	file(GLOB_RECURSE ALL_TEST_DATA_CONFIG_FILES LIST_DIRECTORIES false ${TEST_DATA_CONFIG_DIR_SRC}/*.*)
	file(COPY ${ALL_TEST_DATA_FILES} DESTINATION ${TEST_DATA_DIR_DST})
	file(COPY ${ALL_TEST_DATA_CONFIG_FILES} DESTINATION ${TEST_DATA_DIR_DST})

	file(GLOB_RECURSE ALL_TEST_DATA_RESOURCE_FILES LIST_DIRECTORIES false ${TEST_DATA_DIR_DST}/*.*)
	set_source_files_properties(${ALL_TEST_DATA_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
else()
	message(WARNING "Test data directory not found")
endif()

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../acl_compressor/includes/*.h
	${PROJECT_SOURCE_DIR}/../acl_compressor/sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${ALL_MAIN_SOURCE_FILES} ${ALL_COMMON_SOURCE_FILES} ${ALL_TEST_DATA_RESOURCE_FILES})

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DRTM_ON_ASSERT_THROW)
add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)

# Disable allocation track since if we fail a regression test, we'll throw an exception
# and fail to free memory, leading to a crash in the allocator
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Enable SJSON
add_definitions(-DACL_USE_SJSON)

target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation")
