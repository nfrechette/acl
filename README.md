# Animation Compression Library

## Goals

This library has two primary goals:

*  Implement state of the art and production ready animation compression algorithms
*  Serve as a benchmark to compare various techniques against one another

It is very common for most game engines or animation libraries to implement their own animation compression but the problem
is so narrow and focused in scope, that it makes sense for the industry to converge on a single implementation.

Academic papers on the subject often will not compare techniques using the same error metric or raw size function which can
skew the results or make them very hard to evaluate. They might also compare against techniques which are not state of the
art or even used in production. This library aims to implement as many techniques as possible, side by side, in order
to compare them fairly.

## Philosophy

Much thought was put into designing the library for it to be as flexible and powerful as possible. To this end, the following decisions were made:

*  The library consists of 100% C++ header files and is thus easy to integrate in any game engine
*  We implement our own math types and functions in order to support 32 and 64 bit floating point math
*  Compression is primarily performed with 64 bit floating point math for maximum precision
*  Decompression is entirely performed with 32 bit floating point math for maximum speed
*  [An intermediary clip format](https://github.com/nfrechette/acl/wiki/The-ACL-file-format) is supported in order to facilitate debugging and bug reporting
*  All allocations use a [game provided allocator](https://github.com/nfrechette/acl/blob/develop/includes/acl/core/memory.h)
*  All asserts use a [game provided macro](https://github.com/nfrechette/acl/blob/develop/includes/acl/core/error.h)

## Platforms supported

We aim to support compression and decompression on as many platforms as possible but for now VS2015 on Windows is supported.
In the future, SSE, AVX, and Neon will be fully supported as well.

## Algorithms supported

*  [Uniformly sampled](https://github.com/nfrechette/acl/wiki/Algorithm:-uniformly-sampled)
*  Linear key reduction (TODO)
*  Spline key reduction (TODO)
*  Wavelets (TODO)

## Getting up and running

### Windows

1. Install Visual Studio 2015
2. Install CMake 3.9 or higher
3. Install Python 3.3 (version 3.3 is required for the FBX SDK by some scripts)
4. Generate the IDE solution with: `python make.py`
   The solution is generated under `./build`
   Note that if you do not have CMake in your `PATH`, you should define the `ACL_CMAKE_HOME` environment variable to something like `C:\Program Files\CMake`.
5. Build the IDE solution with: `python make.py -build`

## Reference material

In order to test the algorithm implementations with real world data, we took the Carnegie-Mellon database. It contains over 2500 animation clips.
This is the data all graphs and charts shown will be based on. Data available upon request, it is far too large for GitHub.

## MIT License

Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
