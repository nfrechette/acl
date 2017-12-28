cmake_minimum_required (VERSION 3.9)
project(arithmetic_playground)

set(CMAKE_CXX_STANDARD 14)

include_directories("${PROJECT_SOURCE_DIR}/../../includes")

# Grab all of our source files
file(GLOB_RECURSE ACL_ARITHMETIC_PLAYGROUND_SOURCE_FILES
	${PROJECT_SOURCE_DIR}/sources/*.cpp)

add_executable(arithmetic_playground ${ACL_ARITHMETIC_PLAYGROUND_SOURCE_FILES})

if(MSVC)
	# Replace some default compiler switches and add new ones
	STRING(REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})		# Disable RTTI
	STRING(REPLACE "/W3" "/W4" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})	# Bump warnings to W4
	target_compile_options(arithmetic_playground PRIVATE /Zi)		# Add debug info
	target_compile_options(arithmetic_playground PRIVATE /Oi)		# Generate intrinsic functions

	# Disable some warnings
	target_compile_options(arithmetic_playground PRIVATE /wd4100)	# unreferenced formal parameter
	target_compile_options(arithmetic_playground PRIVATE /wd4324)	# structure was padded due to alignment specified
	target_compile_options(arithmetic_playground PRIVATE /wd4127)	# conditional expression is constant

	# Always enable AVX in the playground
	target_compile_options(arithmetic_playground PRIVATE "/arch:AVX")

	# Add linker flags
	set(CMAKE_EXE_LINKER_FLAGS /DEBUG)
else()
	# TODO: Handle OS X properly: https://stackoverflow.com/questions/5334095/cmake-multiarchitecture-compilation
	if(CPU_INSTRUCTION_SET MATCHES "x86")
		target_compile_options(arithmetic_playground PRIVATE "-m32")
	elseif(CPU_INSTRUCTION_SET MATCHES "x64")
		target_compile_options(arithmetic_playground PRIVATE "-m64")
	endif()

	if(CPU_INSTRUCTION_SET MATCHES "x86" OR CPU_INSTRUCTION_SET MATCHES "x64")
		# Always enable AVX in the playground
		target_compile_options(arithmetic_playground PRIVATE "-mavx")
	endif()
endif()

install(TARGETS arithmetic_playground RUNTIME DESTINATION bin)
