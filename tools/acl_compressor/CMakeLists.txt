cmake_minimum_required (VERSION 3.2)
project(acl_compressor)

set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")
include_directories("${PROJECT_SOURCE_DIR}/../../external/sjson-cpp-0.2.0/includes")

# Grab all of our source files
file(GLOB_RECURSE ACL_SOURCE_FILES
	${PROJECT_SOURCE_DIR}/sources/*.cpp
	${PROJECT_SOURCE_DIR}/*.py)

create_source_groups("${ACL_SOURCE_FILES}" ${PROJECT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${ACL_SOURCE_FILES})

if(MSVC)
	# Replace some default compiler switches and add new ones
	STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Disable RTTI
	STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})	# Bump warnings to W4
	target_compile_options(${PROJECT_NAME} PRIVATE /Zi)				# Add debug info
	target_compile_options(${PROJECT_NAME} PRIVATE /Oi)				# Generate intrinsic functions

	# Disable some warnings
	target_compile_options(${PROJECT_NAME} PRIVATE /wd4100)			# unreferenced formal parameter
	target_compile_options(${PROJECT_NAME} PRIVATE /wd4324)			# structure was padded due to alignment specified
	target_compile_options(${PROJECT_NAME} PRIVATE /wd4127)			# conditional expression is constant

	if(MSVC_VERSION GREATER 1900)
		# VS2017 and above
		target_compile_options(${PROJECT_NAME} PRIVATE /permissive-)
	endif()

	if(USE_AVX_INSTRUCTIONS)
		target_compile_options(${PROJECT_NAME} PRIVATE "/arch:AVX")
	endif()

	# Add linker flags
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG")
else()
	# TODO: Handle OS X properly: https://stackoverflow.com/questions/5334095/cmake-multiarchitecture-compilation
	if(CPU_INSTRUCTION_SET MATCHES "x86")
		target_compile_options(${PROJECT_NAME} PRIVATE "-m32")
		target_link_libraries(${PROJECT_NAME} PRIVATE "-m32")
	elseif(CPU_INSTRUCTION_SET MATCHES "x64")
		target_compile_options(${PROJECT_NAME} PRIVATE "-m64")
		target_link_libraries(${PROJECT_NAME} PRIVATE "-m64")
	endif()

	if(CPU_INSTRUCTION_SET MATCHES "x86" OR CPU_INSTRUCTION_SET MATCHES "x64")
		if(USE_AVX_INSTRUCTIONS)
			target_compile_options(${PROJECT_NAME} PRIVATE "-mavx")
		else()
			target_compile_options(${PROJECT_NAME} PRIVATE "-msse4.1")
		endif()
	endif()
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
