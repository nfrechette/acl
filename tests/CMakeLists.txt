cmake_minimum_required (VERSION 3.9)
project(acl_unit_tests)

include_directories("${PROJECT_SOURCE_DIR}/../includes")
include_directories("${PROJECT_SOURCE_DIR}/external/catch-1.9.6")

# Grab all of our source files
file(GLOB_RECURSE ACL_UNIT_TEST_SOURCE_FILES ${PROJECT_SOURCE_DIR}/sources/*.cpp)

add_executable(acl_unit_tests ${ACL_UNIT_TEST_SOURCE_FILES})

install(TARGETS acl_unit_tests RUNTIME DESTINATION bin)
