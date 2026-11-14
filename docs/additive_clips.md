# Additive animation clips

Additive animation clips are a core tool within an animation runtime but sadly, how additive animations are blended and applied is not standard. As such, this library aims to support the most common additive modes. Contributions welcome!

The core logic for additive clips lives [here](../includes/acl/core/additive_utils.h).

## The transform space

A `transform_space_desc_t` (see [here](../includes/acl/compression/transform_space.h)) tells compression how to measure the error of a clip. It holds two independent properties:

*  `space`: how a sample uses the transform hierarchy, see `transform_space_t`. At the moment, ACL supports only `local`: each transform is relative to its parent transform.
*  `base_format` and `base`: how a sample combines with its base clip, see `additive_clip_format8` and the formats below. With `none`, the clip has no base.

Compression first combines each sample with the base. Then it puts the result in object space to measure the error. The transform space does not change the compressed data: decompression returns the samples of the clip, and your runtime applies them to the base.

```c++
transform_space_desc_t transform_space;
transform_space.base_format = additive_clip_format8::additive1;
transform_space.base = &base_track_list;

error_result result = compress_track_list(allocator, raw_track_list, settings, transform_space, out_compressed_tracks, stats);
```

Use the same transform space for [pre-processing](pre_processing_raw_tracks.md) (`pre_process_settings_t::transform_space`) and to [measure the error](error_metrics.md) (`calculate_compression_error`).

## Relative additive clips

Clips in this space are applied on top of their base clip with a simple multiplication:

`result = additive * base`

This is the standard way to represent things in local space of something else. For example, every bone is in relative and local space of its parent bone. To convert a bone from local space to its parent space, we multiply them.

## Additive 0

*I couldn't come up with a better name for this, suggestions welcome!*

Clips in this space have their rotations multiplied with the base, the translations are added, and the scale is combined as follow:

`scale = additive_scale * base_scale`

Among others, this mode is used by [ozz-animation](http://guillaumeblanc.github.io/ozz-animation/).

## Additive 1

*I couldn't come up with a better name for this, suggestions welcome!*

Clips in this space have their rotations multiplied with the base, the translations are added, and the scale is combined as follow:

`scale = (1.0 + additive_scale) * base_scale`

This is the additive mode used by Unreal 4.
