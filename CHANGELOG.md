# Significant changes per release

## Unreleased

*  Add extended samples to scalar tracks, each sample can use a constant or a linear interpolation, see [extended samples](docs/extended_samples.md)
*  Fix scalar tracks with more than one component that always used the raw bit rate
*  Add the `scalar_format` compression setting, it defaults to full precision like the transform formats
*  Default `compression_settings` now keep scalar tracks with full precision, use `get_default_compression_settings()` for variable bit rates
*  Fix `pre_process_track_list` with extended scalar samples, it now changes only the sample values
*  Compression ignores the sample interpolators that have no effect when it selects the interpolation mode of a track
*  Add the `-analyze` action to `make.py` to run the clang static analyzer on the public headers
*  Disable fast math with the `float_control` pragma under Clang where the target supports it
*  Add the `-fast_math` option to `make.py` to compile the public headers with fast math, and a CI workflow that uses it
*  Add native Linux `riscv64` builds to `make.py`, they use the scalar code path of RTM
*  Fix `split_database_bulk_data` that gave a null pointer to `std::memcpy` when a quality tier is empty
*  ACL now applies the additive base to the `rtm::vqvf` source transforms before `convert_transforms`, `apply_additive_to_base` always receives `rtm::vqvf` transforms
*  Loop optimization and the detailed segment stats now support error metrics that convert transforms
*  Fix the SoA quaternion rotation of the error metrics, two terms had the wrong sign
*  Add the `-error_vqv` option and the `use_vqv_error_metric` config option to `acl_compressor`, they select the `vqvf` runtime transform type
*  The `-error_mtx` option of `acl_compressor` now selects the `matrix3x4f` runtime transform type, it supports additive clips
*  Add the `vqf` transform track type (rotation and translation, no scale) with `track_vqf` and `track_array_vqf`, the compressed tracks keep the type of the raw tracks
*  Add the compressed data version `v02_02_99_1` for the `vqf` track type
*  Decompression does not write the scale of a `vqf` clip, the default transform decompression settings accept `vqf` and `vqvf`
*  Add the `convert_transform_type` pre-process action and `pre_process_settings_t::transform_type` to convert between `vqvf` and `vqf`
*  The additive overloads of `compress_track_list` and `calculate_compression_error` take a `track_array`, the clip and its base can have different transform types
*  `pre_process_settings_t::additive_base` is now a `const track_array*`
*  The clip reader and writer support `vqf` tracks in the track list format
*  Add the `-source_vq` option to `acl_compressor` and its python script, it compresses the clips without scale as `vqf` tracks
*  Add the `vqsf` transform track type (rotation, translation, and uniform scale) with `track_vqsf` and `track_array_vqsf`, the compressed data version `v02_02_99_1` also covers it
*  A compressed `vqsf` clip stores one scale component for each scale sub-track: the constant samples, the clip and segment ranges, and the animated samples are smaller than with `vqvf`
*  Decompression writes the scale of a `vqsf` clip with the new `track_writer::write_scale(uint32_t, rtm::scalarf_arg0)` overload when the track writer has it, otherwise with the `rtm::vector4f_arg0` overload and the value in [xyzw]
*  The default transform decompression settings accept `vqsf`
*  The `convert_transform_type` pre-process action converts between `vqvf`, `vqsf`, and `vqf`
*  The clip reader and writer support `vqsf` tracks in the track list format
*  Add the `-source_vqs` option to `acl_compressor` and its python script, it compresses the clips without scale or with a uniform scale as `vqsf` tracks
*  Add `transform_runtime_type_t` and the transform adapters (`itransform_adapter`): `vqf_transform_adapter`, `vqsf_transform_adapter`, `vqvf_transform_adapter`, `vqmf_transform_adapter`, and `matrix3x4f_transform_adapter`, see [error metrics](docs/error_metrics.md)
*  Add the transform metrics (`itransform_metric`): `shell_transform_metric` and `precise_transform_metric`, they measure the error through a transform adapter
*  Add the `transform_runtime_type`, `custom_transform_adapter`, and `transform_metric` settings to `compression_settings` and `pre_process_settings_t`
*  The default runtime transform type `automatic` uses VQM arithmetic with scale, or VQS arithmetic with `vqsf` raw tracks
*  Compression uses VQ arithmetic and the precise metric for each segment without scale, it measures the exact error: the compressed size can be slightly larger and fewer clips have an error above the threshold
*  Compression always uses a custom transform adapter, also for a segment without scale
*  Deprecate `compression_settings::error_metric` and `pre_process_settings_t::error_metric` with `ACL_DEPRECATED`, the compiler warns when you use them and ACL 2.3 will remove them, see [migrating](docs/migrating.md)
*  Deprecate `itransform_error_metric`, the legacy error metrics, and the `calculate_compression_error` overloads that take one, ACL 2.3 will remove them
*  Add the `calculate_compression_error` overloads that take the compression settings, they measure the error as compression does
*  The bit rate algorithm v2 supports each runtime transform type, VQS, matrix, and custom transform types no longer use the bit rate algorithm v1
*  The built-in transform adapters and metrics use code without virtual function calls, compression is faster
*  Fix the bit rate algorithm v1 that converted transforms that it did not sample, an assert could fail with an additive base
*  Disable floating point contraction (FMA) in the ACL headers with the `optimize` pragma under GCC
*  Add `transform_space_t` and `transform_space_desc_t`: the space of the transform tracks and their base, see [additive clips](docs/additive_clips.md)
*  Add the `compress_track_list` overload that takes a `transform_space_desc_t`, and deprecate the overload that takes an additive base and an additive format, ACL 2.3 will remove it
*  Add `pre_process_settings_t::transform_space`, and deprecate `pre_process_settings_t::additive_base` and `additive_format`, ACL 2.3 will remove them
*  The `calculate_compression_error` overload with the compression settings and a base now takes a `transform_space_desc_t`
*  The clip reader reads the optional `transform_space` value of a raw clip, it defaults to `local`
*  Add the `mesh` transform space: each transform is in object space, compression measures the error of each transform without its parent, and it combines the clip with the base in mesh space
*  Add the `-mesh_space` option and the `is_mesh_space` config option to `acl_compressor` and its python script, they convert a local space clip into mesh space
*  Add two regression configurations in mesh space, without a base and with a relative base
*  Add the `mesh_rotation` transform space: the rotations are in object space, the translations and scales are in local space, it supports the `vqf`, `vqsf`, `vqvf`, and `vqmf` runtime transform types and the `additive0` and `additive1` base formats
*  Add the `-mesh_rotation_space` option and the `is_mesh_rotation_space`, `is_bind_pose_additive0`, and `is_bind_pose_additive1` config options to `acl_compressor`, and two regression configurations in mesh rotation space
*  In the mesh rotation space, the bit rate search caches the transforms of the `vqf` and `vqsf` types, as in local space
*  Add the `custom` base format and `itransform_base_adapter_t`: a base adapter combines each transform with the base, set it in `transform_space_desc_t::base_adapter`, see [additive clips](docs/additive_clips.md#custom-base-format)
*  Fix the `max_error` stat of `acl_compressor` that ignored the additive base

## 2.1.0

*  Add support for bind pose stripping
*  Add support for loop handling through looping policy
*  Add support for pre-processing
*  Add automatic compression level selection
*  Optimize compression through dominant shell computation
*  Add support for per sub-track rounding
*  Use error metric to detect constant/default sub-tracks
*  Tons of cleanup and minor improvements
*  Run regression tests with CI
*  Update to RTM 2.2.0
*  Update to sjson-cpp 0.9.0
*  Add support for GCC 12 and 13
*  Add support for clang 15
*  Add support for XCode 14
*  Add support for C++14, C++17, and C++20
*  Add support for MSYS2

## 2.0.6

*  Force macro expansion in version namespace identifier
*  Update to RTM v2.1.5
*  Update sjson-cpp to v0.8.3

## 2.0.5

*  Add support for clang 12, 13, and 14
*  Add support for GCC 11
*  Add support for XCode 12 and 13
*  Add support for Arm64 development on OS X and Linux
*  Misc CI improvements
*  Update to RTM v2.1.4
*  Update to Catch2 v2.13.7

## 2.0.4

*  Disable versioned namespace by default to avoid breaking ABI in patch release

## 2.0.3

*  Update sjson-cpp to v0.8.2
*  Update rtm to v2.1.3
*  Add versioned namespace to allow multiple versions to coexist within a binary
*  Fix database sampling interpolation when using a rounding mode other than `none`
*  Other minor fixes

## 2.0.2

*  Fix potential heap corruption when stripping a database that isn't split
*  Fix database tier bulk data size to include its padding

## 2.0.1

*  Fix incorrect seek offset when seeking past 0.0 in a single frame clip

## 2.0.0

*  Unified and cleaned up APIs
*  Cleaned up naming convention to match C++ stdlib, boost
*  Introduced streaming database support
*  Decompression profiling now uses Google Benchmark
*  Decompression has been heavily optimized
*  Compression has been heavily optimized
*  First release to support backwards compatibility going forward
*  Migrated all math to Realtime Math
*  Clips now support 4 billion samples/tracks
*  WebAssembly support added through emscripten
*  Many other improvements

## 1.3.5

*  Gracefully fail compression when we have more than 50000 samples
*  Update Catch2 to 2.13.1 to work with latest CMake

## 1.3.4

*  Avoid assert when using an additive base with a static pose/single frame

## 1.3.3

*  Fix single track decompression when scale is present with more than one segment
*  Gracefully fail compression when we have more than 65535 samples

## 1.3.2

*  Fix crash when compressing with an empty track array
*  Strip unused code when stat logging isn't required
*  Fix CompressedClip hash to be deterministic

## 1.3.1

*  Fix bug with scalar track decompression where garbage could be returned
*  Fix scalar track quantization to properly check the resulting error
*  Fix scalar track creation and properly copy the sample size
*  Other minor fixes and improvements

## 1.3.0

*  Added support for VS2019, GCC 9, clang7, and Xcode 11
*  Updated sjson-cpp and added a dependency on Realtime Math (RTM)
*  Optimized compression and decompression significantly
*  Added support for multiple root bones
*  Added support for scalar track compression
*  Many bug fixes and improvements

## 1.2.1

*  Silence SSE floating point exceptions during compression
*  Minor fixes

## 1.2.0

*  Added support for GCC 8, clang 6, Xcode 10, and Windows ARM64
*  Updated catch2 and sjson-cpp
*  Integrated SonarCloud
*  Added a compression level setting and changed default to *Medium*
*  Various bug fixes, minor optimizations, and cleanup

## 1.1.0

*  Added proper ARM NEON support
*  Properly detect SSE2 with MSVC if AVX isn't used
*  Lots of decompression performance optimizations
*  Minor fixes and cleanup

## 1.0.0

*  Minor additions to fully support UE4
*  Minor cleanup

## 0.8.0

*  Improved error handling
*  Added additive clip support
*  Added `acl_decompressor` tool to profile and test decompression
*  Increased warning levels to highest possible
*  Many more improvements and fixes

## 0.7.0

*  Added full support for Android and iOS
*  Added support for GCC6 and GCC7 on Linux
*  Downgraded C++ version to from 14 to 11
*  Added regression tests
*  Added lots of unit tests for core and math headers
*  Many more improvements and fixes

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
