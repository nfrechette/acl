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
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp-0.3.0/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../tests/sources")

# Setup resources
set(TEST_DATA_DIR_SRC "${PROJECT_SOURCE_DIR}/../../test_data/test_data_v1")
set(TEST_DATA_CONFIG_DIR_SRC "${PROJECT_SOURCE_DIR}/../../test_data/configs")
set(TEST_DATA_DIR_DST "${PROJECT_BINARY_DIR}/assets")
file(GLOB_RECURSE ALL_TEST_DATA_FILES LIST_DIRECTORIES false
	${TEST_DATA_DIR_SRC}/*.*)
file(GLOB_RECURSE ALL_TEST_DATA_CONFIG_FILES LIST_DIRECTORIES false
	${TEST_DATA_CONFIG_DIR_SRC}/*.*)
file(COPY ${ALL_TEST_DATA_FILES} DESTINATION ${TEST_DATA_DIR_DST})
file(COPY ${ALL_TEST_DATA_CONFIG_FILES} DESTINATION ${TEST_DATA_DIR_DST})

file(GLOB_RECURSE ALL_TEST_DATA_RESOURCE_FILES LIST_DIRECTORIES false
	${TEST_DATA_DIR_DST}/*.*)
set_source_files_properties(${ALL_TEST_DATA_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${ALL_MAIN_SOURCE_FILES} ${ALL_TEST_DATA_RESOURCE_FILES})

target_link_libraries(${PROJECT_NAME} "-framework CoreFoundation")
