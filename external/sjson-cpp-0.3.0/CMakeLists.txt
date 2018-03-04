cmake_minimum_required (VERSION 3.2)
project(sjson-cpp)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

include(CMakeUtils)
include(CMakeCompiler)
include(CMakePlatforms)

set(CPU_INSTRUCTION_SET false CACHE STRING "CPU instruction set")

# Grab all of our include files
file(GLOB_RECURSE SJSON_CPP_INCLUDE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/includes/*.h
	${PROJECT_SOURCE_DIR}/*.md
	${PROJECT_SOURCE_DIR}/cmake/*.cmake
	${PROJECT_SOURCE_DIR}/tools/vs_visualizers/*.natvis)

create_source_groups("${SJSON_CPP_INCLUDE_FILES}" ${PROJECT_SOURCE_DIR})

file(GLOB SJSON_CPP_ROOT_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/*.md
	${PROJECT_SOURCE_DIR}/*.py)

# Create a dummy target so they show up in the IDE
add_custom_target(sjson-cpp SOURCES ${SJSON_CPP_INCLUDE_FILES} ${SJSON_CPP_ROOT_FILES})

if(CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_CONFIGURATION_TYPES Debug Release)
	set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Reset the configurations to what we need" FORCE)
endif()

enable_testing()

# Add other projects
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
