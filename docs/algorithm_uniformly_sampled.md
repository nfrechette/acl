# Algorithm: uniformly sampled

The uniformly sampled algorithm is by far the simplest. A clip is sampled at a fixed internal (e.g. 30 FPS) and each sample is retained. Constant tracks are compacted to a single sample and default tracks are dropped.

The compression is achieved by storing the samples on a reduced number of bits using the various [rotation and vector formats](https://github.com/nfrechette/acl/blob/develop/docs/rotation_and_vector_formats.md).

Decompression is very fast because the data is uniformly spaced. Each sample is sorted by time and by track to ensure that when we sample a specific point in time, all the relevant samples are contiguous in memory.

Here is the code for the [encoder](https://github.com/nfrechette/acl/blob/develop/includes/acl/algorithm/uniformly_sampled/encoder.h) and [decoder](https://github.com/nfrechette/acl/blob/develop/includes/acl/algorithm/uniformly_sampled/decoder.h).
