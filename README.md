[![CLA assistant](https://cla-assistant.io/readme/badge/nfrechette/acl)](https://cla-assistant.io/nfrechette/acl)
[![Build status](https://ci.appveyor.com/api/projects/status/8h1jwmhumqh9ie3h?svg=true)](https://ci.appveyor.com/project/nfrechette/acl)
[![Build Status](https://travis-ci.org/nfrechette/acl.svg?branch=develop)](https://travis-ci.org/nfrechette/acl)

# ==> Research Branch <==

THIS IS A RESEARCH BRANCH. IT IS NOT MAINTAINED AND ITS ONLY PURPOSE IS TO BE A PLAYGROUND FOR RESEARCH.

This branch will compare the accuracy of using the current approach with **float32** arithmetic against **float64** and **fixed point** arithmetic.
See [here](./includes/acl/core/research.h) for relevant defines.

This research branch adds two things: **float64** support to `acl_compressor` and the `arithmetic_playground` executable.

## Conclusion

**float64** does not have a measurable impact on the memory footprint. It seems that **float32** arithmetic is good enough for us and nearly as accurate.
However, **float64** arithmetic is much slower to execute and the compression time increases considerably. **float32** wins hands down here.

Adding **fixed point** support to everything seemed too complicated and instead a small playground was created to explore how it performs.
Various flavors were tried and measured. Overall, accuracy is almost always worst or equal to **float32** arithmetic. **64** bit integers
are required to keep accuracy competitive but performance degrades considerably with the sharp increase in registers and instructions used.
The biggest win came from performing a fast coercion from **fixed point** to **float32** by performing a `shift` and logical `or` with the exponent
bits. This yielded a **20-30%** win over the legacy **float32** implementation with nearly the same accuracy. A **32** bit **fixed point** variant
did come close in terms of performance but the accuracy was quite a bit worst. It is hard to estimate how the memory footprint would be impacted.
Despite being shorter by **3** instructions compared to the legacy with fast coercion, it consistently clocked slower on my machine. This is likely
due to pipeline stalls and in the real decompression code, it is quite possible these could be hidden much better.

Overall, **fixed point** performance was underwhelming on my CPU. It is worth noting that it contains **2** floating point units which allow
dual dispatching just like for integer arithmetic. However, the Xbox One CPU does not and nor do current mobile devices. It is entirely possible that
**fixed point** arithmetic could be a consistent win on those platforms and proper measurement will be required.

The key take aways are:

*  Arithmetic accuracy does not measurably impact the memory footprint, **float32** is good enough
*  Fixed point arithmetic is not more accurate and it can be slower on modern processors
*  Fast coercion is a big performance win

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
*  [An intermediary clip format](./docs/the_acl_file_format.md) is supported in order to facilitate debugging and bug reporting
*  All allocations use a [game provided allocator](./includes/acl/core/memory.h)
*  All asserts use a [game provided macro](./includes/acl/core/error.h)

## Supported platforms

The library aims to support the most common platforms for the most common use cases. There is very little platform specific code as such it should work nearly everywhere.

The math library is not yet fully optimized for every platform. The overwhelming majority of the math heavy code executes when compressing, not decompressing.
Decompression is typically very simple and light in order to be fast. Very little math is involved beyond interpolating values.

*  Compression and decompression: Windows (VS2015, VS2017) x86 and x64, Linux (gcc5, clang4, clang5) x64
*  Decompression only: Android (NVIDIA CodeWorks)

## Algorithms supported

*  [Uniformly sampled](./docs/algorithm_uniformly_sampled.md)
*  Linear key reduction (TODO)
*  Spline key reduction (TODO)
*  Wavelets (TODO)

## Getting up and running

### Windows and Linux

1. Install the proper compiler for your platform
2. Install CMake 3.2 or higher
3. Install Python 3
4. Generate the IDE solution with: `python make.py`  
   The solution is generated under `./build`  
   Note that if you do not have CMake in your `PATH`, you should define the `ACL_CMAKE_HOME` environment variable to something like `C:\Program Files\CMake`.
5. Build the IDE solution with: `python make.py -build`
6. Run the unit tests with: `python make.py -test`

## Performance metrics

*  [Carnegie-Mellon University database performance](./docs/cmu_performance.md)
*  [Paragon database performance](./docs/paragon_performance.md)

## MIT License

Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
