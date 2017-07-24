# ACL vs Unreal 4 vs Unity 5

In order to keep the progress of ACL grounded in the real world, it is imperative that we compare it against the competition out there. In order to do so, as many statistics as possible were extracted from Unreal 4 and Unity 5.

To compile the statistics, the animation database from Carnegie-Mellon University is used.

*  Number of clips: 2534
*  Sample rate: 24 FPS
*  Total duration: 490h 15m 44.40s

**TODO: Show stats and graphs**

## ACL

Statistics for ACL are being generated with the `acl_compressor` tool found [here](https://github.com/nfrechette/acl/tree/develop/tools/acl_compressor). It supports various compression method, only the best will be tracked here.

*  Raw size: 998.34 MB
*  Compressed size: 73.60 MB
*  Max error: 0.3448 centimeters (clip 132_44)
*  Compression time: 04h 56m 44.67s (single threaded)
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

Note that the compressor can run on as many cores as you'd like.

**Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0)**

See [here](https://github.com/nfrechette/acl/blob/develop/docs/performance_history.md) for a history of performance progress across the various releases.

## Unreal 4

In order to measure statistics in Unreal 4, ACL was integrated along with a small [commandlet](https://github.com/nfrechette/acl/blob/develop/tools/ue4_stats_dump) to run the necessary compression and decompression logic. Everything uses the default and automatic compression settings which performs an exhaustive search of the best compression method.

*  Raw size: 1003 MB
*  Compressed size: 109 MB
*  Max error: 5.8782 centimeters (clip 15_05)
*  Compression time: ~4 hours

Sadly the Unreal 4 compression logic does not support multi-threading and must run from the main thread.

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

*Contributions welcome on this topic if you are familiar with Unity*
