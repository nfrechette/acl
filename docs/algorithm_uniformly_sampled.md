# Algorithm: uniformly sampled

The uniformly sampled algorithm is by far the simplest. A clip is sampled at a fixed internal (e.g. 30 FPS) and each sample is retained. Constant tracks are compacted to a single sample and default tracks are dropped.

Compression is achieved by storing the samples on a reduced number of bits using the various [rotation and vector formats](rotation_and_vector_formats.md).

Decompression is very fast because the data is uniformly spaced. Each sample is sorted by time and by track to ensure that when we sample a specific point in time, all the relevant samples are contiguous in memory for optimal cache locality during decompression. This also means that the decompression performance is independent of the playback direction. It will be the same when playing forward, backward, and randomly.
