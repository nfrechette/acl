# Carnegie-Mellon University database performance history

To compile the statistics, the animation database from [Carnegie-Mellon University](http://mocap.cs.cmu.edu/) is used.

ACL supports various compression methods but only the overall best variant will be tracked here (see `get_default_compression_settings()` for details).

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).

## Results from release [1.0.0](https://github.com/nfrechette/acl/releases/tag/v1.0.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.02 MB**
*  Compression ratio: **21.33 : 1**
*  Max error: **0.0703** centimeters (clip *83_11*)
*  Compression time: **37m 13.14s** (single threaded)
*  Compression time: **10m 11.49s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

## Results from release [0.8.0](https://github.com/nfrechette/acl/releases/tag/v0.8.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.02 MB**
*  Compression ratio: **21.33 : 1**
*  Max error: **0.0703** centimeters (clip *122_11*)
*  Compression time: **36m 27.22s** (single threaded)
*  Compression time: **11m 03.60s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

## Results from release [0.7.0](https://github.com/nfrechette/acl/releases/tag/v0.7.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.04 MB**
*  Compression ratio: **21.32 : 1**
*  Max error: **0.0479** centimeters (clip *81_18*)
*  Compression time: **51m 24.01s** (single threaded)
*  Compression time: **13m 38.96s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

## Results from release [0.6.0](https://github.com/nfrechette/acl/releases/tag/v0.6.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.04 MB**
*  Compression ratio: **21.32 : 1**
*  Max error: **0.0479** centimeters (clip *81_18*)
*  Compression time: **51m 40.20s** (single threaded)
*  Compression time: **13m 04.58s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction


## Results from release [0.5.0](https://github.com/nfrechette/acl/releases/tag/v0.5.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.09 MB**
*  Compression ratio: **21.31 : 1**
*  Max error: **0.0587** centimeters (clip *144_32*)
*  Compression time: **01h 23m 51.48s** (single threaded)
*  Compression time: **00h 09m 21.94s** (multi threaded on 11 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: This release introduced 3D scale support and this is reflected in the raw size as well as the compression ratio.*

## Results from release [0.4.0](https://github.com/nfrechette/acl/releases/tag/v0.4.0):

*  Raw size: **1000.56 MB**
*  Compressed size: **82.25 MB**
*  Compression ratio: **12.16 : 1**
*  Max error: **0.0635** centimeters (clip *144_32*)
*  Compression time: **50m 38.96s** (single threaded)
*  Compression time: **05m 27.05s** (multi threaded on 11 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: There was a bug in fbx2acl truncating the last sample in 0.3.0 and older versions.*

## Results from release [0.3.0](https://github.com/nfrechette/acl/releases/tag/v0.3.0):

*  Raw size: **998.34 MB**
*  Compressed size: **105.94 MB**
*  Compression ratio: **9.42 : 1**
*  Max error: **0.0748** centimeters (clip *144_31*)
*  Compression time: **03h 37m 06.87s** (single threaded)
*  Best variant: Uniform sampling with variable bit rate and range reduction

## Results from release [0.2.0](https://github.com/nfrechette/acl/releases/tag/v0.2.0):

*  Raw size: **998.34 MB**
*  Compressed size: **239.51 MB**
*  Compression ratio: **4.17 : 1**
*  Max error: **0.0698** centimeters (clip *01_10*)
*  Compression time: **~10 minutes**
*  Best variant: Uniform sampling with variable bit rate and range reduction

*Note: There was a bug in how the error was measured with release 0.2.0.*
