# Pre-processing raw tracks

Once you have created a [raw track list](creating_a_raw_track_list.md) and an [allocator instance](implementing_an_allocator.md), you can optionally pre-process them.

Pre-processing gives explicit control over raw data modification. When compressing, ACL will never modify the raw data. This is ideal because it ensures that none of the inputs are modified and the whole process is deterministic and repeatable. However, sometimes this can lead to sub-optimal compression. In particular, under certain conditions, ACL may not be able to reach the raw data values when attempting to optimize things. As a result, more precision than otherwise necessary might be retained.

As such, it is recommended that the raw data be pre-processed before compression. This can be done at any point before compression (e.g. after FBX import).

Various pre-process actions can be performed in bulk or individually, see `pre_process_actions` for details.

Pre-processing actions can be *lossy* or *lossless*. See `pre_process_actions` and `pre_process_precision_policy` for details.

When pre-processing transform tracks, selecting the right [error metric](error_metrics.md) is important and you will want to carefully pick the one that best approximates how your game engine performs skinning. You will need to use the same error metric for pre-processing and compression.

```c++
#include <acl/compression/pre_process.h>

using namespace acl;

pre_process_settings_t settings;
settings.actions = pre_process_actions::all;
settings.precision_policy = pre_process_precision_policy::lossy;

// Required for transform track pre-processing
qvvf_transform_error_metric error_metric;
settings.error_metric = &error_metric;

pre_process_track_list(allocator, settings, transform_tracks);
```

See [pre_process.h](../includes/acl/compression/pre_process.h) for API and other usage details.
