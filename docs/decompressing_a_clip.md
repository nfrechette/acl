# Decompressing a clip

Once you have a [compressed clip](compressing_a_raw_clip.md), the first order of business to decompress it is to select a set of `DecompressionSettings`. The decompression settings `struct` is populated with `constexpr` functions that can be overridden to turn on or off the code generation per feature. For example, the default decompression settings disable all the code for packing formats that are not variable. For the uniformly sampled algorithm, see [here](../includes/acl/algorithm/uniformly_sampled/decoder.h) for details.

The next thing you need is a `DecompressionContext` instance. This will allow you to actually decompress poses, bones, and tracks. You can also use it to seek arbitrarily in the clip. For safety, the decompression context is templated with the decompression settings.

```c++
using namespace acl;
using namespace acl::uniformly_sampled;

DecompressionContext<DefaultDecompressionSettings> context;

context.initialize(*compressed_clip);
context.seek(sample_time, SampleRoundingPolicy::None);

context.decompress_bone(bone_index, &rotation, &translation, &scale);
```

As shown, a context must be initialized with a compressed clip instance. Some context objects such as the one used by uniform sampling can be re-used by any compressed clip and does not need to be re-created while others might require this. In order to detect when this might be required, the function `is_dirty(const CompressedClip& clip)` is provided. Some context objects cannot be created on the stack and must be dynamically allocated with an allocator instance. The functions `make_decompression_context(...)` are provided for this purpose.

You can seek anywhere in a clip but you will need to handle looping manually in your game engine. When seeking, you must also provide a `SampleRoundingPolicy` to dictate how the interpolation is to be performed. See [here](../includes/acl/core/interpolation_utils.h) for details.

Every decompression function supported by the context is prefixed with `decompress_`. Uniform sampling supports decompressing a whole pose with a custom `OutputWriter` for optimized pose writing. You can implement your own and coerce to your own math types. The type is templated on the `decompress_pose` function in order to be easily inlined.

```c++
Transform_32* transforms = new Transform_32[num_bones];
DefaultOutputWriter pose_writer(transforms, num_bones);
context.decompress_pose(pose_writer);
```
