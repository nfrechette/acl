# Significant changes per release

## 0.6.0

*  Hooked up continuous build integration
*  Added support for VS2017 on Windows
*  Added support for GCC5, Clang4, and Clang5 on Linux
*  Added support for Xcode 8.3 and Xcode 9.2 on OS X
*  Added support for x86 on Windows, Linux, and OS X
*  Better handle scale with built in error metrics
*  Many more improvements and fixes

## 0.5.0

*  Added support for 3D scale
*  Added partial support for Android (works in Unreal 4.15)
*  A fix to the variable bit rate optimization algorithm
*  Added a CLA system
*  Refactoring to support multiple algorithms better
*  More changes and additions to stat logging
*  Many more improvements and fixes

## 0.4.0

*  Lots of math performance, accuracy, and consistency improvements
*  Implemented segmenting support in uniformly sampled algorithm
*  Range reduction per segment support added
*  Minor fixes to fbx2acl
*  Optimized variable quantization considerably
*  Major changes to which stats are dumped and how they are processed
*  Many more improvements and fixes

## 0.3.0

*  Added CMake support
*  Improved error measuring and accuracy
*  Improved variable quantization
*  Convert most math to float32 to improve accuracy and performance
*  Many more improvements and fixes

## 0.2.0

*  Added clip_writer to create ACL files from a game integration
*  Added some unit tests and moved them into their own project
*  Added basic per track variable quantization
*  Added CMake support
*  Lots of cleanup, minor changes, and fixes

## 0.1.0

Initial release!

*  Uniformly sampled algorithm
*  Various rotation and vector formats
*  Clip range reduction
*  ACL SJSON file format
*  Custom allocator interface
*  Assert overrides
*  Custom math types and functions
*  Various tools to test the library
*  Visual Studio 2015 supported, x86 and x64
