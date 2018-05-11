# Compressing a raw clip

Once you have created a [raw animation clip instance](creating_a_raw_clip.md), you are ready to compress it. In order to do so, you simply have to pick the algorithm you will use and a set of compression settings.

For now, we only implement a single algorithm: [uniformly sampled](algorithm_uniformly_sampled.md). This is a simple and excellent algorithm to use for everyday animation clips.

While we support various [rotation and vector quantization formats](rotation_and_vector_formats.md), the overall best performing are the *variable* variants. It is safe to use them for all your clips but if you do happen to run into issues with some exotic clips, you can easily fallback to less aggressive variants.

[Segmenting](http://nfrechette.github.io/2016/11/10/anim_compression_uniform_segmenting/) ensures that large clips are clip into smaller segments and compressed independently to allow a smaller memory footprint as well as faster compression and decompression.

[Range reduction](range_reduction.md) is important and also something you will want to enable for all your tracks both at the clip and segment level.

Selecting the right [error metric](error_metrics.md) is important and you will want to carefully pick the one that best approximates how your game engine performs skinning.

The last important setting to choose is the `error_threshold`. This is used in conjunction with the error metric and the virtual vertex distance from the [skeleton](creating_a_skeleton.md) in order to guarantee that a certain quality is maintained. A default value of **0.01cm** is safe to use and it most likely should never be changed unless the units you are using differ. If you do run into issues where compression artifacts are visible, in all likelihood the virtual vertex distance used on the problematic bones is not conservative enough.

```c++
CompressionSettings settings;
settings.rotation_format = RotationFormat8::QuatDropW_Variable;
settings.translation_format = VectorFormat8::Vector3_Variable;
settings.scale_format = VectorFormat8::Vector3_Variable;
settings.range_reduction = RangeReductionFlags8::AllTracks;
settings.segmenting.enabled = true;
settings.segmenting.range_reduction = RangeReductionFlags8::AllTracks;

TransformErrorMetric error_metric;
settings.error_metric = &error_metric;

UniformlySampledAlgorithm algorithm(settings);
OutputStats stats;
CompressedClip* compressed_clip = nullptr;
ErrorResult error_result = algorithm.compress_clip(allocator, clip, compressed_clip, stats);
```

You can also query the current default and recommended settings with this function: `get_default_compression_settings()`.
