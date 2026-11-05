# Extended samples

ACL keeps the samples of a track at uniform intervals and does not store a time for each sample. By default, each sample interpolates linearly to the next sample. An extended track adds an interpolator to each sample. The interpolator of a sample controls the interval from that sample to the next sample.

Only scalar tracks support extended samples (`float1f`, `float2f`, `float3f`, `float4f`, and `vector4f`). Transform tracks (`vqvf`) do not support them, and `track_vqvf_ex` does not compile.

See also [issue #526](https://github.com/nfrechette/acl/issues/526) for the design notes.

## Sample interpolators

The `sample_interpolator_t` enum in [sample_interpolator.h](../includes/acl/core/sample_interpolator.h) lists the interpolators:

*  `constant`: between a sample and the next sample, the output is the value of the first sample. Use it for a step, for example a visibility flag or a material index.
*  `linear`: the output is `lerp(value0, value1, alpha)`. This is the behavior of a track that does not have extended samples.

ACL does not support curves (Bezier, Hermite, Catmull-Rom) at this time.

## Create an extended track

An extended track type has the `_ex` suffix: `track_float1f_ex` to `track_vector4f_ex`. An extended track list has the same suffix: `track_array_float1f_ex` to `track_array_vector4f_ex`. Each sample is a `track_extended_sample_t<T>` (see [track_extended_sample.h](../includes/acl/core/track_extended_sample.h)) that holds an `interpolator` and a `value`. The default interpolator is `linear`.

Each track in a track list must use the same track interpolator: all the tracks are linear, or all the tracks are extended. In an extended track list, each track can use a different interpolator for each sample.

```c++
#include <acl/compression/track_array.h>

using namespace acl;

track_desc_scalarf desc;
desc.output_index = 0;
desc.precision = 0.001F;

track_float1f_ex raw_track = track_float1f_ex::make_reserve(desc, allocator, num_samples, sample_rate);
raw_track[0].set_constant(1.0F);	// Between sample 0 and 1, the value is 1.0
raw_track[1].set_constant(5.0F);	// Between sample 1 and 2, the value is 5.0
raw_track[2].set_linear(2.0F);		// Between sample 2 and 3, the value goes from 2.0 to 8.0
raw_track[3].set_linear(8.0F);
// ...

track_array_float1f_ex raw_track_list(allocator, num_tracks);
raw_track_list[0] = std::move(raw_track);
```

The raw track list samples extended tracks with the same rules as the compressed tracks. Thus `track_array::sample_tracks(..)` and `track_array::sample_track(..)` give the reference values.

## Loops

The interpolator of the last sample has an effect only with the wrap policy:

*  With the clamp policy and the non-looping policy, no interval follows the last sample. Thus its interpolator has no effect.
*  With the wrap policy, the interpolator of the last sample controls the interval from the last sample to the first sample.

When compression optimizes a loop (`compression_settings::optimize_loops`), it removes the last sample because that sample matches the first sample. See [how to handle looping playback](handling_looping_playback.md).

## Rounding policies

The `sample_rounding_policy` applies first. Then a constant sample rounds the interpolation alpha down to 0.0. Between a constant sample and the next sample, the output is:

*  `none`: the value of the constant sample.
*  `floor`: the value of the constant sample.
*  `ceil`: the value of the next sample.
*  `nearest`: the value of the nearest sample.

Thus a constant sample changes only the output of the `none` policy. With the other policies, a constant sample and a linear sample give the same output. This also applies to each track when you use `per_track` (see [handling per track rounding](handling_per_track_rounding.md)).

## Pre-process

`pre_process_track_list(..)` supports extended tracks. It changes only the sample values. The interpolators do not change. See [pre-processing raw tracks](pre_processing_raw_tracks.md).

## Compression

Use `compress_track_list(..)` as you do with linear tracks (see [compressing raw tracks](compressing_raw_tracks.md)). Compression measures the error on each sample value. Between two samples, the output is one of the two values or a linear interpolation of them. Thus the track precision also applies between the samples.

The metadata of each track stores one of three modes:

*  `linear`: each sample is linear.
*  `constant`: each sample is constant.
*  `mixed`: the track uses the two interpolators.

Only a mixed track stores the interpolators, with 1 bit for each sample. A constant track (one value for all the samples) stores no interpolator. Thus an extended track where all the samples use the same interpolator has the same size as a linear track.

To select the mode, compression ignores each interpolator that has no effect on the output:

*  The interpolator of the last sample, when the track list does not wrap. No interval follows the last sample.
*  The interpolator of a sample that has the same value as the next sample (binary equality). Each interpolator gives that value.

For example, a track where each sample is constant except the last sample uses the `constant` mode, and it stores no interpolator. In a track with the `linear` mode, a constant sample with the same value as the next sample interpolates linearly. Then the output can be 1 ulp from the sample value. This occurs on x86 without FMA, because RTM uses the stable form of the linear interpolation. A linear track with two equal samples has the same behavior.

The compressed tracks use the version `v02_02_99_0` (see [compressed_tracks_version.h](../includes/acl/core/compressed_tracks_version.h)). ACL 2.1 and earlier cannot decompress this data.

## Decompression

The decompression setting `are_extended_samples_supported()` controls the code that reads the interpolators (see [decompression_settings.h](../includes/acl/decompression/decompression_settings.h)). It is `true` in all the default decompression settings. If your data has no extended samples, set it to `false` in your decompression settings. Then the optimizer removes that code.

When the setting is `false`, `decompression_context::initialize(..)` returns `false` for compressed tracks that have extended samples. If `skip_initialize_safety_checks()` is `true`, the context does not do this check. Do not decompress extended samples with such a context, because the output is not correct.

The `track_writer` receives only the output values. The decompression API does not change. See [decompressing tracks](decompressing_a_track_list.md).

## Conversion

`convert_track_list(..)` converts compressed tracks that have extended samples into an extended raw track list. With the `scalarf_full` scalar format, the conversion also keeps the exact values.

A mixed track keeps the interpolator of each sample. A linear track or a constant track gives the interpolator of its mode to each sample, thus an interpolator that has no effect can change. When each track uses the linear mode, the compressed tracks have no extended samples, and the conversion gives a linear raw track list.

## The ACL file format

In an `.acl.sjson` file, an extended track sets `interpolator = "extended"`. Each sample then holds an interpolator and a value:

```
tracks = [
	{
		name = "step"
		type = "float1f"
		interpolator = "extended"
		precision = 0.001
		output_index = 0
		data = [
			{
				interpolator = "constant"
				value = [ 1.0 ]
			}
			{
				interpolator = "linear"
				value = [ 2.0 ]
			}
		]
	}
]
```

A track without an `interpolator` key is a linear track. See [the ACL file format](the_acl_file_format.md).
