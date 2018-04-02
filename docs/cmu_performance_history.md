# Carnegie-Mellon University database performance history

To compile the statistics, the animation database from [Carnegie-Mellon University](http://mocap.cs.cmu.edu/) is used.

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).

Statistics for ACL are being generated with the `acl_compressor` tool found [here](../tools/acl_compressor). It supports various compression method but only the overall best variant will be tracked here. Every clip uses an error threshold of **0.01cm (0.1mm)**.

## Results from release [0.7.0](https://github.com/nfrechette/acl/releases/tag/v0.7.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.04 MB**
*  Compression ratio: **21.32 : 1**
*  Max error: **0.0479** centimeters (clip *81_18*)
*  Compression time: **00h 51m 24.01s** (single threaded)
*  Compression time: **00h 13m 38.96s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

## Results from release [0.6.0](https://github.com/nfrechette/acl/releases/tag/v0.6.0):

*  Raw size: **1429.38 MB**
*  Compressed size: **67.04 MB**
*  Compression ratio: **21.32 : 1**
*  Max error: **0.0479** centimeters (clip *81_18*)
*  Compression time: **00h 51m 40.20s** (single threaded)
*  Compression time: **00h 13m 04.58s** (multi threaded on 4 cores)
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
*  Compression time: **00h 50m 38.96s** (single threaded)
*  Compression time: **00h 05m 27.05s** (multi threaded on 11 cores)
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
