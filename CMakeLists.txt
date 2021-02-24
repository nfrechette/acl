cmake_minimum_required (VERSION 3.2)
project(acl NONE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

include(CMakeUtils)
include(CMakeCompiler)
include(CMakePlatforms)

set(USE_AVX_INSTRUCTIONS false CACHE BOOL "Use AVX instructions")
set(USE_POPCNT_INSTRUCTIONS false CACHE BOOL "Use POPCOUNT instructions")
set(USE_SIMD_INSTRUCTIONS true CACHE BOOL "Use SIMD instructions")
set(USE_SJSON true CACHE BOOL "Use SJSON")
set(CPU_INSTRUCTION_SET false CACHE STRING "CPU instruction set")
set(BUILD_BENCHMARK_EXE false CACHE BOOL "Enable the benchmark projects")

set(TEST_DATA_DIR "" CACHE STRING "The full path of the regression test data directory")
set(DECOMP_DATA_DIR "" CACHE STRING "The full path of the decompression data directory")

if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES Debug Release)
	set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

# Determine if we should include unit tests
set(INCLUDE_UNIT_TESTS true)

if(PLATFORM_IOS)
	# Catch2 2.13 doesn't compile on iOS with Xcode 8 and 9
	if(PLATFORM_XCODE_VERSION MATCHES "^8\\.")
		set(INCLUDE_UNIT_TESTS false)
	elseif(PLATFORM_XCODE_VERSION MATCHES "^9\\.")
		set(INCLUDE_UNIT_TESTS false)
	endif()
endif()

# Grab all of our include files
file(GLOB_RECURSE ACL_INCLUDE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/includes/*.h
	${PROJECT_SOURCE_DIR}/docs/*.md
	${PROJECT_SOURCE_DIR}/cmake/*.cmake
	${PROJECT_SOURCE_DIR}/tools/release_scripts/*.py
	)

create_source_groups("${ACL_INCLUDE_FILES}" ${PROJECT_SOURCE_DIR})

file(GLOB ACL_ROOT_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.md
	${PROJECT_SOURCE_DIR}/*.py)

# Add every natvis file
file(GLOB_RECURSE NATVIS_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.natvis)

if(MSVC)
	# With MSVC, the custom target creates a Utility project. Even though it isn't a proper C++ project
	# that compiles into something, MSVC will still parse the code and report intellisense information.
	# A utility project doesn't have an editable field in its properties to add an include directory
	# but adding it anyway like this is enough to make sure the headers are found by intellisense.
	include_directories("${PROJECT_SOURCE_DIR}/includes")
endif()

# Create a dummy target so they show up in the IDE
add_custom_target(${PROJECT_NAME} SOURCES ${ACL_INCLUDE_FILES} ${ACL_ROOT_FILES} ${NATVIS_FILES})

# Enable CTest
enable_testing()

# Add other projects
add_subdirectory("${PROJECT_SOURCE_DIR}/tools/acl_compressor")

if(BUILD_BENCHMARK_EXE)
	add_subdirectory("${PROJECT_SOURCE_DIR}/tools/acl_decompressor")
endif()

if(INCLUDE_UNIT_TESTS)
	add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
endif()

# Custom regression testing (requires SJSON support)
if(USE_SJSON)
	if(PLATFORM_ANDROID)
		add_subdirectory("${PROJECT_SOURCE_DIR}/tools/regression_tester_android")
	elseif(PLATFORM_IOS)
		add_subdirectory("${PROJECT_SOURCE_DIR}/tools/regression_tester_ios")
	endif()
endif()
