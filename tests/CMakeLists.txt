cmake_minimum_required (VERSION 3.9)
project(acl_unit_tests)

include_directories("${PROJECT_SOURCE_DIR}/../includes")
include_directories("${PROJECT_SOURCE_DIR}/external/catch-1.9.6")

# Grab all of our source files
file(GLOB_RECURSE ACL_UNIT_TEST_SOURCE_FILES ${PROJECT_SOURCE_DIR}/sources/*.cpp)

add_executable(acl_unit_tests ${ACL_UNIT_TEST_SOURCE_FILES})

# Replace some default compiler switches and add new ones
STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Disable RTTI
STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})	# Bump warnings to W4
target_compile_options(acl_unit_tests PRIVATE /Zi)				# Add debug info
target_compile_options(acl_unit_tests PRIVATE /Oi)				# Generate intrinsic functions

# Disable some warnings
target_compile_options(acl_unit_tests PRIVATE /wd4100)			# unreferenced formal parameter
target_compile_options(acl_unit_tests PRIVATE /wd4324)			# structure was padded due to alignment specified

if(USE_AVX_INSTRUCTIONS)
	target_compile_options(acl_unit_tests PRIVATE "/arch:AVX")
endif()

# Add linker flags
set(CMAKE_EXE_LINKER_FLAGS /DEBUG)

install(TARGETS acl_unit_tests RUNTIME DESTINATION bin)
