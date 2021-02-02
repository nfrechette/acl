cmake_minimum_required (VERSION 3.2)
project(acl_unit_tests CXX)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/catch2/single_include")

# Grab all of our test source files
file(GLOB_RECURSE ALL_TEST_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../sources/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp)

create_source_groups("${ALL_TEST_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ALL_TEST_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../../external/catch2/contrib")
include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME})

setup_default_compiler_flags(${PROJECT_NAME})

if(MSVC)
	if(CPU_INSTRUCTION_SET MATCHES "arm64")
		# Exceptions are not enabled by default for ARM targets, enable them
		target_compile_options(${PROJECT_NAME} PRIVATE /EHsc)
	endif()
endif()

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DRTM_ON_ASSERT_THROW)
add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)

add_definitions(-DACL_USE_SJSON)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
