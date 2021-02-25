# Migrating from older releases

## Migrating to ACL 2.0 from 1.3

ACL 2.0 brought along with it a lot of cleanup of high level APIs, file organization, and migrated all math usage to [Realtime Math](https://github.com/nfrechette/rtm).

### Coding standard

Most code in ACL 1.3 used `CamelCase` but everything has now been migrated to `snake_case`. This is to ensure consistency with the rest of the C++ standard library and other prominant libraries. Enums, classes, and functions have been renamed to the new standard but have otherwise mostly kept the same names.

### Code organization

All implementation details have now been moved into the `acl::acl_impl` namespace to avoid polluting the main namespace. Most of that code now also lives under `impl` directories (e.g. `acl/compression/impl/clip_context.h`).

Furthermore, the public headers try and stay as lean as possible with only function and type signatures where possible. Implementation details have been moved to headers with a `.impl.h` suffix (e.g. `acl/compression/impl/compress.impl.h`).

Going forward, only code in public headers will be subject to deprecation rules and backward compatibility support. Implementation details are free to move and change in any release.

### Realtime Math

RTM started off as a fork of ACL that contained only its math and it is maintained separately. Types were renamed but most functions have kept the same name.

* acl::Vector4_32 -> rtm::vector4f
* acl::Quat_32 -> rtm::quatf
* acl::Transform_32 -> rtm::qvvf

See the ACL source code for example usage but migrating should be mostly painless.

ACL now relies on RTM to detect the compiler used and the architecture being compiled to. It also relies on RTM for a few other math and code generation related macros. All macros start with a `RTM_` prefix much like previously `ACL_`. Macros mostly kept the same name with the new prefix.

### New compression and decompression APIs

ACL 1.3 introduced a new compression/decompression APIs for scalar tracks that used `acl::track_array` and `acl::track`. Both of these have typed aliases as well for the supported math types (e.g. `acl::track_qvvf`). ACL 2.0 now exclusively uses this new API even for hierarchical joint based animations. See [here](compressing_raw_tracks.md) for example compression usage.

The same feature set is still supported but some parameters moved out of the `acl::CompressionSettings` (now called `acl::compression_settings`) and have been made tunable per individual track.

A single header now contains all compression related entry points under [acl/compression/compress.h](../includes/acl/compression/compress.h).

The decompression API is very similar to the old one and migrating should be fairly straight forward. It relies on as much statically known information as possible via `acl::decompression_settings` to strip out as much unused code as possible. Backwards compatibility with older file formats is also controlled that way. A few other features are controlled that way as well. See [here](decompressing_a_track_list.md) for example decompression usage.

A single header now contains all decompression related entry points under [acl/decompression/decompress.h](../includes/acl/decompression/decompress.h).

### Error metrics

While the logic of the error metric hasn't itself changed, the API has been rewritten in order to allow better code generation and faster execution. The old `ISkeletalErrorMetric` interface has been renamed to `itransform_error_metric` and it can be found under [acl/compression/transform_error_metrics.h](../includes/acl/compression/transform_error_metrics.h).

Error metrics now allow operating and caching other transform types (e.g. `rtm::matrix3x4f`). See the above header for example usage.
