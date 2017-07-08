cmake_minimum_required (VERSION 3.9)
project(acl_compressor)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")

# Grab all of our source files
file(GLOB_RECURSE ACL_COMPRESSOR_SOURCE_FILES
	${PROJECT_SOURCE_DIR}/sources/*.cpp
	${PROJECT_SOURCE_DIR}/*.py)

add_executable(acl_compressor ${ACL_COMPRESSOR_SOURCE_FILES})

install(TARGETS acl_compressor RUNTIME DESTINATION bin)
