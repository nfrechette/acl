cmake_minimum_required (VERSION 3.2)
project(acl_unit_tests)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../includes")
include_directories("${PROJECT_SOURCE_DIR}/../external/catch-1.9.6")

# Grab all of our source files
file(GLOB_RECURSE ACL_UNIT_TEST_SOURCE_FILES LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/sources/*.h
	${PROJECT_SOURCE_DIR}/sources/*.cpp)

# Create groups for the source files, this creates the proper directory structure under the
# Visual Studio filters
foreach(_file IN ITEMS ${ACL_UNIT_TEST_SOURCE_FILES})
	get_filename_component(_file_path "${_file}" PATH)
	file(RELATIVE_PATH _file_path_rel "${PROJECT_SOURCE_DIR}" "${_file_path}")
	string(REPLACE "/" "\\" _group_path "${_file_path_rel}")
	source_group("${_group_path}" FILES "${_file}")
endforeach()

add_executable(acl_unit_tests ${ACL_UNIT_TEST_SOURCE_FILES})
add_test(NAME UNIT COMMAND acl_unit_tests)

if(MSVC)
	# Replace some default compiler switches and add new ones
	STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Disable RTTI
	STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})	# Bump warnings to W4
	target_compile_options(acl_unit_tests PRIVATE /Zi)				# Add debug info
	target_compile_options(acl_unit_tests PRIVATE /Oi)				# Generate intrinsic functions

	# Disable some warnings
	target_compile_options(acl_unit_tests PRIVATE /wd4100)			# unreferenced formal parameter
	target_compile_options(acl_unit_tests PRIVATE /wd4324)			# structure was padded due to alignment specified
	target_compile_options(acl_unit_tests PRIVATE /wd4127)			# conditional expression is constant

	if(MSVC_VERSION GREATER 1900)
		# VS2017 and above
		target_compile_options(acl_unit_tests PRIVATE /permissive-)
	endif()

	if(USE_AVX_INSTRUCTIONS)
		target_compile_options(acl_unit_tests PRIVATE "/arch:AVX")
	endif()

	# Add linker flags
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -STACK:2097152")
else()
	if(CPU_INSTRUCTION_SET MATCHES "x86")
		target_compile_options(acl_unit_tests PRIVATE "-m32")
		target_link_libraries(acl_unit_tests PRIVATE "-m32")
	elseif(CPU_INSTRUCTION_SET MATCHES "x64")
		target_compile_options(acl_unit_tests PRIVATE "-m64")
		target_link_libraries(acl_unit_tests PRIVATE "-m64")
	endif()

	if(CPU_INSTRUCTION_SET MATCHES "x86" OR CPU_INSTRUCTION_SET MATCHES "x64")
		if(USE_AVX_INSTRUCTIONS)
			target_compile_options(acl_unit_tests PRIVATE "-mavx")
		else()
			target_compile_options(acl_unit_tests PRIVATE "-msse4.1")
		endif()
	endif()
endif()

install(TARGETS acl_unit_tests RUNTIME DESTINATION bin)
