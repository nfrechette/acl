# Default sub-track handling

When a joint track is compressed, it is split into 3 sub-tracks: rotation, translation, scale. Each of these can be in one of three categories: animated (N samples retained), constant (1 sample retained), or default (0 sample retained). Because default sub-tracks do not store a sample, they are the most compact and leveraging them to their full extent allows for significant memory savings.

The default values are the identity but that is not always optimal. A better choice is whatever default value your integrating code base uses. A common choice is the character bind pose (e.g. T-stance).

Note that some additive formats do not use a scale of 1.0 as their default value and for those clips, overriding the default value will be required for proper usage.

## Tuning the compression

Telling ACL what default values you use is simple. When you create the raw tracks array, simply specify it in the `track_desc_transformf` by setting the `default_value` member. ACL will use that value during compression but it will *NOT* be included in the compressed data stream since we do not store default samples. You will need to provide it again during decompression.

## Handling decompression

If you use a default value that differs from the identity, you will need to provide it to ACL during decompression. This can be done in a number of ways through the `track_writer` implementation you provide when sampling tracks.

These are the members you must override and implement:

```c++
//////////////////////////////////////////////////////////////////////////
// If default sub-tracks aren't skipped, a value must be written. Either
// they are constant for every sub-track (e.g. identity) or they vary per
// sub-track (e.g. bind pose).
// By default, default sub-tracks are constant and the identity.
// Must be static constexpr!
static constexpr default_sub_track_mode get_default_rotation_mode() { return default_sub_track_mode::constant; }
static constexpr default_sub_track_mode get_default_translation_mode() { return default_sub_track_mode::constant; }
static constexpr default_sub_track_mode get_default_scale_mode() { return default_sub_track_mode::legacy; }

//////////////////////////////////////////////////////////////////////////
// If default sub-tracks are constant, these functions return their value.
rtm::quatf RTM_SIMD_CALL get_constant_default_rotation() const { return rtm::quat_identity(); }
rtm::vector4f RTM_SIMD_CALL get_constant_default_translation() const { return rtm::vector_zero(); }
rtm::vector4f RTM_SIMD_CALL get_constant_default_scale() const { return rtm::vector_set(1.0F); }

//////////////////////////////////////////////////////////////////////////
// If default sub-tracks are variable, these functions return their value.
rtm::quatf RTM_SIMD_CALL get_variable_default_rotation(uint32_t /*track_index*/) const { return rtm::quat_identity(); }
rtm::vector4f RTM_SIMD_CALL get_variable_default_translation(uint32_t /*track_index*/) const { return rtm::vector_zero(); }
rtm::vector4f RTM_SIMD_CALL get_variable_default_scale(uint32_t /*track_index*/) const { return rtm::vector_set(1.0F); }
```

The sub-track mode dictates how default sub-tracks are handled. A sub-track can be `skipped` and ACL won't attempt to write it (suitable when the default values are already in the output buffer), they can be `constant` and identical for all sub-tracks of that type (the default behavior, suitable for ordinary clips and additive clips), and `variable` where each sub-track can have its own default value. A value of `legacy` can be used for scale for the time being but this is meant for backwards compatibility only and will be removed in the future.

If sub-tracks aren't skipped, ACL will query what the default value is (whether constant or variable) and it will write it out to the output buffer as it would normally. Skipping sub-tracks is handy if the output buffer is pre-populated with the default values. This may or may not be faster depending on your integration and whether or not ACL needs to perform an indirection lookup with the track index.

