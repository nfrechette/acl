# Matinee fight scene performance

To compile the statistics, the [Matinee fight scene](http://nfrechette.github.io/2017/10/05/acl_in_ue4/) is used.

*  Number of clips: **5**
*  Sample rate: **30 FPS**
*  Cinematic duration: **66 seconds**
*  Main Trooper has over **550** bones

For ACL and Unreal 4, the error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).

*  [ACL](fight_scene_performance.md#acl)
*  [Unreal 4](fight_scene_performance.md#unreal-4)

# ACL

Statistics for ACL are being generated with a custom integration into Unreal **4.15**. The variant profiled uses uniform sampling with variable bit rate and range reduction enabled. Every clip uses an error threshold of **0.01cm (0.1mm)**. The max error is the error reported by Unreal from their own error metric.

| ACL v0.5 | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total |
| -------- | --------- | --------- | --------- | --------- | ------------ | ----- |
| **Compressed size (KB)** | 287.33 | 303.67 | 303.71 | 313.93 | 7496.00 | 8704.64 |
| **Raw size (KB)** | 5508.05 | 5352.89 | 5430.47 | 5508.05 | 42075.43 | 63874.88 |
| **Compression ratio** | 19.17 : 1 | 17.63 : 1 | 17.88 : 1 | 17.55 : 1 | 5.61 : 1 | 7.34 : 1 |
| **Max error (cm)** | 0.245 | 0.242 | 0.178 | 0.188 | 0.199 | |
| **Compression time (s)** | 8.85 | 13.81 | 12.45 | 14.87 | 72.67 | 122.66 |

# Unreal 4

## As packaged

When you install locally the Matinee fight scene from the marketplace, it has compression settings already pre-selected. These are the numbers from those default settings.

| UE 4.15 | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total |
| ------- | --------- | --------- | --------- | --------- | ------------ | ----- |
| **Compressed size (KB)** | 194.97 | 268.34 | 277.48 | 304.56 | 24741.00 | 25786.35 |
| **Raw size (KB)** | 5508.05 | 5352.89 | 5430.47 | 5508.05 | 42075.43 | 63874.88 |
| **Compression ratio** | 28.25 : 1 | 19.95 : 1 | 19.57 : 1 | 18.09 : 1 | 1.70 : 1 | 2.48 : 1 |
| **Max error (cm)** | 0.369 | 0.302 | 0.302 | 0.326 | 0.353 | |
| **Compression time (s)** | 6.67 | 5.00 | 4.57 | 6.92 | 286.86 | 310.02 |

## Automatic

Using the automatic compression with default settings.

| UE 4.15 | Trooper 1 | Trooper 2 | Trooper 3 | Trooper 4 | Main Trooper | Total |
| ------- | --------- | --------- | --------- | --------- | ------------ | ----- |
| **Compressed size (KB)** | 121.60 | 165.84 | 175.12 | 186.32 | 21686.00 | 22334.88 |
| **Raw size (KB)** | 5508.05 | 5352.89 | 5430.47 | 5508.05 | 42075.43 | 63874.88 |
| **Compression ratio** | 45.30 : 1 | 32.28 : 1 | 31.01 : 1 | 29.56 : 1 | 1.94 : 1 | 2.86 : 1 |
| **Max error (cm)** | 0.345 | 0.358 | 0.303 | 0.320 | 0.399 | |
| **Compression time (s)** | 414.39 | 347.18 | 338.16 | 386.80 | 8688.41 | 10174.94 |
