# Handling per track rounding

When sampling raw or compressed tracks, a `sample_rounding_policy` is provided to control whether or not we should interpolate and what to return. For most use cases, the same rounding policy can be used for all tracks but ACL also supports the host runtime to specify per track which rounding policy should be used. This feature is disabled by default by the default `decompression_settings` because it isn't common and it adds a bit of overhead. To enable and use this feature, you need to:

*  Enable the feature in your `decompression_settings`, see `decompression_settings::is_per_track_rounding_supported()`.
*  Provide the `seek(..)` function with `sample_rounding_policy::per_track`.
*  Implement the rounding policy query function in your `track_writer`, see `track_writer::get_rounding_policy(..)`.

When the feature is enabled in the `decompression_settings`, ACL will calculate all possible samples it might need per track as opposed to just one when all tracks use the same rounding policy. This is cheaper than it sounds. By default, ACL always interpolates (using a stable interpolation function) even when `floor`, `ceil`, and `nearest` are used. When `per_track` is used, we retain both samples used to interpolate (for `floor` and `ceil`), we find the nearest and retain it, and we interpolate as well as we otherwise would. Although we do extra work, the added instructions can often execute in the shadow of more expensive ones that surround them which makes this very cheap.

See also [this blog post](TODO) for details.

## Database support

Using per track rounding may not behave as intended if a partially streamed database is used. For performance reasons, unlike the modes above, only a single value will be interpolated: the one at the specified sample time. This means that if we have 3 samples A, B, C and you sample between B and C with 'floor', if B has been moved to a database and is missing, B (interpolated) is not returned. Normally, A and C would be used to interpolate at the sample time specified as such we do not calculate where B lies. As a result of this, A would be returned unlike the behavior of 'floor' when used to sample all tracks. This is the behavior chosen by design. During decompression, samples are unpacked and interpolated before we know which track they belong to. As such, when the per track mode is used, we output 4 samples (instead of just the one we need), one for each possible mode above. One we know which sample we need (among the 4), we can simply index with the rounding mode to grab it. This is very fast and the cost is largely hidden. Supporting the same behavior as the rounding modes above when a partially streamed in database is used would require us to interpolate 3 samples instead of 1 which would be a lot more expensive for rotation sub-tracks. It would also add a lot of code complexity. For those reasons, the behavior differs.

A [future task](https://github.com/nfrechette/acl/issues/392) will allow tracks to be split into different layers where database settings can be independent. This will allow us to place special tracks into their own layer where key frames are not removed.

A [separate task](https://github.com/nfrechette/acl/issues/407) may allow us to specify per track whether the values can be interpolated or not. This would allow us to detect boundary key frames (where the value changes) and retain those we need to ensure that the end result is identical.
