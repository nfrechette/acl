# Matinee fight scene performance

|                       | v1.3.0 | v1.2.0 | v1.1.0    |
| --------------------- | --------- | --------- | --------- |
| **Compressed size**   | 8.18 MB | 8.77 MB | 8.66 MB   |
| **Compression ratio** | 7.63 : 1 | 7.11 : 1 | 7.20 : 1  |
| **Compression time**  | 4.89s | 20.27s | 1m 2.62s |
| **Compression speed**  | 13074.59 KB/sec | 3150.43 KB/sec | 1020.06 KB/sec |
| **Max error**         | 0.0634 cm | 0.0591 cm | 0.0620 cm |
| **Error 99<sup>th</sup> percentile** | 0.0201 cm | 0.0382 cm | 0.0255 cm |
| **Samples below error threshold** | 97.90 % | 94.61 % | 95.03 % |

*Note: Starting with v1.3.0 measurements have been made with a Ryzen 2950X CPU while prior versions used an Intel i7 6850K.*

# Data and method used

To compile these statistics, the [Matinee fight scene](http://nfrechette.github.io/2017/10/05/acl_in_ue4/) is used.

*  Number of clips: **5**
*  Sample rate: **30 FPS**
*  Cinematic duration: **66 seconds**
*  *Troopers* 1-4 have **71** bones and the *Main Trooper* has **551** bones
*  Raw size: **62.38 MB** (10x float32 * num bones * num samples)

ACL supports various compression methods but only the overall best variant will be tracked here (see `get_default_compression_settings()` for details).

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md) with the default error threshold of **0.01cm**. The **99th** percentile and the number of samples below the error threshold are calculated by measuring the error on every bone at every sample.

The performance of ACL in Unreal Engine 4 is tracked by the plugin [here](https://github.com/nfrechette/acl-ue4-plugin/blob/develop/Docs/fight_scene_performance.md).
