# Compressing scalar tracks

Once you have created a [raw track list](creating_a_raw_track_list.md) and an [allocator instance](implementing_an_allocator.md), you are ready to compress it.

For now, we only implement a single algorithm: [uniformly sampled](algorithm_uniformly_sampled.md). This is a simple and excellent algorithm to use for everyday animation clips.

## Compressing scalar tracks

*Compression settings are currently required as an argument but not used by scalar tracks. It is a placeholder.*
*Segmenting is currently not supported by scalar tracks.*

```c++
#include <acl/compression/compress.h>

using namespace acl;

compression_settings settings;

OutputStats stats;
compressed_tracks* out_compressed_tracks = nullptr;
ErrorResult result = compress_track_list(allocator, raw_track_list, settings, out_compressed_tracks, stats);
```

## Compressing transform tracks

The compression level used will dictate how much time to spend optimizing the variable bit rates. Lower levels are faster but produce a larger compressed size.

While we support various [rotation and vector quantization formats](rotation_and_vector_formats.md), the *variable* variants are generally the best. It is safe to use them for all your clips but if you do happen to run into issues with some exotic clips, you can easily fallback to less aggressive variants.

Selecting the right [error metric](error_metrics.md) is important and you will want to carefully pick the one that best approximates how your game engine performs skinning.

The last important setting to choose is the `error_threshold`. This is used in conjunction with the error metric and the virtual vertex distance (shell distance) in order to guarantee that a certain quality is maintained. A default value of **0.01cm** is safe to use and it most likely should never be changed unless the units you are using differ. If you do run into issues where compression artifacts are visible, in all likelihood the virtual vertex distance used on the problematic bones is not conservative enough.

```c++
#include <acl/compression/compress.h>

using namespace acl;

compression_settings settings;
settings.level = compression_level8::medium;
settings.rotation_format = rotation_format8::quatf_drop_w_variable;
settings.translation_format = vector_format8::vector3f_variable;
settings.scale_format = vector_format8::vector3f_variable;

qvvf_transform_error_metric error_metric;
settings.error_metric = &error_metric;

output_stats stats;
compressed_tracks* out_compressed_tracks = nullptr;
error_result result = compress_track_list(allocator, raw_track_list, settings, out_compressed_tracks, stats);
```

You can also query the current default and recommended settings with this function: `get_default_compression_settings()`.
