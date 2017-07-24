# Performance history

## Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0):

*  Raw size: 998.34 MB
*  Compressed size: 73.60 MB
*  Max error: 0.3448 centimeters (clip 132_44)
*  Compression time: 04h 56m 44.67s (single threaded)
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

## Results from release [0.2.0](https://github.com/nfrechette/acl/releases/tag/v0.2.0):

*  Raw size: 998.34 MB
*  Compressed size: 239.51 MB
*  Max error: 0.0698 centimeters (clip 01_10)
*  Compression time: ~10 minutes
*  Best rotation format: Quat Drop W Variable
*  Best translation format: Vector3 Variable
*  Best range reduction format: Per Clip Rotations & Translations

Note that there was a bug in how the error was measured with release 0.2.0.
