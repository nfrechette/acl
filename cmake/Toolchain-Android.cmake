cmake_minimum_required(VERSION 3.2)

# For Android, we just set the platform name as we won't be using CMake to build anything.
# Instead Gradle is used through CMake.

set(PLATFORM_ANDROID 1)
set(PLATFORM_NAME "Android")

# Remap our CPU instruction set
if(CPU_INSTRUCTION_SET MATCHES "armv7")
	set(CPU_INSTRUCTION_SET "armeabi-v7a")
elseif(CPU_INSTRUCTION_SET MATCHES "arm64")
	set(CPU_INSTRUCTION_SET "arm64-v8a")
endif()

# Set our misc asset directory
set(ACL_ANDROID_MISC_DIR ${CMAKE_SOURCE_DIR}/tools/android_misc)
