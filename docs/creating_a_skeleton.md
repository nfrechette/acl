# Creating a rigid skeleton

After you have found an allocator instance to use, you will need to create a `RigidSkeleton` instance. A skeleton is tree made up of several bones. At the root lies a single bone with no parent. Each bone that follows can have any number of children but a single parent. You will only need a few things to get started:

*  A virtual vertex distance
*  A parent bone index
*  An optional bind transform
*  An optional string for the bone name

The virtual vertex distance is used by the [error metric function](error_metrics.md). It will measure the error of a vertex at that particular distance from the bone in object/mesh space. This distance should be large enough to contain the vast majority of the visible vertices skinned on that particular bone. The algorithm ensures that all vertices contained up to this distance will have an error lower than the supplied error threshold in the compression settings. It is generally sufficient for this value to be approximate and it is often safe to use the same value for every bone for humanoid characters. The value of **3cm** is good enough for cinematographic quality for most characters. While visible vertices around the torso will often be further away than this, finger tips and facial bones will be closer than that. Because the compressed track data is stored in relative space of the parent bone, any error will accumulate down the hierarchy. This means that in order to keep the leaf bones within that accuracy threshold, all the parent bones in that chain will require even higher accuracy.

The bind transform is present mainly for debugging purposes and it is otherwise not used by the library. The bone name is present as well exclusively for debugging purposes.

```c++
RigidBone bones[num_bones];
for (int bone_index = 0; bone_index < num_bones; ++bone_index)
{
    if (bone_index != 0)
        bones[bone_index].parent_index = bone_index - 1;	// Single bone chain

    bones[bone_index].vertex_distance = 3.0f;
}

RigidSkeleton(allocator, bones, num_bones);
```

*Note: The current API is subject to change for v2.0.*
