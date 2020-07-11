# Creating a raw track list

Once you have an [allocator instance](implementing_an_allocator.md), the next step is to create a raw track list and populate it with the data we will want to compress. Doing so is fairly straight forward and you will only need a few things:

*  [An allocator instance](implementing_an_allocator.md)
*  The number of tracks (a track array cannot be resized)
*  The number of samples per track (each track has the same number of samples)
*  The rate at which samples are recorded (e.g. 30 == 30 FPS)
*  Access to the [Realtime Math](../external/README.md) headers since its types are used by the API

```c++
#include <acl/compression/track_array.h>

using namespace acl;

uint32_t num_tracks = 4;
uint32_t num_samples_per_track = 20;
float sample_rate = 30.0f;
track_array_float3f raw_track_list(allocator, num_tracks);
```

Once you have created an instance, simply populate the track data. Note that at the moment, tracks contained in the array are untyped but they must all have the same track type (e.g. `float2f`), the same number of samples per track, and the same sample rate.

Each track requires a track description. It contains a number of important properties:

*  `output_index`: after compression, this is the index of the track. This allows re-ordering for LOD processing and other similar use cases.
*  `parent_index`: for transform tracks, indicates the parent transform index it is relative to (in local space of).
*  `precision`: the precision we aim to attain when optimizing the bit rate. The resulting compression error is nearly guaranteed to be below this threshold.
*  `shell_distance`: for transform tracks, indicates the distance at which we measure the error, see [error metric function](error_metrics.md).

```c++
track_desc_scalarf desc0;
desc0.output_index = 0;
desc0.precision = 0.001F;

track_desc_transformf desc1;
desc1.output_index = 0;
desc1.parent_index = 0;
desc1.precision = 0.01F;
desc1.shell_distance = 3.0F;
```

Tracks can be created in one of four ways:

*  `make_copy(..)`: creates a new track and copies the data internally. The new instance owns the memory and the original data is no longer required.
*  `make_reserve(..)`: creates a new track and allocates the required memory without initializing it. It owns the new allocated memory.
*  `make_owner(..)`: creates a new track and takes ownership of the provided source pointer. No copy takes place.
*  `make_ref(..)`: creates a new track and references the provided source data. No copy takes place and the new instance does not own the referenced memory.

`make_owner(..)` and `make_ref(..)` are handy when the track data is already allocated somewhere and doesn't need to be copied explicitly. Functions that take source data as input also support a custom `stride` and make no assumption about the data layout for utmost flexibility.

```c++
track_float3f raw_track0 = track_float3f::make_reserve(desc0, allocator, num_samples, sample_rate);
raw_track0[0] = rtm::float3f{ 1.0F, 3123.0F, 315.13F };
raw_track0[1] = rtm::float3f{ 2.333F, 321.13F, 31.66F };
raw_track0[2] = rtm::float3f{ 3.123F, 81.0F, 913.13F };
raw_track0[3] = rtm::float3f{ 4.5F, 91.13F, 41.135F };
// ...
raw_track_list[0] = std::move(raw_track0);
```

Once your raw track list has been populated with data, it is ready for [compression](compressing_raw_tracks.md). The data contained within the `track_array` will be read-only.

## Additive animation clips

If the clip you are compressing is an additive clip, you will also need to create an instance for the base clip. Once you have both clip instances, you can compress them together with `compress_track_list(..)`. This will allow you to specify the clip instance that represents the base clip as well as the [format](additive_clips.md) used by the additive clip.

The library assumes that the raw clip data has already been transformed to be in additive or relative space.

## Re-ordering or stripping bones

Sometimes it is desirable to re-order the bones being outputted or strip them altogether. This could be to facilitate LOD support or various forms of skeleton changes without needing to re-import the clips. This is easily achieved by setting the desired `output_index` on each `track_desc_transformf` contained in a `track_qvvf`. The default value is the track index. You can use `k_invalid_track_index` to strip the track from the final compressed output.

*Note that each `output_index` needs to be unique and there can be no gaps. If **20** bones are outputted, the indices must run from **[0 .. 20)**.*

## Compressing morph target blend weights

Curves that drive a morph target (aka blend shape) are not ordinary and require special consideration. They ultimately drive an object space mesh deformation and as such we can leverage that information to reduce the memory footprint and simplify the task of choosing a suitable `precision` value.

If you have access to the morph target deformation information, you can calculate the largest vertex displacement for a given curve. Once you have that value, use a morph target deformation precision value such as **0.1 mm** and use the maximum vertex displacement to calculate the desired blend weight `precision` value like this:

`blend weight precision = vertex precision / vertex displacement delta`

See [here](https://nfrechette.github.io/2020/05/04/morph_target_compresion/) for more details.
