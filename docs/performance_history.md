# Performance history

## Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0):

*  Raw size: 998.34 MB
*  Compressed size: 105.94 MB
*  Compression ratio: 9.42 : 1
*  Max error: 0.0748 centimeters (clip 144_31)
*  Compression time: 03h 37m 06.87s (single threaded)
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

## Results from release [0.2.0](https://github.com/nfrechette/acl/releases/tag/v0.2.0):

*  Raw size: 998.34 MB
*  Compressed size: 239.51 MB
*  Compression ratio: 4.17 : 1
*  Max error: 0.0698 centimeters (clip 01_10)
*  Compression time: ~10 minutes
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

Note that there was a bug in how the error was measured with release 0.2.0.
