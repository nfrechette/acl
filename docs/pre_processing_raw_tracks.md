# Pre-processing raw tracks

Once you have created a [raw track list](creating_a_raw_track_list.md) and an [allocator instance](implementing_an_allocator.md), you can optionally pre-process them.

Pre-processing gives explicit control over raw data modification. When compressing, ACL will never modify the raw data. This is ideal because it ensures that none of the inputs are modified and the whole process is deterministic and repeatable. However, sometimes this can lead to sub-optimal compression. In particular, under certain conditions, ACL may not be able to reach the raw data values when attempting to optimize things. As a result, more precision than otherwise necessary might be retained.

As such, it is recommended that the raw data be pre-processed before compression. This can be done at any point before compression (e.g. after FBX import).

Various pre-process actions can be performed in bulk or individually, see `pre_process_actions` for details.

Pre-processing actions can be *lossy* or *lossless*. See `pre_process_actions` and `pre_process_precision_policy` for details.

With [extended samples](extended_samples.md), `pre_process_track_list(..)` changes only the sample values. The sample interpolators do not change.

When pre-processing transform tracks, selecting the right [error metric](error_metrics.md) is important and you will want to carefully pick the one that best approximates how your game engine performs skinning. You will need to use the same error metric for pre-processing and compression. When `settings.error_metric` is `nullptr` (the default), pre-processing uses the same default error metric as compression.

```c++
#include <acl/compression/pre_process.h>

using namespace acl;

pre_process_settings_t settings;
settings.actions = pre_process_actions::recommended;
settings.precision_policy = pre_process_precision_policy::lossy;

// Optional: when null, pre-processing uses the same default error metric as compression
// vqvf_transform_error_metric error_metric;
// settings.error_metric = &error_metric;

pre_process_track_list(allocator, settings, transform_tracks);
```

## Transform type conversion

The `convert_transform_type` action converts transform tracks into the type of `settings.transform_type` (`vqvf`, `vqsf`, or `vqf`). It is not part of the recommended actions, because it changes the type of the compressed tracks. Compression never changes the type of the raw tracks.

The three types hold more and more scale information: `vqf` has no scale, `vqsf` has a uniform scale, and `vqvf` has a 3D scale.

*  A promotion (for example `vqf` -> `vqsf`, or `vqsf` -> `vqvf`) always succeeds and is lossless. When the source has no scale, each new scale is the identity of the additive format: 0.0 with `additive1`, 1.0 otherwise.
*  A demotion to `vqf` removes the scale. It happens only when no track needs its scale. With the lossless policy, each scale sample must be exactly the identity. With the lossy policy, the error must stay within the precision of each track when its scale becomes the identity.
*  A demotion from `vqvf` to `vqsf` makes each scale uniform. It happens only when no track needs a non-uniform scale. With the lossless policy, the X, Y, and Z components of each scale sample must be exactly equal. With the lossy policy, the error must stay within the precision of each track when its scale becomes the mean of its X, Y, and Z components.

When a track needs its scale, the tracks keep their type.

This action runs last. Thus, with the lossy policy, `sanitize_constant_tracks` and `sanitize_default_tracks` run first.

A typed track array (for example `track_array_vqvf`) cannot change its type. To convert, give `pre_process_track_list(..)` a `track_array`. With a typed track array, the action returns an error when the type must change.

```c++
// Compress the clips without scale as VQ tracks
pre_process_settings_t settings;
settings.actions = pre_process_actions::recommended | pre_process_actions::convert_transform_type;
settings.precision_policy = pre_process_precision_policy::lossy;
settings.transform_type = track_type8::vqf;

track_array transform_tracks = std::move(raw_vqv_tracks);
pre_process_track_list(allocator, settings, transform_tracks);

// transform_tracks.get_track_type() is now vqf if no track needs its scale
```

See [pre_process.h](../includes/acl/compression/pre_process.h) for API and other usage details.
