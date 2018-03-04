cmake_minimum_required(VERSION 3.2)
project(acl_compressor)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_ANDROID_ARCH armv7-a)
set(CMAKE_ANDROID_API_MIN 9)
set(CMAKE_ANDROID_API 15)
set(CMAKE_ANDROID_GUI 1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")

set(CMAKE_ANDROID_JAVA_SOURCE_DIR "${PROJECT_SOURCE_DIR}/java")

include_directories("${PROJECT_SOURCE_DIR}/../../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../../external/sjson-cpp-0.3.0/includes")
include_directories("${PROJECT_SOURCE_DIR}/../includes")

# Grab all of our common source files
file(GLOB_RECURSE ALL_COMMON_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/../sources/*.h
	${PROJECT_SOURCE_DIR}/../sources/*.cpp)

create_source_groups("${ALL_COMMON_SOURCE_FILES}" ${PROJECT_SOURCE_DIR}/..)

# Grab all of our main source files
file(GLOB_RECURSE ALL_MAIN_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.cpp)

create_source_groups("${ALL_MAIN_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

# Add resource files
set(JAVA_FILES java/com/acl/MainActivity.java)
set(RESOURCE_FILES res/values/strings.xml)
set(ANDROID_FILES AndroidManifest.xml)

add_executable(${PROJECT_NAME} ${ALL_COMMON_SOURCE_FILES} ${ALL_MAIN_SOURCE_FILES} ${JAVA_FILES} ${RESOURCE_FILES} ${ANDROID_FILES})

target_compile_options(${PROJECT_NAME} PRIVATE -fexceptions)

target_include_directories(${PROJECT_NAME} PUBLIC jni)

target_link_libraries(${PROJECT_NAME} m) # test linking to library by name

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SKIP_ANT_STEP 0)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD 1)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_PROGUARD_CONFIG_PATH proguard-android.txt)
set_target_properties(${PROJECT_NAME} PROPERTIES ANDROID_SECURE_PROPS_PATH /definitely/insecure)

set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DIRECTORIES $<TARGET_FILE_DIR:${PROJECT_NAME}>)
set_property(TARGET ${PROJECT_NAME} PROPERTY ANDROID_NATIVE_LIB_DEPENDENCIES ${PROJECT_NAME}.so)
