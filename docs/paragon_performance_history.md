# Paragon database performance history

To compile the statistics, a large number of animations from [Paragon](https://www.epicgames.com/paragon) are used.
In October 2017 the animations were extracted and converted to the [ACL file format](the_acl_file_format.md) losslessly. The data is sadly **NOT** available upon request.
Epic has permitted [Nicholas Frechette](https://github.com/nfrechette) to use them for research purposes only under a non-disclosure agreement.

*  Number of clips: **6558**
*  Total duration: **07h 00m 45.27s**
*  Raw size: **4276.11 MB** (10x float32 * num bones * num samples)

The error is measured **3cm** away from each bone to simulate the visual mesh skinning process as described [here](error_metrics.md).

Statistics for ACL are being generated with the `acl_compressor` tool found [here](../tools/acl_compressor). It supports various compression method but only the overall best variant will be tracked here. Every clip uses an error threshold of **0.01cm (0.1mm)**.

## Results from release [0.8.0](https://github.com/nfrechette/acl/releases/tag/v0.8.0):

*  Compressed size: **206.09 MB**
*  Compression ratio: **20.75 : 1**
*  Max error: **3.8615** centimeters
*  Compression time: **07h 30m 46.44s** (single threaded)
*  Compression time: **02h 03m 17.12s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: The error is unusually high for **3** exotic clips*

## Results from release [0.7.0](https://github.com/nfrechette/acl/releases/tag/v0.7.0):

*  Compressed size: **205.58 MB**
*  Compression ratio: **20.80 : 1**
*  Max error: **3.8615** centimeters
*  Compression time: **09h 43m 41.08s** (single threaded)
*  Compression time: **02h 37m 23.60s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: The error is unusually high for **3** exotic clips*

## Results from release [0.6.0](https://github.com/nfrechette/acl/releases/tag/v0.6.0):

*  Compressed size: **205.58 MB**
*  Compression ratio: **20.80 : 1**
*  Max error: **3.8615** centimeters
*  Compression time: **09h 38m 28.25s** (single threaded)
*  Compression time: **02h 36m 46.82s** (multi threaded on 4 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: The error is unusually high for **3** exotic clips*

## Results from release [0.5.0](https://github.com/nfrechette/acl/releases/tag/v0.5.0):

*  Compressed size: **205.69 MB**
*  Compression ratio: **20.79 : 1**
*  Max error: **9.7920** centimeters
*  Compression time: **19h 04m 25.11s** (single threaded)
*  Compression time: **01h 53m 42.84s** (multi threaded on 11 cores)
*  Best variant: Segmented uniform sampling with variable bit rate and range reduction

*Note: The error is unusually high for **3** exotic clips and the single threaded compression time was the total thread time which is much higher due to hyper threading*
