# Significant changes per release

## Unreleased

See [migrating](docs/migrating.md) for the changes that can affect your code.

### Breaking changes and deprecations

*  ACL needs RTM 2.4.0 or later
*  The `qvv` names are now `vqv`, as in RTM 2.4: `track_vqvf`, `track_array_vqvf`, `track_type8::vqvf`, `vqvf_transform_error_metric`, and `acl/math/vqvf.h`. The `qvv` names are deprecated, and ACL 2.3 will remove them
*  A `compression_settings` that you construct keeps the scalar tracks with full precision. Use `get_default_compression_settings()` or the new `scalar_format` setting for a variable bit rate
*  Deprecate the legacy error metrics: `itransform_error_metric`, the built-in legacy metrics, `compression_settings::error_metric`, `pre_process_settings_t::error_metric`, and the `calculate_compression_error` overloads that take a legacy metric. ACL 2.3 will remove them
*  Deprecate the `compress_track_list` overload that takes an additive base and an additive format, and `pre_process_settings_t::additive_base` and `additive_format`. Use `transform_base_desc_t`. ACL 2.3 will remove them
*  ACL applies the additive base before `convert_transforms`: `apply_additive_to_base` of a legacy error metric always receives `rtm::vqvf` transforms
*  The default compression settings use the `quatf_drop_largest_variable` rotation format, and `default_transform_decompression_settings` supports only this rotation format. To decompress the clips that use `quatf_drop_w_variable`, override `is_rotation_format_supported` in your decompression settings
*  All the public functions that return a value use the new `ACL_NO_DISCARD` macro. With C++17 and later, it is `[[nodiscard]]`, and the compiler gives a warning when your code does not use the return value. Define `ACL_NO_DISCARD` before you include ACL to use a different attribute, or none. See [configuration macros](docs/configuration_macros.md)
*  The compiler gives a warning when your code uses a deprecated API. Define `ACL_NO_DEPRECATION` to disable the warnings
*  The compressed data version is now `v02_02_00`. ACL 2.2 decompresses the data of ACL 2.0 and 2.1

### Transform tracks

*  Add the `vqf` transform track type (rotation and translation) and the `vqsf` transform track type (rotation, translation, and uniform scale), with `track_vqf`, `track_array_vqf`, `track_vqsf`, and `track_array_vqsf`. The compressed tracks keep the type of the raw tracks
*  A compressed `vqsf` clip stores one scale component in place of three: the constant samples, the ranges, and the animated samples are smaller than with `vqvf`
*  Decompression writes the scale of a `vqsf` clip with the new `track_writer::write_scale(uint32_t, rtm::scalarf_arg0)` overload when the track writer has it. It does not write the scale of a `vqf` clip
*  Add the `convert_transform_type` pre-process action and `pre_process_settings_t::transform_type` to convert between `vqvf`, `vqsf`, and `vqf`
*  Add the runtime transform type (`transform_runtime_type_t`), the transform adapters (`itransform_adapter`), and the transform metrics (`itransform_metric`). Compression measures the error with the arithmetic of your runtime: VQ, VQS, VQV, VQM, matrix, or your own type. See [error metrics](docs/error_metrics.md)
*  Add the built-in transform adapters (`vqf_transform_adapter`, `vqsf_transform_adapter`, `vqvf_transform_adapter`, `vqmf_transform_adapter`, and `matrix3x4f_transform_adapter`) and transform metrics (`shell_transform_metric` and `precise_transform_metric`)
*  Add the `transform_runtime_type`, `custom_transform_adapter`, and `transform_metric` settings to `compression_settings` and `pre_process_settings_t`
*  The default runtime transform type, `automatic`, uses VQM arithmetic with scale, or VQS arithmetic with `vqsf` tracks. Each segment without scale uses VQ arithmetic and the precise metric: the error is exact, the compressed size can be slightly larger, and fewer clips have an error above the threshold
*  Add the transform space of a track array (`transform_space_t`, `track_array::set_transform_space`): `local`, `mesh`, and `mesh_rotation`. The `mesh_rotation` space does not support the `relative` base format. See [additive clips](docs/additive_clips.md)
*  Add `transform_base_desc_t` for the base of a clip, and `pre_process_settings_t::transform_base`. The clip and its base can have different transform types
*  Add the `custom` base format: a base adapter (`itransform_base_adapter_t`) combines each transform with the base
*  Add the uniform playback scale of a clip (`track_array::set_playback_scale`). The precision, the keyframe stripping threshold, and the measured error are in playback units
*  Add the `sanitize_identity_rotations` pre-process action to the recommended actions. With the lossy policy, it sets the [xyz] components of a rotation with W = 1.0 or W = -1.0 to 0.0
*  Add the `quatf_drop_largest_full` and `quatf_drop_largest_variable` rotation formats: a rotation drops another component than W to keep the dropped component far from 0. See [rotation formats](docs/rotation_and_vector_formats.md#dropping-the-largest-quaternion-component). With `quatf_drop_largest_variable`, each segment of a track drops the component whose smallest absolute value is the largest. A constant rotation, and each sample with `quatf_drop_largest_full`, drops its largest component. The index of the dropped component uses no additional memory
*  On two data sets of raw clips, `quatf_drop_largest_variable` gives a compressed size 0.5% and 3.2% smaller than `quatf_drop_w_variable`, and the number of clips with an error above 0.02 goes from 115 to 0 and from 749 to 2. On arm64, the time to decompress a full pose is about 4% higher
*  The clip reader and the clip writer support the `vqf` and `vqsf` tracks, the transform space, and the playback scale

### Scalar tracks

*  Add extended samples to scalar tracks: each sample can use a constant or a linear interpolation. See [extended samples](docs/extended_samples.md)
*  Add the `scalar_format` compression setting. Its default value is full precision, the same as the transform formats
*  A track writer can skip the scalar tracks that the runtime does not need, with `skip_track_float1` to `skip_track_float4` and `skip_track_vector4`
*  Fix the scalar tracks with more than one component that always used the raw bit rate
*  Fix `pre_process_track_list` with extended scalar samples: it now changes only the sample values
*  Compression ignores the sample interpolators that have no effect when it selects the interpolation mode of a track

### Compression

*  Add the bit rate algorithm v2. It supports each runtime transform type, and it is faster than the bit rate algorithm v1. Only a legacy error metric that does not support the v2 algorithm uses the v1 algorithm
*  Optimize the bit rate algorithm v2: it measures first the raw bit rates and the critical transform that failed. The results do not change. Compression is 57% to 66% faster, and up to 9x faster on long clips with scale and deep transform chains
*  The built-in transform adapters and metrics use code without virtual function calls: compression is faster
*  The precise transform metric computes the error without the delta transform: compression is faster, for example 15% on a data set of clips without scale, and the results do not change
*  Add `estimate_compression_peak_memory_usage` to estimate the peak memory usage of `compress_track_list`. See [compressing raw tracks](docs/compressing_raw_tracks.md#peak-memory-usage)
*  Compression does not allocate the bit rate database when no format is variable: with full precision formats, the peak memory usage is 37% to 64% lower
*  Compression measures the same error as `calculate_compression_error`: it no longer normalizes a transform after it combines the transform with the additive base
*  Add the `calculate_compression_error` overloads that take the compression settings. They measure the error as compression does
*  Fix the constant and default sub-track tests, the rigid shells, and the bit rate algorithm v2 with mixed precisions. Before, a large parent scale, a scale below 1, mixed precisions, or a rotation between two non-uniform scales could give an error far above the precision
*  Loop optimization and the detailed segment stats support the error metrics that convert transforms
*  Fix the compression of a clip with one sample and a base with more than one sample. The clip now uses the first base sample
*  Fix `split_database_bulk_data`, which gave a null pointer to `std::memcpy` when a quality tier is empty
*  Fix the bit rate algorithm v1, which converted transforms that it did not sample
*  Fix the bit rate search with a stripped track (`k_invalid_track_index`): it now uses the raw value of the track, as `calculate_compression_error`. Before, it used the compressed value that the runtime never decompresses, and the search could select bit rates that were too high or too low
*  `convert_track_list` returns an error when it cannot initialize its decompression context
*  Fix the SoA quaternion rotation of the error metrics: two terms had the wrong sign
*  Fix the `track_bit_rate_database_size` value of the detailed stats: it now gives the size in bytes

### Decompression

*  Optimize the decompression of transform tracks: the bit set iteration, the unpack of the vector3 values, and the unpack of the constant rotations are faster
*  Add WASM SIMD code paths where ACL uses SSE2 or NEON intrinsics directly. With V8 on arm64, the time to decompress a full pose is 15% lower
*  Fix the determinism of the results: with the NEON FMA instructions, with the interpolation alpha of scalar tracks, and with the rotation normalization

### Platforms and build

*  Add support for GCC 14 and 15, Clang 16 to 23, Visual Studio 2026, and Xcode 16 and 26
*  CI tests GCC 14 and 15, Clang 22 and 23, Visual Studio 2022 and 2026, and Xcode 16 and 26. CI no longer tests the older compilers, but `make.py` still accepts them
*  Add support for ARM64EC with Visual Studio
*  Add native Linux `riscv64` builds, they use the scalar code path of RTM
*  Add Linux `armv7` cross compilation with GCC and Clang, `qemu-arm` runs the unit tests
*  Disable fast math in the ACL headers with the `float_control` pragma under Clang, on the targets that support it
*  Disable floating point contraction (FMA) in the ACL headers under Clang, on the targets that support `float_control`: x86, x64, AArch64 (Clang 16+, Apple Clang 15+), and RISC-V (Clang 18+). With `-ffp-contract=fast`, LLVM Clang ignores the pragma: use `-ffp-contract=fast-honor-pragmas`
*  With GCC, the headers do not disable floating point contraction. To get the same results on all the targets with GCC, compile with `-ffp-contract=off`. See [other considerations](docs/misc_integration_details.md)
*  Add `acl/config.h`, which lists the macros that configure ACL
*  Update to RTM 2.4.0

### Tools and documentation

*  Add a documentation site: Doxygen reads the `///` comments of the public headers, and MkDocs makes the site with the guides and the API reference for develop and for each release
*  Add the [configuration macros](docs/configuration_macros.md) page
*  Add these options to `acl_compressor`, with their config options: `-source_vq`, `-source_vqs`, `-error_vqv`, `-mesh_space`, `-mesh_rotation_space`, `-base_custom`, and `-playback_scale=`. The `-error_mtx` option now selects the `matrix3x4f` runtime transform type
*  Add regression configurations for the mesh space, the mesh rotation space, the custom base format, and the playback scale
*  Fix the `max_error` stat of `acl_compressor`, which ignored the additive base
*  `acl_compressor` and `acl_decompressor` stop when `pre_process_track_list` returns an error
*  Add the `-analyze` action to `make.py` to run the clang static analyzer on the public headers
*  Add the `-fast_math` option to `make.py` to compile the public headers with fast math, and a CI workflow that uses it
*  On macOS and iOS, the decompression benchmark runs on a performance core

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
