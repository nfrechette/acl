# Matinee fight scene performance

To compile the statistics, the [Matinee fight scene](http://nfrechette.github.io/2017/10/05/acl_in_ue4/) is used.

*  Number of clips: **5**
*  Sample rate: **30 FPS**
*  Cinematic duration: **66 seconds**
*  Main Trooper has over **550** bones

For ACL and Unreal 4, the error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).

*  [ACL](fight_scene_performance.md#acl)
*  [Unreal 4](fight_scene_performance.md#unreal-4)

|                            | ACL v0.8 | UE 4.15 Automatic | UE 4.15 Packaged |
| -------------------------- | -------- | ----------------- | ---------------- |
| **Total compressed size** | 8.49 MB | 21.81 MB | 25.18 MB |
| **Total raw size**       | 62.38 MB | 62.38 MB | 62.38 MB |
| **Total compression ratio**    | 7.35 : 1 | 2.86 : 1 | 2.48 : 1 |
| **Max error**                  | 0.217 cm | 0.399 cm | 0.369 cm |
| **Total compression time**     | 1m 5s | 2h 49m 35s | 5m 10s |



# ACL

Statistics for ACL are being generated with a custom integration into Unreal **4.15**. The variant profiled uses uniform sampling with variable bit rate and range reduction enabled. Every clip uses an error threshold of **0.01cm (0.1mm)**. The max error is the error reported by Unreal from their own error metric.

| ACL v0.8              | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total    |
| --------------------- | --------- | --------- | --------- | --------- | ------------ | -------- |
| **Compressed size**   | 287.05 KB | 302.89 KB | 303.12 KB | 313.85 KB | 7.31 MB      | 8.49 MB  |
| **Raw size**          | 5508 KB   | 5353 KB   | 5430 KB   | 5508 KB   | 41.09 MB     | 62.38 MB |
| **Compression ratio** | 19.19 : 1 | 17.67 : 1 | 17.92 : 1 | 17.55 : 1 | 5.62 : 1     | 7.35 : 1 |
| **Max error**         | 0.187 cm  | 0.213 cm  | 0.197 cm  | 0.188 cm  | 0.217 cm     |          |
| **Compression time**  | 5.45s     | 7.28s     | 6.61s     | 8.21s     | 36.97s       | 1m 5s    |

# Unreal 4

## As packaged

When you install locally the Matinee fight scene from the marketplace, it has compression settings already pre-selected. These are the numbers from those default settings.

| UE 4.15               | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total    |
| --------------------- | --------- | --------- | --------- | --------- | ------------ | -------- |
| **Compressed size**   | 194.97 KB | 268.34 KB | 277.48 KB | 304.56 KB | 24.16 MB     | 25.18 MB |
| **Raw size**          | 5508 KB   | 5353 KB   | 5430 KB   | 5508 KB   | 41.09 MB     | 62.38 MB |
| **Compression ratio** | 28.25 : 1 | 19.95 : 1 | 19.57 : 1 | 18.09 : 1 | 1.70 : 1     | 2.48 : 1 |
| **Max error**         | 0.369 cm  | 0.302 cm  | 0.302 cm  | 0.326 cm  | 0.353 cm     |          |
| **Compression time**  | 6.67s     | 5.00s     | 4.57s     | 6.92s     | 4m 46s       | 5m 10s   |

## Automatic

Using the automatic compression with default settings. As described [here](http://nfrechette.github.io/2017/01/11/anim_compression_unreal4/), the automatic compression tries many algorithms and settles on the best memory footprint that is also under the desired error threshold. The default error threshold is **1.0cm**.

| UE 4.15               | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total      |
| --------------------- | --------- | --------- | --------- | --------- | ------------ | ---------- |
| **Compressed size**   | 121.60 KB | 165.84 KB | 175.12 KB | 186.32 KB | 21.18 MB     | 21.81 MB   |
| **Raw size**          | 5508 KB   | 5353 KB   | 5430 KB   | 5508 KB   | 41.09 MB     | 62.38 MB   |
| **Compression ratio** | 45.30 : 1 | 32.28 : 1 | 31.01 : 1 | 29.56 : 1 | 1.94 : 1     | 2.86 : 1   |
| **Max error**         | 0.345 cm  | 0.358 cm  | 0.303 cm  | 0.320 cm  | 0.399 cm     |            |
| **Compression time**  | 6m 54s    | 5m 47s    | 5m 38s    | 6m 27s    | 2h 24m 48s   | 2h 49m 35s |
