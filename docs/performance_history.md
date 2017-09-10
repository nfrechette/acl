# Performance history

To compile the statistics, the animation database from Carnegie-Mellon University is used.
The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).
Every clip uses an error threshold of **0.01cm (0.1mm)**.

**TODO: Show stats and graphs**

## Results from release [0.4.0](https://github.com/nfrechette/acl/releases/tag/v0.4.0):

*  Raw size: 1000.56 MB
*  Compressed size: 82.25 MB
*  Compression ratio: 12.16 : 1
*  Max error: 0.0635 centimeters (clip 144_32)
*  Compression time: 00h 50m 38.96s (single threaded)
*  Compression time: 00h 05m 27.05s (multi threaded)
*  Best algorithm: Uniform sampling
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations, Per Segment Rotations & Translations

Note that there was a bug in fbx2acl truncating the last sample in 0.3.0 and older versions.

## Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0):

*  Raw size: 998.34 MB
*  Compressed size: 105.94 MB
*  Compression ratio: 9.42 : 1
*  Max error: 0.0748 centimeters (clip 144_31)
*  Compression time: 03h 37m 06.87s (single threaded)
*  Best algorithm: Uniform sampling
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

## Results from release [0.2.0](https://github.com/nfrechette/acl/releases/tag/v0.2.0):

*  Raw size: 998.34 MB
*  Compressed size: 239.51 MB
*  Compression ratio: 4.17 : 1
*  Max error: 0.0698 centimeters (clip 01_10)
*  Compression time: ~10 minutes
*  Best algorithm: Uniform sampling
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

Note that there was a bug in how the error was measured with release 0.2.0.
