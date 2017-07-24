cmake_minimum_required (VERSION 3.9)
project(acl)

set(USE_AVX_INSTRUCTIONS false CACHE BOOL "Use AVX instructions")

# Grab all of our include files
file(GLOB_RECURSE ACL_INCLUDE_FILES LIST_DIRECTORIES false ${PROJECT_SOURCE_DIR}/includes/*.h)

# Create a dummy target so they show up in the IDE
add_custom_target(acl SOURCES ${ACL_INCLUDE_FILES})

# Create groups for the source files, this creates the proper directory structure under the
# Visual Studio filters
foreach(_file IN ITEMS ${ACL_INCLUDE_FILES})
	get_filename_component(_file_path "${_file}" PATH)
	file(RELATIVE_PATH _file_path_rel "${PROJECT_SOURCE_DIR}" "${_file_path}")
	string(REPLACE "/" "\\" _group_path "${_file_path_rel}")
	source_group("${_group_path}" FILES "${_file}")
endforeach()

# Add other projects
add_subdirectory("${PROJECT_SOURCE_DIR}/tools/acl_compressor")
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
