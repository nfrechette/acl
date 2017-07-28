# ACL vs Unreal 4 vs Unity 5

In order to keep the progress of ACL grounded in the real world, it is imperative that we compare it against the competition out there. In order to do so, as many statistics as possible were extracted from Unreal 4 and Unity 5.

To compile the statistics, the animation database from Carnegie-Mellon University is used.

*  Number of clips: 2534
*  Sample rate: 24 FPS
*  Total duration: 09h 48m 18.89s
*  Raw size: 998.34 MB (7x float32 * num bones * num samples)

For ACL and Unreal 4, the error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](https://github.com/nfrechette/acl/blob/develop/docs/error_metrics.md).

**TODO: Show stats and graphs**

## ACL

Statistics for ACL are being generated with the `acl_compressor` tool found [here](https://github.com/nfrechette/acl/tree/develop/tools/acl_compressor). It supports various compression method, only the best will be tracked here. Every clip uses an error threshold of **0.01cm (0.1mm)**.

*  Compressed size: 105.94 MB
*  Compression ratio: 9.42 : 1
*  Max error: 0.0748 centimeters (clip 144_31)
*  Compression time: 03h 37m 06.87s (single threaded)
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

Note that you can compress any number of clips in parallel with multiple threads but each clip uses a single thread for now.

**Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0)**

See [here](https://github.com/nfrechette/acl/blob/develop/docs/performance_history.md) for a history of performance progress across the various releases.

## Unreal 4

In order to measure statistics in Unreal 4, ACL was integrated along with a small [commandlet](https://github.com/nfrechette/acl/blob/develop/tools/ue4_stats_dump) to run the necessary compression and decompression logic. Everything uses the default and automatic compression settings which performs an exhaustive search of the best compression method.

*  Compressed size: 109.93 MB
*  Compression ratio: 9.08 : 1
*  Max error: 0.0850 centimeters (clip 128_11)
*  Compression time: 04h 00m 29.06s (single threaded)

Sadly the Unreal 4 compression logic does not support multi-threading and must be run from the main thread.

**Results from Unreal 4.15.0**

## Unity 5

Sadly I have not yet managed to find a way to implement a custom error metric in Unity nor how to even sample a clip procedurally in both the raw and/or compressed form. This makes comparing the results somewhat difficult. However I did manage to extract the following statistics with the default compression settings and the `optimal` compression algorithm:

*  Number of muscle curves: 329153
*  Number of constant curves: 119173
*  Number of dense curves: 203526
*  Number of stream curves: 6454
*  Editor memory footprint: 1.22 GB
*  Win32 memory footprint: 282 MB
*  Max error: unknown
*  Compression time: unknown

**Results from Unity 5.6.1f1**

*Contributions welcome on this topic if you are familiar with Unity*
