cmake_minimum_required (VERSION 3.2)
project(acl_compressor)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp-0.3.0/includes")

# Grab all of our source files
file(GLOB_RECURSE ACL_SOURCE_FILES
	${PROJECT_SOURCE_DIR}/sources/*.cpp
	${PROJECT_SOURCE_DIR}/*.py)

create_source_groups("${ACL_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ACL_SOURCE_FILES})

setup_default_compiler_flags(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
