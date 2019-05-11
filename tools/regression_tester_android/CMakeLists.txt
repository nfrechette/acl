cmake_minimum_required(VERSION 3.2)
project(acl_regression_tester_android)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_ANDROID_ARCH armv7-a)
set(CMAKE_ANDROID_API_MIN 24)
set(CMAKE_ANDROID_API 24)
set(CMAKE_ANDROID_GUI 1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

set(CMAKE_ANDROID_JAVA_SOURCE_DIR "${PROJECT_SOURCE_DIR}/java")

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../acl_compressor/includes")
include_directories("${PROJECT_SOURCE_DIR}/../acl_compressor/sources")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp/includes")

# Setup resources
set(PROJECT_RESOURCE_DIR "${PROJECT_BINARY_DIR}/res")
file(COPY res DESTINATION ${PROJECT_BINARY_DIR})
file(COPY AndroidManifest.xml DESTINATION ${PROJECT_BINARY_DIR})
file(COPY proguard-android.txt DESTINATION ${PROJECT_BINARY_DIR})

if(EXISTS ${TEST_DATA_DIR})
	set(TEST_DATA_CONFIG_DIR_SRC "${TEST_DATA_DIR}/../configs")
	set(TEST_DATA_DIR_DST "${PROJECT_BINARY_DIR}/assets")
	file(GLOB_RECURSE ALL_TEST_DATA_FILES LIST_DIRECTORIES false ${TEST_DATA_DIR}/*.*)
	file(GLOB_RECURSE ALL_TEST_DATA_CONFIG_FILES LIST_DIRECTORIES false ${TEST_DATA_CONFIG_DIR_SRC}/*.*)
	file(COPY ${ALL_TEST_DATA_FILES} DESTINATION ${TEST_DATA_DIR_DST})
	file(COPY ${ALL_TEST_DATA_CONFIG_FILES} DESTINATION ${TEST_DATA_DIR_DST})
else()
	message(WARNING "Test data directory not found")
endif()

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../acl_compressor/includes/*.h
	${PROJECT_SOURCE_DIR}/../acl_compressor/sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

# Add resource files
set(JAVA_FILES java/com/acl/MainActivity.java)
set(RESOURCE_FILES "${PROJECT_RESOURCE_DIR}/values/strings.xml")
set(ANDROID_FILES "${PROJECT_BINARY_DIR}/AndroidManifest.xml")

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES} ${JAVA_FILES} ${RESOURCE_FILES} ${ANDROID_FILES})

# Enable exceptions
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

# Enable debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE -g)

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)
add_definitions(-DSJSON_CPP_ON_ASSERT_THROW)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DACL_NO_INTRINSICS)
endif()

# Disable allocation tracking since if we fail a regression test, we'll throw an exception
# and fail to free memory, leading to a crash in the allocator
add_definitions(-DACL_NO_ALLOCATOR_TRACKING)

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m log android)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SKIP_ANT_STEP 0)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD 1)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD_CONFIG_PATH proguard-android.txt)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SECURE_PROPS_PATH /definitely/insecure)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_AndroidBuildSystem "AntBuild")				# Use Ant as our build system

set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DIRECTORIES $<TARGET_FILE_DIR:${PROJECT_NAME}>)
set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DEPENDENCIES ${PROJECT_NAME}.so)
