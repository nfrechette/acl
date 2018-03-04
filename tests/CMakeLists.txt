cmake_minimum_required (VERSION 3.2)
project(acl_unit_tests_root)

if(PLATFORM_WINDOWS OR PLATFORM_OSX OR PLATFORM_LINUX)
	add_subdirectory("${PROJECT_SOURCE_DIR}/main_windows")
elseif(PLATFORM_ANDROID)
	add_subdirectory("${PROJECT_SOURCE_DIR}/main_android")
endif()
