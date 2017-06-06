# Animation Compression Library

## Goals

This library has 2 primary goals:

*  Implement state of the art production ready animation compression algorithms
*  Serve as a benchmark to compare various techniques against one another

It is very common for most game engines or animation libraries to implement their own animation compression but the problem is so narrow and focused in scope, that it makes sense for the industry to converge on a single implementation.

Academic papers on the subject often will not compare techniques using the same error metric or raw size function which can skew the results or make them very hard to compare. They might also compare against techniques which are not state of the art or even used in production. This library aims to implement as many techniques as possible, side by side, in order to compare them fairly.

## Philosophy

Much thought was put into designing the library for it to be as flexible and powerful as possible. To this end, the following decisions were made:

*  The library consists of 100% C++ header files and is thus easy to integrate in any game engine
*  We implement our own math types and functions in order to support 32 and 64 bit floating point math
*  Compression is entirely performed with 64 bit floating point math for maximum precision
*  Decompression is entirely performed with 32 bit floating point math for maximum speed
*  An intermediary clip format is supported in order to facilitate debugging and bug reporting
*  All allocations use a game provided allocator
*  All asserts use a game provided macro (TODO)

## Platforms supported

We aim to support compression and decompression on as many platforms as possible but for now VS2015 on Windows is supported. In the future, SSE, AVX, and Neon will be fully supported as well.

## Algorithms supported

*  [Uniformly sampled keys](http://nfrechette.github.io/2016/11/15/anim_compression_quantization/) (full precision, **fixed quantization, variable quantization**)
*  **[Linear key reduction](http://nfrechette.github.io/2016/12/07/anim_compression_key_reduction/) (full precision, fixed quantization, variable quantization)**
*  **[Spline key reduction](http://nfrechette.github.io/2016/12/10/anim_compression_curve_fitting/) (full precision, fixed quantization, variable quantization)**
*  **[Wavelets](http://nfrechette.github.io/2016/12/19/anim_compression_signal_processing/)**

**Algorithms in bold are not yet supported**

## MIT License

Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
