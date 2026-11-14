# Additive animation clips

Additive animation clips are a core tool within an animation runtime but sadly, how additive animations are blended and applied is not standard. As such, this library aims to support the most common additive modes. Contributions welcome!

The core logic for additive clips lives [here](../includes/acl/core/additive_utils.h).

## The transform space

Two independent properties tell compression how to measure the error of a clip:

*  The transform space of the clip: how a sample uses the transform hierarchy, see `transform_space_t` (see [here](../includes/acl/compression/transform_space.h)). The track array holds it: set it with `track_array::set_transform_space`.
    *  `local` (the default): each transform is relative to its parent transform.
    *  `mesh`: each transform is in object space (mesh space). It does not use its parent transform. Compression measures the error of each transform alone: the error of a parent does not move its children.
    *  `mesh_rotation`: the rotation of each transform is in object space, its translation and scale are relative to its parent transform. The rotation of a parent does not change the object rotation of its children, but the parent still moves them. Compression measures the error in object space with this rule: `object = mul(local, parent_object)`, with `local.rotation = mul(rotation, conjugate(parent_object.rotation))`.
*  The base of the clip: how a sample combines with its base clip. A `transform_base_desc_t` (see [here](../includes/acl/compression/transform_base.h)) holds the base format (`format`, see `additive_clip_format8` and the formats below) and the base tracks (`tracks`). With `none`, the clip has no base. With `custom`, the `adapter` combines them, see [below](#custom-base-format).

Compression first combines each sample with the base. Then it puts the result in object space to measure the error. The transform space does not change the compressed data: decompression returns the samples of the clip, and your runtime applies them to the base.

The base is always in local space, with the transform hierarchy of the clip: the transform space of the base tracks must be `local`. In mesh space, compression puts the base in mesh space with `rtm::vqv_mul` (the scale of a parent does not shear its children). Then it combines each sample with the base in mesh space. In mesh rotation space, compression puts only the rotations of the base in mesh space, then it combines each sample with the base: the rotations in mesh space, the translations and scales in local space. The compressed tracks keep the parent indices of the clip.

The mesh rotation space has two limits:

*  It needs the object rotation of each parent: it supports the `vqf`, `vqsf`, `vqvf`, and `vqmf` runtime transform types (and `automatic`). The product of these types multiplies the rotations as quaternions, the VQM scale/shear matrix holds the shear. It does not support `matrix3x4f`: a matrix mixes the rotation with the scale and the shear. It does not support a custom transform adapter: ACL cannot read its rotation.
*  It does not support the `relative` base format: that format multiplies the whole transforms, but the rotations and the translations are in different spaces. Use `additive0` or `additive1`.

A rotation of a parent moves its children but does not rotate them: the subtree is not rigid with respect to the parent. Thus the bit rate search cannot cache the transform of a child relative to its parent, as it does in local space. With the `vqf` and `vqsf` types, the search caches the rotation of each child and the sum of the translations below the parent. With the `vqvf` and `vqmf` types, the search multiplies each transform chain, and compression is slower.

```c++
raw_track_list.set_transform_space(transform_space_t::mesh_rotation);

transform_base_desc_t transform_base;
transform_base.format = additive_clip_format8::additive1;
transform_base.tracks = &base_track_list;

error_result result = compress_track_list(allocator, raw_track_list, settings, transform_base, out_compressed_tracks, stats);
```

A clip without a base needs only its transform space: the `compress_track_list` overload without a base uses it.

[Pre-processing](pre_processing_raw_tracks.md) and the [error measurement](error_metrics.md) (`calculate_compression_error`) also use the transform space of the track array. Give them the same base as compression (`pre_process_settings_t::transform_base`).

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

## Custom base format

When your runtime combines a clip with its base in another way, implement an `itransform_base_adapter_t` (see [here](../includes/acl/compression/transform_base_adapter.h)). Set the base format to `custom`, and set `adapter` in the base description:

```c++
class my_base_adapter final : public itransform_base_adapter_t
{
public:
    virtual const char* get_name() const override { return "my_base_adapter"; }

    // The scale of a sample without scale, 0.0 or 1.0 (defaults to 1.0)
    virtual float get_default_scale() const override { return 1.0F; }

    virtual rtm::vqvf apply_base(uint32_t transform_index, const rtm::vqvf& base_transform, const rtm::vqvf& transform) const override
    {
        // The root does not use the base
        if (transform_index == 0)
            return transform;

        return transform_add0(base_transform, transform);
    }
};

my_base_adapter base_adapter;

transform_base_desc_t transform_base;
transform_base.format = additive_clip_format8::custom;
transform_base.tracks = &base_track_list;
transform_base.adapter = &base_adapter;
```

The adapter combines one transform at a time:

*  Compression puts the base in the transform space of the clip first, as with the other formats. The base transform and the clip transform that the adapter receives are in the same space. Then compression puts the combined transforms in object space. Thus the adapter does not need the transform hierarchy.
*  The transform index lets the adapter combine some transforms in another way, for example the root.
*  The default scale is the scale of the samples of a track without scale (for example, a `vqf` track). Decompression returns it when the compressed tracks have no scale. The compressed data keeps one bit for it, thus it must be 0.0 or 1.0.
*  When the clip and the base have no scale, compression ignores the scale of the combined transform.
*  Compression calls the adapter in its inner loops, with the raw and the lossy samples. The same input must always give the same output.

The custom base format has these limits:

*  The legacy error metric (`compression_settings::error_metric`) does not support it. Compression and pre-processing return an error.
*  The deprecated `compress_track_list` overload that takes an additive base and an additive format cannot receive an adapter. It returns an error with the `custom` format.
*  A clip file can hold the `custom` format name, but not the adapter. Your tool must supply the adapter.
