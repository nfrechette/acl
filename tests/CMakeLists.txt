cmake_minimum_required (VERSION 3.2)
project(acl_unit_tests)

set(CMAKE_CXX_STANDARD 14)

include_directories("${PROJECT_SOURCE_DIR}/../includes")
include_directories("${PROJECT_SOURCE_DIR}/external/catch-1.9.6")

# Grab all of our source files
file(GLOB_RECURSE ACL_UNIT_TEST_SOURCE_FILES ${PROJECT_SOURCE_DIR}/sources/*.cpp)

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
		target_compile_options(acl_compressor PRIVATE /permissive-)
	endif()

	if(USE_AVX_INSTRUCTIONS)
		target_compile_options(acl_unit_tests PRIVATE "/arch:AVX")
	endif()

	# Add linker flags
	set(CMAKE_EXE_LINKER_FLAGS /DEBUG)
else()
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86" OR CMAKE_SYSTEM_PROCESSOR MATCHES "amd64")
		if(USE_AVX_INSTRUCTIONS)
			target_compile_options(acl_compressor PRIVATE "-mavx")
		else()
			target_compile_options(acl_compressor PRIVATE "-msse4.1")
		endif()
	endif()
endif()

install(TARGETS acl_unit_tests RUNTIME DESTINATION bin)
