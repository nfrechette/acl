# Additive animation clips

Additive animation clips are a core tool within an animation runtime but sadly, how additive animations are blended and applied is not standard. As such, this library aims to support the most common additive modes. Contributions welcome!

The core logic for additive clips lives [here](../includes/acl/core/additive_utils.h).

## The transform space

A `transform_space_desc_t` (see [here](../includes/acl/compression/transform_space.h)) tells compression how to measure the error of a clip. It holds two independent properties:

*  `space`: how a sample uses the transform hierarchy, see `transform_space_t`.
    *  `local` (the default): each transform is relative to its parent transform.
    *  `mesh`: each transform is in object space (mesh space). It does not use its parent transform. Compression measures the error of each transform alone: the error of a parent does not move its children.
    *  `mesh_rotation`: the rotation of each transform is in object space, its translation and scale are relative to its parent transform. The rotation of a parent does not change the object rotation of its children, but the parent still moves them. Compression measures the error in object space with this rule: `object = mul(local, parent_object)`, with `local.rotation = mul(rotation, conjugate(parent_object.rotation))`.
*  `base_format` and `base`: how a sample combines with its base clip, see `additive_clip_format8` and the formats below. With `none`, the clip has no base.

Compression first combines each sample with the base. Then it puts the result in object space to measure the error. The transform space does not change the compressed data: decompression returns the samples of the clip, and your runtime applies them to the base.

The base is always in local space, with the transform hierarchy of the clip. In mesh space, compression puts the base in mesh space with `rtm::vqv_mul` (the scale of a parent does not shear its children). Then it combines each sample with the base in mesh space. In mesh rotation space, compression puts only the rotations of the base in mesh space, then it combines each sample with the base: the rotations in mesh space, the translations and scales in local space. The compressed tracks keep the parent indices of the clip.

The mesh rotation space has two limits:

*  It needs the object rotation of each parent: it supports the `vqf`, `vqsf`, `vqvf`, and `vqmf` runtime transform types (and `automatic`). The product of these types multiplies the rotations as quaternions, the VQM scale/shear matrix holds the shear. It does not support `matrix3x4f`: a matrix mixes the rotation with the scale and the shear. It does not support a custom transform adapter: ACL cannot read its rotation.
*  It does not support the `relative` base format: that format multiplies the whole transforms, but the rotations and the translations are in different spaces. Use `additive0` or `additive1`.

A rotation of a parent moves its children but does not rotate them: the subtree is not rigid with respect to the parent. Thus the bit rate search cannot cache the transform of a child relative to its parent, as it does in local space. With the `vqf` and `vqsf` types, the search caches the rotation of each child and the sum of the translations below the parent. With the `vqvf` and `vqmf` types, the search multiplies each transform chain, and compression is slower.

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
