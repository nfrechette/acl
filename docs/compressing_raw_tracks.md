# Compressing raw tracks

Once you have created a [raw track list](creating_a_raw_track_list.md) and an [allocator instance](implementing_an_allocator.md), you are ready to compress it.

For now, we only implement a single algorithm: [uniformly sampled](algorithm_uniformly_sampled.md). This is a simple and excellent algorithm to use for everyday animation clips.

Note that before compression, it is recommended to [pre-process your raw tracks](pre_processing_raw_tracks.md).

## Compressing raw scalar tracks

Scalar tracks use only some of the compression settings: `scalar_format`, `optimize_loops`, and `metadata`. With the `scalarf_variable` format (see `get_default_compression_settings()`), each animated track uses the lowest bit rate that keeps its precision. With the `scalarf_full` format, each animated track keeps its samples with full precision. Like the transform formats, `compression_settings` defaults to full precision.

Scalar tracks can also use [extended samples](extended_samples.md) to select how each sample interpolates.

*Segmenting is currently not supported by scalar tracks.*

```c++
#include <acl/compression/compress.h>

using namespace acl;

compression_settings settings = get_default_compression_settings();

OutputStats stats;
compressed_tracks* out_compressed_tracks = nullptr;
ErrorResult result = compress_track_list(allocator, raw_track_list, settings, out_compressed_tracks, stats);
```

## Compressing raw transform tracks

The compression level used will dictate how much time to spend optimizing the variable bit rates. Lower levels are faster but produce a larger compressed size.

While we support various [rotation and vector quantization formats](rotation_and_vector_formats.md), the *variable* variants are generally the best. It is safe to use them for all your clips but if you do happen to run into issues with some exotic clips, you can easily fallback to less aggressive variants.

Selecting the right [runtime transform type](error_metrics.md) is important: select the one that your game engine uses to compose transforms. With `transform_runtime_type_t::automatic` (the default), compression uses exact VQM arithmetic when scale is present (or VQS arithmetic with `vqsf` raw tracks). Without scale, it uses VQ arithmetic and measures the exact error. Select another type only when your game engine composes transforms differently (for example, `transform_runtime_type_t::vqvf` for VQV arithmetic that discards the shear).

The last important setting to choose is the `error_threshold`. This is used in conjunction with the transform metric and the virtual vertex distance (shell distance) in order to guarantee that a certain quality is maintained. A default value of **0.01cm** is safe to use and it most likely should never be changed unless the units you are using differ. If you do run into issues where compression artifacts are visible, in all likelihood the virtual vertex distance used on the problematic bones is not conservative enough.

```c++
#include <acl/compression/compress.h>

using namespace acl;

compression_settings settings;
settings.level = compression_level8::medium;
settings.rotation_format = rotation_format8::quatf_drop_w_variable;
settings.translation_format = vector_format8::vector3f_variable;
settings.scale_format = vector_format8::vector3f_variable;

// Optional: the default 'automatic' uses VQM arithmetic with scale
// settings.transform_runtime_type = transform_runtime_type_t::vqvf;

output_stats stats;
compressed_tracks* out_compressed_tracks = nullptr;
error_result result = compress_track_list(allocator, raw_track_list, settings, out_compressed_tracks, stats);
```

You can also query the current default and recommended settings with this function: `get_default_compression_settings()`.

## Peak memory usage

Compression allocates temporary buffers. The largest clips use many times their raw size. When many clips compress in parallel, the sum of these buffers can cause the system to write memory pages to disk.

Use `estimate_compression_peak_memory_usage(..)` to find the memory that a clip needs before you compress it. It takes the same tracks, settings, and base as `compress_track_list(..)`. It does not allocate memory, and its cost is low compared to compression. For example, a scheduler can start a new compression only when the sum of the estimates is less than a memory budget.

```c++
#include <acl/compression/compress.h>

using namespace acl;

const size_t peak_memory_usage = estimate_compression_peak_memory_usage(raw_track_list, settings);
```

The overload with a track list reads the sizes, the hierarchy, and the names of the tracks, not their samples. To decide before you load the tracks, use the overload with a `compression_memory_desc_t`. Fill it from the metadata of the asset: the track type, the number of tracks, and the number of samples are required. The other values are optional. Except for `total_name_length`, each default value gives the largest estimate:

*  `max_hierarchy_depth`: the largest number of ancestors of a transform. Without it, the estimate uses a single chain of transforms, and some buffers grow with the square of the number of transforms.
*  `total_name_length`: the length of the names. Set it when the metadata settings include the names.
*  `looping_policy`, `transform_space`, and the values of the base (`base_format`, `num_base_tracks`, `num_base_samples_per_track`, `base_has_non_uniform_scale`).

```c++
#include <acl/compression/compress.h>

using namespace acl;

compression_memory_desc_t desc;
desc.track_type = track_type8::vqvf;
desc.num_tracks = num_bones;
desc.num_samples_per_track = num_frames;
desc.max_hierarchy_depth = max_bone_depth;	// Optional

const size_t peak_memory_usage = estimate_compression_peak_memory_usage(desc, settings);
```

On a large set of production clips, the estimate with only the track type and the numbers of tracks and samples is 2% larger than the estimate with the track list for the median clip, and at most 17% larger. With `max_hierarchy_depth` too, the two estimates are equal.

The value is the largest sum of the live allocations that compression requests from its allocator. It does not include the overhead of the allocator, the raw tracks, or the statistics of `output_stats::logging`. The estimate cannot know which tracks are constant before compression, thus it assumes that each track is animated. The value is usually larger than the real peak. On a large set of production clips with the default settings, the estimate is 9% to 118% larger than the measured peak, and 58% larger for the median clip.
