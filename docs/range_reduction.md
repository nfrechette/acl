# Range reduction

Range reduction is performed by calculating the range of possible values in a list and normalizing the values within that range. Reconstructing the original value is trivial:

`origial_value = (normalized_value * range_extent) + range_min`

This is an important optimization to keep the memory footprint as low as possible because it typically allows us to increase the precision retained on the normalized values.

This feature can be enabled at the clip level where entire tracks are quantized over their full range as well as at the segment level where they are normalized over the segment only. Enabling the feature at the segment level requires it to also be enabled at the clip level because we store the range information in quantized form. This is entirely controlled by the underlying compression algorithm.

Additional reading:

*  [How it works](https://nfrechette.github.io/2016/11/09/anim_compression_range_reduction/)
*  [How much it helps](https://nfrechette.github.io/2017/09/10/acl_v0.4.0/)
