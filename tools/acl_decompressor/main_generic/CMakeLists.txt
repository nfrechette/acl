cmake_minimum_required (VERSION 3.2)
project(acl_decompressor CXX)

set(CMAKE_CXX_STANDARD 11)

# Google Benchmark
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "No need to run benchmark's tests" FORCE)
add_subdirectory("${PROJECT_SOURCE_DIR}/../../../external/benchmark" google_benchmark)

include_directories("${PROJECT_SOURCE_DIR}/../../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/benchmark/include")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/sjson-cpp/includes")
include_directories("${PROJECT_SOURCE_DIR}/../sources")

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
    ${PROJECT_SOURCE_DIR}/../sources/*.h
    ${PROJECT_SOURCE_DIR}/../sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp
	${PROJECT_SOURCE_DIR}/../*.py
	${PROJECT_SOURCE_DIR}/../*.md)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

setup_default_compiler_flags(${PROJECT_NAME})

# Link Google Benchmark
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark)

if(MSVC)
	if(CPU_INSTRUCTION_SET MATCHES "arm64")
		# Exceptions are not enabled by default for ARM targets, enable them
		target_compile_options(${PROJECT_NAME} PRIVATE /EHsc)
	endif()
endif()

# Disable allocation tracking
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

# Enable SJSON
add_definitions(-DACL_USE_SJSON)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
