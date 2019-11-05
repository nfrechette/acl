cmake_minimum_required (VERSION 3.2)
project(acl_compressor CXX)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../includes")

if(USE_SJSON)
	include_directories("${PROJECT_SOURCE_DIR}/../../../external/sjson-cpp/includes")
endif()

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../includes/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp
	${PROJECT_SOURCE_DIR}/../*.py
	${PROJECT_SOURCE_DIR}/../*.md)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

setup_default_compiler_flags(${PROJECT_NAME})

if(MSVC)
	if(CPU_INSTRUCTION_SET MATCHES "arm64")
		# Exceptions are not enabled by default for ARM targets, enable them
		target_compile_options(${PROJECT_NAME} PRIVATE /EHsc)
	endif()
endif()

# Abort on failure, easier to debug issues this way
add_definitions(-DACL_ON_ASSERT_ABORT)
add_definitions(-DRTM_ON_ASSERT_ABORT)
add_definitions(-DSJSON_CPP_ON_ASSERT_ABORT)

# Enable SJSON when needed
if(USE_SJSON)
	add_definitions(-DACL_USE_SJSON)
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
