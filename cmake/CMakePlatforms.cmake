cmake_minimum_required(VERSION 3.2)

if(PLATFORM_NAME)
	return()	# Already set
endif()

# Detect which platform we have
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(PLATFORM_WINDOWS 1)
	set(PLATFORM_NAME "Windows")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	if(PLATFORM_IOS)
		set(PLATFORM_NAME "iOS")
	else()
		set(PLATFORM_OSX 1)
		set(PLATFORM_NAME "OS X")
	endif()

	# Get the Xcode version being used.
	execute_process(COMMAND xcodebuild -version
		OUTPUT_VARIABLE PLATFORM_XCODE_VERSION
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	string(REGEX MATCH "Xcode [0-9\\.]+" PLATFORM_XCODE_VERSION "${PLATFORM_XCODE_VERSION}")
	string(REGEX REPLACE "Xcode ([0-9\\.]+)" "\\1" PLATFORM_XCODE_VERSION "${PLATFORM_XCODE_VERSION}")
	message(STATUS "Building with Xcode version: ${PLATFORM_XCODE_VERSION}")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(PLATFORM_LINUX 1)
	set(PLATFORM_NAME "Linux")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
	set(PLATFORM_ANDROID 1)
	set(PLATFORM_NAME "Android")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Emscripten")
	set(PLATFORM_EMSCRIPTEN 1)
	set(PLATFORM_NAME "Emscripten")
else()
	message(FATAL_ERROR "Unknown platform ${CMAKE_SYSTEM_NAME}!")
endif()

message(STATUS "Detected platform: ${PLATFORM_NAME}")
