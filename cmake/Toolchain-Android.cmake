cmake_minimum_required (VERSION 3.2)
include(ProcessorCount)

set(CMAKE_SYSTEM_NAME Android)

# Use the clang tool set because it's support for C++11 is superior
set(CMAKE_GENERATOR_TOOLSET DefaultClang)

# Make sure we use all our processors when building
ProcessorCount(CMAKE_ANDROID_PROCESS_MAX)
