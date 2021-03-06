cmake_minimum_required (VERSION 3.2)
project(acl_decompressor)

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "No need to run benchmark's tests" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/../../../external/benchmark" google_benchmark)

# iOS cmake toolchain does not support CMAKE_CXX_STANDARD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Force enable debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# Enable optimizations in Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

set(MACOSX_BUNDLE_EXECUTABLE_NAME ${PROJECT_NAME})
set(MACOSX_BUNDLE_INFO_STRING "com.acl.acl-decompressor")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.acl.acl-decompressor")
set(MACOSX_BUNDLE_BUNDLE_NAME "acl-decompressor")

include_directories("${PROJECT_SOURCE_DIR}/../../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../sources")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/sjson-cpp/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/benchmark/include")

# Setup resources
if(EXISTS ${DECOMP_DATA_DIR})
	set(DECOMP_DATA_DIR_DST "${PROJECT_BINARY_DIR}/assets")
	file(GLOB_RECURSE ALL_DECOMP_DATA_FILES LIST_DIRECTORIES false ${DECOMP_DATA_DIR}/*.*)
	file(COPY ${ALL_DECOMP_DATA_FILES} DESTINATION ${DECOMP_DATA_DIR_DST})

	file(GLOB_RECURSE ALL_DECOMP_DATA_RESOURCE_FILES LIST_DIRECTORIES false ${DECOMP_DATA_DIR_DST}/*.*)
	set_source_files_properties(${ALL_DECOMP_DATA_RESOURCE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
else()
	message(WARNING "Decompression data directory not found")
endif()

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../sources/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES} ${ALL_DECOMP_DATA_RESOURCE_FILES})

# Link Google Benchmark
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark)

# Disable allocation tracking
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

# Enable SJSON
add_definitions(-DACL_USE_SJSON)

target_link_libraries(${PROJECT_NAME} PRIVATE "-framework CoreFoundation")

set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/Info.plist.in)
