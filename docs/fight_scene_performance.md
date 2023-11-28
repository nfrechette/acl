# Matinee fight scene performance

|                       | v2.1.0 | v2.0.0 | v1.3.0 |
| --------------------- | --------- | --------- | --------- |
| **Compressed size**   | 8.06 MB | 8.18 MB | 8.18 MB |
| **Compression ratio** | 7.74 : 1 | 7.62 : 1 | 7.63 : 1 |
| **Compression time**  | 5.27s | 3.86s | 4.89s |
| **Compression speed**  | 12125.77 KB/sec | 16534.75 KB/sec | 13074.59 KB/sec |
| **Max error**         | 0.0235 cm | 0.0635 cm | 0.0634 cm |
| **Error 99<sup>th</sup> percentile** | 0.0094 cm | 0.0201 cm | 0.0201 cm |
| **Samples below error threshold** | 99.94 % | 97.91 % | 97.90 % |

*Note: Starting with v1.3.0 measurements have been made with a Ryzen 2950X CPU while prior versions used an Intel i7 6850K.*

# Data and method used

To compile these statistics, the [Matinee fight scene](https://nfrechette.github.io/2017/10/05/acl_in_ue4/) is used.

*  Number of clips: **5**
*  Sample rate: **30 FPS**
*  Cinematic duration: **66 seconds**
*  *Troopers* 1-4 have **71** bones and the *Main Trooper* has **551** bones
*  Raw size: **62.38 MB** (10x float32 * num bones * num samples)

ACL supports various compression methods but only the overall best variant will be tracked here (see `get_default_compression_settings()` for details).

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md) with the default error threshold of **0.01cm**. The **99th** percentile and the number of samples below the error threshold are calculated by measuring the error on every bone at every sample.

The performance of ACL in Unreal Engine is tracked by the plugin [here](https://github.com/nfrechette/acl-ue4-plugin/blob/develop/Docs/fight_scene_performance.md).
