# Transform error metric

A proper error metric is central to every animation compression algorithm and ACL is no different. The general technique implemented is the one described on this [blog post](https://nfrechette.github.io/2016/11/01/anim_compression_accuracy/) and the various implementations as well as the interfaces live [here](../includes/acl/compression/transform_error_metrics.h).

Some care must be taken when selecting which error metric to use. If the error it calculates isn't representative of how it would be calculated in the host game engine, the resulting visual fidelity might suffer. ACL implements a number of popular implementations and you are free to implement and use your own. The `qvvf_transform_error_metric` error metric is a sensible default.

## qvvf_transform_error_metric

This implementation will use `rtm::qvvf` (which implements Vector-Quaternion-Vector (*VQV*) arithmetic) to calculate the error both when scale is present and when it isn't and for both local and object space. It is a solid default and it should handle very well most clips in the wild. However take note that because it uses a simple `vector3` to represent the 3D scale component, it cannot properly handle skew and shear that arises as a result of combining multiple transforms in a bone chain. In practice it rarely matters and if you truly need this, an error metric that uses matrices when scale is present might perform better or by using Vector-Quaternion-Matrix (*VQM*) arithmetic.

## qvvf_matrix3x4f_transform_error_metric

This implementation uses `rtm::qvvf` when there is no scale in both local and object space and as well as local space when there is scale. This is generally safe because there is no skew or shear present in the transform. However, when scale is present the object space error metric will convert the transforms into `rtm::matrix3x4f` in order to combine them into the final object space transform for the bone. This properly handles 3D scale but due to numerical accuracy constraints the error can accumulate to unacceptable levels when very large or very small scale is present and combined with very large translations.

## additive_qvvf_transform_error_metric

This implementation is based on the `qvvf_transform_error_metric` and handles additive and regular animation clips. See [here](additive_clips.md) for the various additive operations supported and which one to select for your game engine.

For the purpose of measuring the error, we ignore object space additive blending. It typically does not play a significant role in the resulting error because the pose is fundamentally very close. As such, the additive clip is always applied in local space to the base clip.

If your game engine performs additive blending in a way that ACL does not support, you can modify the sources (and hopefully submit a pull request) to include it or you can implement your own error metric. It is fairly simple, just follow the examples already present in the code.

## Implementing your own error metric

In order to implement your own error metric you need to figure out how your host game engine combines the local space bone transforms into object space in order to do the same. Once you have that information, implement a class that derives from the `itransform_error_metric` interface. You can use the other error metrics as examples. You will then be able to provide it to the compression algorithm by feeding it to the `CompressionSettings`.

Fundamentally the interface requires you to implement two versions of the error metric: with or without scale. The variants with no scale are only present as an optimization to avoid unnecessary computation when a clip does not contain any scale. It is common enough to warrant an optimized code path.

If your error metric uses a different type than `rtm::qvvf`, you can implement the other functions as needed. See the interface for details.

