# Decompression performance

In order to measure the decompression performance, the [acl_decompressor](../tools/acl_decompressor) tool is used to extract the relevant metrics and a [python script](../tools/graph_generation) is used to parse them.

Here are the clips we measure on:

*  The 42 [regression clips](../test_data) from [CMU](cmu_performance.md) database
*  The 5 clips from the [Matinee fight](fight_scene_performance.md) scene

Note that the data is not yet conveniently packaged.

Here are the platforms we measure on:

*  Desktop: Intel i7-6850K @ 3.8 GHz (for v1.2 and earlier)
*  Desktop: Ryzen 2950X @ 3.5 GHz (for v1.3 and later)
*  Laptop: MacBook Pro mid 2014 @ 2.6 GHz
*  Phone: Android Pixel 3 @ 2.5 GHz
*  Tablet: iPad Pro 10.5 inch @ 2.39 GHz

We only show a few compilers and architectures to keep the graphs readable.

**Unless otherwise specified, the results are from release [2.0.0](https://github.com/nfrechette/acl/releases/tag/v2.0.0)**

## Uniformly sampled algorithm

The uniformly sampled algorithm offers consistent performance regardless of the playback direction. Shown here is the median performance of `decompress_pose` with a cold CPU cache for **3** clips with forward, backward, and random playback on the *iPad*.

| Clip Name    | Forward   | Backward  | Random    |
| ------------ | --------- | --------- | --------- |
| 104_30       | 0.978 us  | 1.010 us  | 1.014 us  |
| Trooper_1    | 2.432 us  | 2.258 us  | 2.402 us  |
| Trooper_Main | 30.953 us | 30.658 us | 30.776 us |

As can be seen, the performance is consistent regardless of the playback direction. It also remains consistent regardless of the clip sample rate and the clip playback rate.

### decompress_pose

This function decompresses a whole pose in one go. Shown here is forward playback with a cold CPU cache.

![Uniform decompress_pose CMU Performance](images/acl_decomp_uniform_pose_cmu.png)
![Uniform decompress_pose Matinee Performance](images/acl_decomp_uniform_pose_matinee.png)

Here is the delta with the previous version:

![Uniform decompress_pose CMU Speed Delta](images/acl_decomp_delta_uniform_pose_cmu_speed.png)

![Uniform decompress_pose Matinee Speed Delta](images/acl_decomp_delta_uniform_pose_matinee_speed.png)

### decompress_bone

This function decompresses a single bone. To generate the graphs, we measure the cost of decompressing a whole pose one bone at a time. Shown here is forward playback with a cold CPU cache.

![Uniform decompress_bone CMU Performance](images/acl_decomp_uniform_bone_cmu.png)

![Uniform decompress_bone Matinee Performance](images/acl_decomp_uniform_bone_matinee.png)

Here is the delta with the previous version:

![Uniform decompress_bone CMU Speed Delta](images/acl_decomp_delta_uniform_bone_cmu_speed.png)

![Uniform decompress_bone Matinee Speed Delta](images/acl_decomp_delta_uniform_bone_matinee_speed.png)
