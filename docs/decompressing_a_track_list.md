# Decompressing a track list

Once you have a [compressed track list](compressing_raw_tracks.md), the first order of business to decompress it is to select a set of `decompression_settings`. The decompression settings `struct` is populated with `constexpr` functions that can be overridden to turn on or off the code generation per feature.

The next thing you need is a `decompression_context` instance. This will allow you to actually decompress tracks. You can also use it to seek arbitrarily in the list. For safety, the decompression context is templated with the decompression settings.

```c++
#include "acl/decompression/decompress.h"

using namespace acl;

decompression_context<default_decompression_settings> context;

context.initialize(*tracks);
context.seek(sample_time, sample_rounding_policy::none);

// create an instance of 'track_writer' so we can write the output somewhere
context.decompress_track(track_index, my_track_writer); // a single track
context.decompress_tracks(my_track_writer); // all tracks
```

As shown, a context must be initialized with a compressed track list instance. Some context objects such as the one used by uniform sampling can be re-used by any compressed track list and does not need to be re-created while others might require this. In order to detect when this might be required, the function `is_dirty(const compressed_tracks& tracks)` is provided. Some context objects cannot be created on the stack and must be dynamically allocated with an allocator instance. The functions `make_decompression_context(...)` are provided for this purpose.

You can seek anywhere in a track list but you will need to handle looping manually in your game engine. When seeking, you must also provide a `sample_rounding_policy` to dictate how the interpolation is to be performed. See [here](../includes/acl/core/interpolation_utils.h) for details.

Every decompression function supported by the context is prefixed with `decompress_*`. A `track_writer` is used for optimized output writing. You can implement your own and coerce to your own math types. The type is templated on the `decompress_*` functions in order to be easily inlined.

The API is the same for scalar and joint transform tracks. For optimal code generation, ensure the decompression settings used are tuned to the expected data. See the header where it is defined for more information.

## Floating point exceptions

For performance reasons, the decompression code assumes that the caller has already disabled all floating point exceptions. This avoids the need to save/restore them with every call. ACL provides helpers in [acl/core/floating_point_exceptions.h](..\includes\acl\core\floating_point_exceptions.h) to assist and optionally this behavior can be controlled by overriding `decompression_settings::disable_fp_exeptions()`.

## Backwards compatibility

By default, decompression will support every ACL version prior to and including ACL 2.0. If you wish to only support a single version and to strip the unnecessary code, you can specify your own decompression settings struct along with the desired `static constexpr compressed_tracks_version16 version_supported() { return compressed_tracks_version16::any; }` implementation.

*There is no backwards compatibility support for ACL 1.3 and earlier.*
