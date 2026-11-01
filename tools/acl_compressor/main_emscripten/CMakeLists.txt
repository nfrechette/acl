project(acl_compressor CXX)

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

if(USE_SIMD_INSTRUCTIONS)
	target_compile_options(${PROJECT_NAME} PRIVATE -msimd128)		# Enable WASM SIMD
else()
	add_definitions(-DRTM_NO_INTRINSICS)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)		# Enable all warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Wshadow)			# Enable shadowing warnings
target_compile_options(${PROJECT_NAME} PRIVATE -Werror)				# Treat warnings as errors

# Exceptions are not enabled by default, enable native WebAssembly exception handling
target_compile_options(${PROJECT_NAME} PRIVATE -fwasm-exceptions)
target_link_libraries(${PROJECT_NAME} -fwasm-exceptions)

target_link_libraries(${PROJECT_NAME} "-s NODERAWFS=1")				# Enable the raw node file system

target_link_libraries(${PROJECT_NAME} "-s ENVIRONMENT=node")		# Force the environment to node

target_link_libraries(${PROJECT_NAME} "-s ALLOW_MEMORY_GROWTH=1")	# Allow dynamic memory allocation
target_link_libraries(${PROJECT_NAME} "-s STACK_SIZE=5MB")			# The 64 KB default is too small for the debug tests

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.js
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.wasm
	DESTINATION bin)
