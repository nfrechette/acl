cmake_minimum_required (VERSION 3.9)
project(arithmetic_playground)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")

# Grab all of our source files
file(GLOB_RECURSE ACL_ARITHMETIC_PLAYGROUND_SOURCE_FILES
	${PROJECT_SOURCE_DIR}/sources/*.cpp
	${PROJECT_SOURCE_DIR}/*.py)

add_executable(arithmetic_playground ${ACL_ARITHMETIC_PLAYGROUND_SOURCE_FILES})

# Replace some default compiler switches and add new ones
STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Disable RTTI
STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})	# Bump warnings to W4
target_compile_options(arithmetic_playground PRIVATE /Zi)				# Add debug info
target_compile_options(arithmetic_playground PRIVATE /Oi)				# Generate intrinsic functions

# Disable some warnings
target_compile_options(arithmetic_playground PRIVATE /wd4100)			# unreferenced formal parameter
target_compile_options(arithmetic_playground PRIVATE /wd4324)			# structure was padded due to alignment specified
target_compile_options(arithmetic_playground PRIVATE /wd4127)			# conditional expression is constant

if(USE_AVX_INSTRUCTIONS)
	target_compile_options(arithmetic_playground PRIVATE "/arch:AVX")
endif()

# Add linker flags
set(CMAKE_EXE_LINKER_FLAGS /DEBUG)

install(TARGETS arithmetic_playground RUNTIME DESTINATION bin)
