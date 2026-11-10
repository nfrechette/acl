# Transform error metric

A proper error metric is central to every animation compression algorithm and ACL is no different. The general technique implemented is the one described on this [blog post](https://nfrechette.github.io/2016/11/01/anim_compression_accuracy/) and the various implementations as well as the interfaces live [here](../includes/acl/compression/transform_error_metrics.h).

Some care must be taken when selecting which error metric to use. If the error it calculates isn't representative of how it would be calculated in the host game engine, the resulting visual fidelity might suffer. ACL implements a number of popular implementations and you are free to implement and use your own.

When `compression_settings::error_metric` is `nullptr` (the default), compression uses `vqmf_transform_error_metric`, or `additive_vqmf_transform_error_metric` for the additive format of the clip. Pre-processing does the same when `pre_process_settings_t::error_metric` is `nullptr`. To measure the error of the compressed tracks (for example with `calculate_compression_error`), use the same error metric.

## Runtime transform types

An error metric uses a runtime transform type to compose the local space transforms into object space and to measure the error. `itransform_error_metric::get_runtime_type()` returns this type (`transform_runtime_type8`):

*  `vqvf`: a rotation, a translation, and a 3D scale. The product of two transforms discards the shear. Thus the product is not associative.
*  `vqmf`: a rotation, a translation, and a 3x3 scale/shear matrix. The product of two transforms keeps the shear. Thus the object space transforms are exact, and the product is associative.
*  `custom`: a type that ACL does not know. ACL uses it only through the functions of the error metric.

Without scale, the built-in error metrics use `rtm::vqvf` as a VQ transform (a rotation and a translation): they ignore the scale.

The error metric functions always receive `rtm::vqvf` source transforms, also with VQ raw tracks (`vqf`) and VQS raw tracks (`vqsf`). For a VQ track, ACL gives a scale of 1.0, or 0.0 in an `additive1` clip. For a VQS track, ACL gives the uniform scale in [xyz].

The bit rate algorithm uses the runtime type. With scale and `vqvf`, it must compose each transform chain again for each permutation that it tests, because the product is not associative. With scale and `vqmf`, it uses the same cached transforms as a clip without scale.

## vqmf_transform_error_metric

This implementation is the default. Without scale, it uses `rtm::vqvf` as a VQ transform. With scale, it converts the source `rtm::vqvf` transforms into `rtm::vqmf` (Vector-Quaternion-Matrix (*VQM*) arithmetic). The 3x3 scale/shear matrix keeps the shear that non-uniform scale introduces in a bone chain. Thus the object space transforms are exact.

If your game engine composes transforms with VQV arithmetic and discards the shear, the object space pose of your engine can be different from the pose that this error metric measures. Use `vqvf_transform_error_metric` in that case.

## additive_vqmf_transform_error_metric

This implementation is based on the `vqmf_transform_error_metric` and handles additive and regular animation clips. The additive applies to the `rtm::vqvf` source transforms, before the conversion to `rtm::vqmf`. See [here](additive_clips.md) for the various additive operations supported and which one to select for your game engine.

## vqvf_transform_error_metric

This implementation will use `rtm::vqvf` (which implements Vector-Quaternion-Vector (*VQV*) arithmetic) to calculate the error both when scale is present and when it isn't and for both local and object space. Because it uses a simple `vector3` to represent the 3D scale component, it cannot properly handle skew and shear that arises as a result of combining multiple transforms in a bone chain. Use it when your game engine composes transforms the same way. Otherwise, use `vqmf_transform_error_metric`.

## vqvf_matrix3x4f_transform_error_metric

This implementation uses `rtm::vqvf` when there is no scale in both local and object space and as well as local space when there is scale. This is generally safe because there is no skew or shear present in the transform. However, when scale is present the object space error metric will convert the transforms into `rtm::matrix3x4f` in order to combine them into the final object space transform for the bone. This properly handles 3D scale but due to numerical accuracy constraints the error can accumulate to unacceptable levels when very large or very small scale is present and combined with very large translations.

## additive_vqvf_transform_error_metric

This implementation is based on the `vqvf_transform_error_metric` and handles additive and regular animation clips. See [here](additive_clips.md) for the various additive operations supported and which one to select for your game engine.

For the purpose of measuring the error, we ignore object space additive blending. It typically does not play a significant role in the resulting error because the pose is fundamentally very close. As such, the additive clip is always applied in local space to the base clip.

If your game engine performs additive blending in a way that ACL does not support, you can modify the sources (and hopefully submit a pull request) to include it or you can implement your own error metric. It is fairly simple, just follow the examples already present in the code.

## Additive clips

ACL applies the additive to the base with `apply_additive_to_base` on the `rtm::vqvf` source transforms. Then it converts the result with `convert_transforms` into the transform type of the error metric. Thus `apply_additive_to_base` always receives and returns `rtm::vqvf` transforms.

## Implementing your own error metric

In order to implement your own error metric you need to figure out how your host game engine combines the local space bone transforms into object space in order to do the same. Once you have that information, implement a class that derives from the `itransform_error_metric` interface. You can use the other error metrics as examples. You will then be able to provide it to the compression algorithm by feeding it to the `CompressionSettings`.

Fundamentally the interface requires you to implement two versions of the error metric: with or without scale. The variants with no scale are only present as an optimization to avoid unnecessary computation when a clip does not contain any scale. It is common enough to warrant an optimized code path.

If your error metric uses a different type than `rtm::vqvf`, you can implement the other functions as needed. See the interface for details. Return `transform_runtime_type8::custom` from `get_runtime_type()` unless your error metric uses `rtm::vqvf` or `rtm::vqmf` with scale.

