cmake_minimum_required (VERSION 3.2)

# Create groups for the test source files, this creates the proper directory structure under the
# Visual Studio filters
macro(create_source_groups _source_files _relative_directory)
	foreach(_file IN ITEMS ${_source_files})
		get_filename_component(_file_path "${_file}" PATH)
		file(RELATIVE_PATH _file_path_rel "${_relative_directory}" "${_file_path}")
		string(REPLACE "/" "\\" _group_path "${_file_path_rel}")
		string(REPLACE "..\\" "" _group_path "${_group_path}")
		source_group("${_group_path}" FILES "${_file}")
	endforeach()
endmacro()
