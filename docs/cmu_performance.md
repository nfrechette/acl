# Carnegie-Mellon University database performance

|                       | v1.1.0     | v1.0.0     | v0.8.0     |
| --------------------- | ---------- | ---------- | ---------- |
| **Compressed size**   | 67.06 MB   | 67.02 MB   | 67.02 MB   |
| **Compression ratio** | 21.31 : 1  | 21.33 : 1  | 21.33 : 1  |
| **Compression time**  | 35m 19.96s | 37m 47.76s | 36m 27.22s |
| **Compression speed**  | 690.43 KB/sec | 645.43 KB/sec | 669.20 KB/sec |
| **Max error**         | 0.0725 cm  | 0.0725 cm  | 0.0703 cm  |
| **Error 99<sup>th</sup> percentile** | 0.0090 cm | 0.0090 cm | 0.0090 cm |
| **Samples below error threshold** | 99.83 % | 99.83 % | 99.83 % |

# Data and method used

To compile the statistics, the [animation database from Carnegie-Mellon University](http://mocap.cs.cmu.edu/) is used.
The raw animation clips in FBX form can be found on the Unity asset store [here](https://www.assetstore.unity3d.com/en/#!/content/19991).
They were converted to the [ACL file format](the_acl_file_format.md) using the [fbx2acl](../tools/fbx2acl) script. Data available upon request, it is far too large for GitHub.

*  Number of clips: **2534**
*  Sample rate: **24 FPS**
*  Total duration: **09h 49m 37.58s**
*  Raw size: **1429.38 MB** (10x float32 * num bones * num samples)

ACL supports various compression methods but only the overall best variant will be tracked here (see `get_default_compression_settings()` for details).

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md) with the default error threshold of **0.01cm**. The **99th** percentile and the number of samples below the error threshold are calculated by measuring the error on every bone at every sample.

The performance of ACL in Unreal Engine 4 is tracked by the plugin [here](https://github.com/nfrechette/acl-ue4-plugin/blob/develop/Docs/cmu_performance.md).

# Results in images

![Compression ratio VS max error per clip](images/acl_cmu_compression_ratio_vs_max_error.png)


![Compression ratio by clip duration](images/acl_cmu_compression_ratio_by_duration.png)
![Compression ratio by clip duration (shortest 100)](images/acl_cmu_compression_ratio_by_duration_shortest_100.png)
![Compression ratio distribution](images/acl_cmu_compression_ratio_distribution.png)
![Compression ratio distribution (bottom 10%)](images/acl_cmu_compression_ratio_distribution_bottom_10.png)
![Compression ratio histogram](images/acl_cmu_compression_ratio_histogram.png)


![Max error by clip duration](images/acl_cmu_max_clip_error_by_duration.png)
![Max error distribution](images/acl_cmu_max_error_distribution.png)
![Max error per clip histogram](images/acl_cmu_max_error_histogram.png)


![Distribution of the error for every bone at every key frame](images/acl_cmu_exhaustive_error.png)
![Distribution of the error for every bone at every key frame (top 10%)](images/acl_cmu_exhaustive_error_top_10.png)

![Distribution of clip durations](images/acl_cmu_clip_durations.png)
