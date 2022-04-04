# Handling looping playback

There are typically two ways to handle looping playback:

*  By `clamping` at the start/end of the clip meaning that we never interpolate between the first/last samples.
*  By `wrapping` at the end of the clip to interpolate with the first sample.

ACL supports both approaches and will pick the optimal one during compression (tunable in the compression settings). This is controlled through the usage of the `sample_looping_policy` enum found in its [header](../includes/acl/core/sample_looping_policy.h). It is best to let ACL handle this and to not override this value.

See also [this blog post](https://nfrechette.github.io/2022/04/03/anim_compression_looping/) for details.

## Clamp policy

Clamping means that looping clips behave just like non-looping ones. We never interpolate between the first/last samples which means that for clips to loop seamlessly, the first and last samples must match and be nearly identical.

The clamp policy can safely be used with non-looping clips as it is identical in value and behavior to the non-looping policy. The two are interchangeable.

## Wrap policy

Wrapping means that looping clips must interpolate between the first and last sample. The first sample will thus serve a dual purpose as true first and synthetic last sample. Because there is some amount of time that passes between the last and first sample, it means that with the wrap policy the clip duration changes as a result of the introduction of this synthetic last sample (the repeating first sample).

This is often used by host runtimes that hope to save a bit of extra memory by not including a largely identical sample but doing so adds complexity and [may not save as much as one might think](https://nfrechette.github.io/2020/08/09/animation_data_numbers/). It is also incorrect for any track that cannot safely interpolate between its last and first sample. For example, it is common for animations to store root motion as an absolute displacement from the start of the clip in the root joint. At a duration of 0 seconds, the root would be at 0 centimeter while at some duration N, the root would be at some distance from the start. Interpolating between the last sample and the first would mean doing so between a large value and a small one that are otherwise not neighboring. Because we artificially introduce a repeating first sample at the end to loop, that sample might have extra information that will be missing as is the case with root motion. This is most commonly mitigated by assuming that the velocity is constant between the second to last and last sample. Extracting the full displacement of the clip also becomes problematic as sampling the clip at its full duration will yield the first sample.

**THE WRAP POLICY SHOULD NOT BE USED WITH NON-LOOPING PLAYBACK.**

## Non-looping policy

The non-looping policy should be used with non-looping clips and it is identical in value and behavior to the clamp policy. The two are interchangeable.

## As compressed

This is the default value for anything exposed that works with compressed data. During compression, ACL will determine which mode is optimal and this value will use it. It is recommended to use this value and to let ACL handle things.

# I want to handle it on my own

Sometimes a host runtime will perform its own loop optimization. ACL also supports this. When creating the raw tracks for ACL, the looping policy can be specified through `set_looping_policy(policy)`. If `clamp` is used (the default), ACL will try to optimize with `wrap` if it can to do safely. If `wrap` is specified, ACL won't attempt anything and it will use that value during compression and later during decompression (through the `as_compressed` value).

If it is not possible or desirable for ACL to determine this during compression, the feature can be disabled in the compression settings. The host runtime can then override the behavior during decompression by specifying the looping policy on the decompression context through `set_looping_policy(policy)`.
