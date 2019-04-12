# Creating a raw clip

Once you have [created a rigid skeleton](creating_a_skeleton.md), the next step is to create a raw clip and populate it with the data we will want to compress. Doing so is fairly straight forward and you will only need a few things:

*  [An allocator instance](implementing_an_allocator.md)
*  [A rigid skeleton](creating_a_skeleton.md)
*  The number of samples per track (each track has the same number of samples)
*  The rate at which samples are recorded (e.g. 30 == 30 FPS)
*  An optional string for the clip name

```c++
uint32_t num_samples_per_track = 20;
float sample_rate = 30.0f;
String name(allocator, "Run Cycle");
AnimationClip clip(allocator, skeleton, num_samples_per_track, sample_rate, name);
```

Once you have created an instance, simply populate the track data. Note that for now, even if you have no scale data, you have to populate the scale track with the default scale value that your engine expects.

```c++
AnimatedBone& bone = clip.get_animated_bone(bone_index);
for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
{
    bone.rotation_track.set_sample(sample_index, quat_identity_64());
    bone.translation_track.set_sample(sample_index, vector_zero_64());
    bone.scale_track.set_sample(sample_index, vector_set(1.0));
}
```

Once your raw clip has been populated with data, it is ready for [compression](compressing_a_raw_clip.md). The data contained within the `AnimationClip` will be read-only.

*Note: The current API is subject to change for **v2.0**. As it stands right now, it forces you to duplicate the memory of the raw clip by copying everything within the library instance. A future API will allow the game engine to own the raw clip memory and have ACL simply reference it directly, avoiding the overhead of a copy.*

## Additive animation clips

If the clip you are compressing is an additive clip, you will also need to create an instance for the base clip. Once you have both clip instances, you link them together by calling `set_additive_base(..)` on the additive clip. This will allow you to specify the clip instance that represents the base clip as well as the [format](additive_clips.md) used by the additive clip.

The library assumes that the raw clip data has already been transformed to be in additive or relative space.

## Re-ordering or stripping bones

Sometimes it is desirable to re-order the bones being outputted or strip them altogether. This could be to facilitate LOD support or various forms of skeleton changes without needing to re-import the clips. This is easily achieved by setting the desired `output_index` on each `AnimatedBone` contained in an `AnimationClip`. The default value is the bone index. You can use `k_invalid_bone_index` to strip the bone from the final compressed output.

*Note that each `output_index` needs to be unique and there can be no gaps. If **20** bones are outputted, the indices must run from **[0 .. 20)**.*