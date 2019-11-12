cmake_minimum_required (VERSION 3.2)

set(CMAKE_SYSTEM_NAME Darwin)

# Set here instead of CMakePlatforms.cmake since we can't distinguis otherwise
set(PLATFORM_IOS 1)

# Find and set the C/C++ compiler paths, cmake doesn't seem to do this properly on its own
execute_process(COMMAND xcrun --sdk iphoneos --find clang OUTPUT_VARIABLE CMAKE_C_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND xcrun --sdk iphoneos --find clang++ OUTPUT_VARIABLE CMAKE_CXX_COMPILER OUTPUT_STRIP_TRAILING_WHITESPACE)

set(CMAKE_MACOSX_BUNDLE YES)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
set(CMAKE_OSX_SYSROOT iphoneos CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "")
set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET 11.0)
