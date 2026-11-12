# Transform error metric

A good error metric is necessary for each animation compression algorithm, and ACL is no different. ACL uses the technique that this [blog post](https://nfrechette.github.io/2016/11/01/anim_compression_accuracy/) describes. ACL measures the error on a rigid shell around each transform: a sphere at a certain distance from the transform (the `shell_distance` of the track).

To measure the error of a clip, compression must know two things:

*  How your runtime composes the local space transforms into object space: the **runtime transform type**. A [transform adapter](../includes/acl/compression/transform_adapters.h) gives the operations of this type.
*  How to measure the error between a raw and a lossy transform: the **transform metric**. See [transform_metrics.h](../includes/acl/compression/transform_metrics.h).

Select the runtime transform type that your runtime uses. When the error that ACL measures is not the error of your runtime, the visual quality can be lower.

## Runtime transform types

`compression_settings::transform_runtime_type` selects the runtime transform type (`transform_runtime_type_t`):

*  `vqf`: a rotation and a translation, stored as `rtm::vqvf`. The scale is not used.
*  `vqsf`: a rotation, a translation, and a uniform scale, stored as `rtm::vqsf`. You can select it only with `vqsf` raw tracks.
*  `vqvf`: a rotation, a translation, and a 3D scale, stored as `rtm::vqvf`. The product of two transforms discards the shear, as in many game engines.
*  `vqmf`: a rotation, a translation, and a 3x3 scale/shear matrix, stored as `rtm::vqmf`. The product keeps the shear. Thus the object space transforms are exact.
*  `matrix3x4f`: a 3x4 affine matrix, stored as `rtm::matrix3x4f`. The product keeps the shear.
*  `custom`: your own type. Compression uses `compression_settings::custom_transform_adapter`.
*  `automatic` (the default): compression selects the type from the raw tracks. It selects `vqsf` for `vqsf` raw tracks when the additive base (if any) has a uniform scale. Otherwise, it selects `vqmf`.

When a clip has no scale, each built-in runtime transform type gives the same transforms. Thus compression uses `vqf`, the fastest type, when the transform metric supports it. Compression makes this selection for each segment: a segment without scale uses `vqf`, also when other segments of the clip have scale.

Compression always uses a custom transform adapter, also in a segment without scale. Your transforms can use data that the source scale does not show. For example, your adapter can add a value that you compress in another track.

Compression always converts `rtm::vqvf` source transforms into the runtime transform type. With VQ raw tracks (`vqf`), the source scale is 1.0, or 0.0 in an `additive1` clip. With VQS raw tracks (`vqsf`), the source scale holds the uniform scale in [xyz].

## Transform metrics

`compression_settings::transform_metric` selects the transform metric. When it is `nullptr` (the default), compression uses the default metric of the runtime transform type:

*  `precise_transform_metric` with `vqf` and `vqsf`. It finds the point on the sphere with the largest error and it computes with float64. With `vqf`, the result is exact. With `vqsf`, when the raw and lossy scales are not equal, the result can be above the exact error. The difference is not more than `|s1 - s0| * shell_distance`.
*  `shell_transform_metric` with `vqvf`, `vqmf`, `matrix3x4f`, and `custom`. It transforms points on the sphere with the raw and lossy transforms and returns the largest distance. Without scale (`vqf`), it uses two points. Otherwise, it uses three points: one on each axis. The result is an approximation of the error.

`shell_transform_metric` supports each runtime transform type. `precise_transform_metric` supports only `vqf` and `vqsf`. Compression returns an error when the metric does not support the runtime transform type.

## Additive clips

ACL applies the additive to the base on the `rtm::vqvf` source transforms. Then it converts the result into the runtime transform type. See [here](additive_clips.md) for the additive formats and how to select the one for your game engine.

For the purpose of measuring the error, we ignore object space additive blending. It typically does not play a significant role in the resulting error because the pose is fundamentally very close. As such, the additive clip is always applied in local space to the base clip.

## Measuring the error of compressed tracks

To measure the error as compression does, give your compression settings to `calculate_compression_error`. An additive clip also needs its additive base and its additive format:

```c++
const track_error error = calculate_compression_error(allocator, raw_tracks, context, settings);
const track_error additive_error = calculate_compression_error(allocator, raw_tracks, context, settings, additive_base_tracks, additive_format);
```

[Pre-processing](pre_processing_raw_tracks.md) uses the same settings: `pre_process_settings_t::transform_runtime_type`, `custom_transform_adapter`, and `transform_metric`. Use the same values for pre-processing and compression.

## Implementing your own runtime transform type

Find how your game engine combines the local space transforms into object space. Then implement a class that derives from `itransform_adapter`. The built-in transform adapters in [transform_adapters.h](../includes/acl/compression/transform_adapters.h) are examples. Then select `transform_runtime_type_t::custom` and set `compression_settings::custom_transform_adapter`.

The adapter gives these operations:

*  `get_transform_size`: the size of your transform type. Its alignment must be 16 bytes or less.
*  `convert_transforms`: converts `rtm::vqvf` source transforms into your type. When your type is `rtm::vqvf`, return false from `needs_conversion`. Compression then does not call `convert_transforms`.
*  `identity` and `mul`: the identity transform and the product of two transforms. The output of `mul` can be one of its inputs.
*  `local_to_object_space`: optional, the default implementation uses `mul`.
*  `mul_point3_x4`: transforms four points. `shell_transform_metric` uses it.
*  `is_associative`: whether or not the product of your type is associative.

When the product is associative, compression keeps the object space transforms in a cache, and it updates only what changes. When the product is not associative, compression must multiply the full transform chain for each bit rate that it tests. This is slower.

With a custom transform adapter, compression calls the adapter through its virtual functions. With a built-in transform adapter and a built-in transform metric, compression uses code without virtual function calls. This code is faster.

## Implementing your own transform metric

Implement a class that derives from `itransform_metric`, and set `compression_settings::transform_metric`. The built-in transform metrics in [transform_metrics.h](../includes/acl/compression/transform_metrics.h) are examples.

*  `is_supported`: returns whether or not the metric can measure transforms of a transform adapter.
*  `calculate_error_squared`: returns the squared error between the raw and the lossy transform. Use the transform adapter in the arguments to read the transforms.

Compression calls your transform metric through its virtual functions. This is also true for an instance of a built-in metric that you set in the settings. To use the faster code of a built-in metric, keep `transform_metric` at `nullptr` and use the default metric.

## Legacy error metrics (deprecated)

A legacy error metric (`itransform_error_metric`) combines the runtime transform type and the error measurement in one interface. These parts of the API are deprecated, and ACL 2.3 will remove them:

*  `compression_settings::error_metric` and `pre_process_settings_t::error_metric`
*  `itransform_error_metric` and the legacy error metrics: `vqvf_transform_error_metric`, `vqvf_precise_transform_error_metric`, `vqvf_matrix3x4f_transform_error_metric`, and `additive_vqvf_transform_error_metric`
*  The `calculate_compression_error` overloads that take an `itransform_error_metric`: use the overloads that take the compression settings

When you set a legacy error metric in the settings, you cannot set `transform_runtime_type`, `custom_transform_adapter`, or `transform_metric`.

To migrate, select the runtime transform type of your legacy error metric:

| Legacy error metric | Runtime transform type | Transform metric |
| --- | --- | --- |
| `vqvf_transform_error_metric` and its additive variant | `vqvf` | `nullptr` (shell) |
| `vqvf_matrix3x4f_transform_error_metric` | `matrix3x4f` | `nullptr` (shell) |
| `vqvf_precise_transform_error_metric` | `vqf` or `automatic` (without scale) | `nullptr` (precise) |
| Your own error metric | `custom` with your transform adapter | your transform metric, or `nullptr` (shell) |

With the runtime transform type, compression applies the additive base itself. You do not need a variant for each additive format.

The bit rate algorithm v2 supports the legacy error metrics that return true from `supports_bit_rate_algorithm_v2`. Of the built-in legacy error metrics, only `vqvf_matrix3x4f_transform_error_metric` returns false. The other legacy error metrics, and your own by default, use the slower bit rate algorithm v1. With the runtime transform type, compression always uses the bit rate algorithm v2.
