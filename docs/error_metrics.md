# Skeleton error metric

A proper error metric is central to every animation compression algorithm and ACL is no different. The general technique implemented is the one described on this [blog post](http://nfrechette.github.io/2016/11/01/anim_compression_accuracy/) and the implementations as well as the interface live [here](../includes/acl/compression/skeleton_error_metric.h).

Some care must be taken when selecting which error metric to use. If the error it calculates isn't representative of how it would be calculated in the host game engine, the resulting visual fidelity might suffer. ACL implements a number of popular implementations and you are free to implement and use your own.

## TransformErrorMetric

This implementation will use `Transform_32` (which implements Vector-Quaternion-Vector (*VQV*) arithmetic) to calculate the error both when scale is present and when it isn't as well as for local and object space. It is a solid default and it should handle very well most clips in the wild. However take note that because it uses a simple `vector3` to represent the 3D scale component, it cannot properly handle skew and shear that arises as a result of combining multiple transforms in a bone chain. In practice it rarely matters and if you truely need this, an error metric that uses matrices when scale is present might perform better or by using Vector-Quaternion-Matrix (*VQM*) arithmetic.

## TransformMatrixErrorMetric

This implementation uses `Transform_32` when there is no scale in both local and object space and as well as local space when there is scale. This is generally safe because there is no skew or shear present in the transform. However, when scale is present the object space error metric will convert the transforms into `AffineMatrix_32` in order to combine them into the final object space transform for the bone. This properly handles 3D scale but due to numerical accuracy the error can accumulate to unacceptable levels when very large or very small scale is present combined with very large translations.

## AdditiveTransformErrorMetric

This implementation is based on the `TransformErrorMetric` and handles additive and regular animation clips. See [here](additive_clips.md) for the various additive operations supported and which one to select for your game engine.

For the purpose of measuring the error, we ignore mesh or object space additive blending. It typically does not play a significant role in the resulting error because the pose is fundamentally very close. As such, the additive clip is always applied in local space to the base clip.

If your game engine performs additive blending in a way that ACL does not support, you will have to modify the sources (and hopefully submit a pull request) to include it. It is fairly easy and simple, just follow the examples already in the code.

## Implementing your own error metric

In order to implement your own error metric you need to figure out how your host game engine combines the local space bone transforms into object space and simply do the same. Once you have that information, implement a class that derives from the `ISkeletalErrorMetric` interface. You can use the other error metrics as examples. You will then be able to provide it to the compression algorithm by feeding it to the `CompressionSettings` ([code](../includes/acl/compression/compression_settings.h)).

Fundamentally the interface requires you to implement four versions of the error metric: with and without scale, and in local or object space. The variants with no scale are only present as an optimization to avoid unnecessary computation when none is present since it is a very common scenario.

Note that if you need to support additive animation clips, you will need to implement your own error metric that properly applies the base animation clip according to your own game engine logic.
