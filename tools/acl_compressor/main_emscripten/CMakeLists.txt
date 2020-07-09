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

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES})

# Abort on failure, easier to debug issues this way
add_definitions(-DACL_ON_ASSERT_ABORT)
add_definitions(-DRTM_ON_ASSERT_ABORT)
add_definitions(-DSJSON_CPP_ON_ASSERT_ABORT)

# Enable SJSON when needed
if(USE_SJSON)
	add_definitions(-DACL_USE_SJSON)
endif()

# Remove '-g' from compilation flags since it sometimes crashes the compiler
string(REPLACE "-g" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
string(REPLACE "-g" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)		# Enable all warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Wshadow)			# Enable shadowing warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Werror)				# Treat warnings as errors

# Exceptions are not enabled by default, enable them
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)
target_link_libraries(${PROJECT_NAME} "-s DISABLE_EXCEPTION_CATCHING=0")

target_link_libraries(${PROJECT_NAME} "-s NODERAWFS=1")				# Enable the raw node file system
target_link_libraries(${PROJECT_NAME} -lnodefs.js)					# Link the node file system

target_link_libraries(${PROJECT_NAME} "-s ENVIRONMENT=node")		# Force the environment to node

target_link_libraries(${PROJECT_NAME} "-s ALLOW_MEMORY_GROWTH=1")	# Allow dynamic memory allocation

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.js
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.wasm
	DESTINATION bin)
