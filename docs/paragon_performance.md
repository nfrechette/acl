# Paragon database performance

|                       | v2.0.0 | v1.3.0 | v1.2.0  |
| --------------------- | ------------- | ------------- | ------------- |
| **Compressed size**   | 208.93 MB | 208.32 MB | 218.58 MB |
| **Compression ratio** | 20.47 : 1 | 20.53 : 1 | 19.56 : 1 |
| **Compression time**  | 8m 10.69s | 10m 23.05s | 28m 56.48s |
| **Compression speed**  | 8923.56 KB/sec | 7027.87 KB/sec | 2521.62 KB/sec |
| **Max error**         | 4.1029 cm | 2.8669 cm | 4.0184 cm |
| **Error 99<sup>th</sup> percentile** | 0.0099 cm | 0.0099 cm | 0.0116 cm |
| **Samples below error threshold** | 99.04 % | 99.04 % | 98.85 % |

Notes:

*  The error is unusually high and above **1 cm** for **4** exotic clips
*  Starting with v1.3.0 measurements have been made with a Ryzen 2950X CPU while prior versions used an Intel i7 6850K.
*  Even though the 'max error' appears to have increased significantly in 2.0.0, the error is not visible to the naked eye due to the exotic nature of the clip it happens in

# Data and method used

To compile these statistics, a large number of animations from [Paragon](https://www.epicgames.com/paragon) are used.

In *October 2017* the animations were manually extracted and converted to the [ACL file format](the_acl_file_format.md) losslessly. The data is sadly **NOT** available upon request.
Epic has permitted [Nicholas Frechette](https://github.com/nfrechette) to use them for research purposes only under a non-disclosure agreement.

*  Number of clips: **6558**
*  Total duration: **7h 0m 45.27s**
*  Raw size: **4276.11 MB** (10x float32 * num bones * num samples)

The data set contains among other things:

*  Lots of characters with varying number of bones
*  Animated objects of various shape and form
*  Very short and very long clips
*  Clips with unusual sample rate (as low as **2** FPS!)
*  World space clips
*  Lots of 3D scale
*  Lots of other exotic clips

ACL supports various compression methods but only the overall best variant will be tracked here (see `get_default_compression_settings()` for details).

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md) with the default error threshold of **0.01cm**. The **99th** percentile and the number of samples below the error threshold are calculated by measuring the error on every bone at every sample.

The performance of ACL in Unreal Engine 4 is tracked by the plugin [here](https://github.com/nfrechette/acl-ue4-plugin/blob/develop/Docs/paragon_performance.md).

# Results in images

![Compression ratio VS max error per clip](images/acl_paragon_compression_ratio_vs_max_error.png)

![Compression ratio distribution](images/acl_paragon_compression_ratio_distribution.png)

![Max error distribution](images/acl_paragon_max_error_distribution.png)

![Distribution of the error for every bone at every key frame](images/acl_paragon_exhaustive_error.png)

![Distribution of the error for every bone at every key frame (top 10%)](images/acl_paragon_exhaustive_error_top_10.png)

![Distribution of clip durations](images/acl_paragon_clip_durations.png)
