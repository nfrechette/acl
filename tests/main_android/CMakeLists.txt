cmake_minimum_required(VERSION 3.2)
project(acl_unit_tests)

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
include_directories("${PROJECT_SOURCE_DIR}/../../external/catch2/single_include/catch2")

# Grab all of our test source files
file(GLOB_RECURSE ALL_TEST_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../sources/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp)

create_source_groups("${ALL_TEST_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

# Add resource files
set(JAVA_FILES java/com/acl/MainActivity.java)
set(RESOURCE_FILES res/values/strings.xml)
set(ANDROID_FILES AndroidManifest.xml)

add_executable(${PROJECT_NAME} ${ALL_TEST_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES} ${JAVA_FILES} ${RESOURCE_FILES} ${ANDROID_FILES})

# Enable exceptions
target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

# Enable debug symbols
target_compile_options(${PROJECT_NAME} PRIVATE -g)

# Throw on failure to allow us to catch them and recover
add_definitions(-DACL_ON_ASSERT_THROW)

# Disable SIMD if not needed
if(NOT USE_SIMD_INSTRUCTIONS)
	add_definitions(-DACL_NO_INTRINSICS)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m)

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SKIP_ANT_STEP 0)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD 1)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD_CONFIG_PATH proguard-android.txt)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SECURE_PROPS_PATH /definitely/insecure)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_AndroidBuildSystem "AntBuild")				# Use Ant as our build system

set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DIRECTORIES $<TARGET_FILE_DIR:${PROJECT_NAME}>)
set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DEPENDENCIES ${PROJECT_NAME}.so)
