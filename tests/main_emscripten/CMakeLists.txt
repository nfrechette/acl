cmake_minimum_required (VERSION 3.2)
project(acl_unit_tests CXX)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/rtm/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp/includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/catch2/single_include")
include_directories("${PROJECT_SOURCE_DIR}/../sources")

# Grab all of our test source files
file(GLOB_RECURSE ALL_TEST_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../sources/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${ALL_TEST_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DRTM_ON_ASSERT_THROW)
add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)

# Enable sjson-cpp usage to test IO
add_definitions(-DACL_USE_SJSON)

# Enable ANSI heap debug checks
add_definitions(-DACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS)
add_definitions(-DACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)		# Enable all warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Wshadow)			# Enable shadowing warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Werror)				# Treat warnings as errors

# Exceptions are not enabled by default, enable them
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)
target_link_libraries(${PROJECT_NAME} PRIVATE "-s DISABLE_EXCEPTION_CATCHING=0")

target_link_libraries(${PROJECT_NAME} PRIVATE "-s NODERAWFS=1")				# Enable the raw node file system
target_link_libraries(${PROJECT_NAME} PRIVATE -lnodefs.js)					# Link the node file system

target_link_libraries(${PROJECT_NAME} PRIVATE "-s ENVIRONMENT=node")		# Force the environment to node

target_link_libraries(${PROJECT_NAME} PRIVATE "-s ALLOW_MEMORY_GROWTH=1")	# Allow dynamic memory allocation

# Setup Catch2 so we can find and execute the unit tests with CTest
set(OptionalCatchTestLauncher node)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/../../external/catch2/contrib")
include(CTest)
include(ParseAndAddCatchTests)
ParseAndAddCatchTests(${PROJECT_NAME})

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.js
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.wasm
	DESTINATION bin)
