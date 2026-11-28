In order to save memory, quantization is used on sampled values to reduce their footprint. To do so, a number of formats are supported.

# Vector formats

### Vector3 Full

Simple full precision format. Each component `[X, Y, Z]` is stored with full floating point precision using `float32`.

### Vector3 Variable

The compression algorithm will search for the optimal bit rate among **19** possible values. An algorithm will select which bit rate to use for each track while keeping the memory footprint and error as low as possible. This format requires range reduction to be enabled.

# Rotation formats

Internally, rotation formats reuse the vector formats with some tweaks.

### Quat Full

A full precision quaternion format. Each component `[X, Y, Z, W]` is stored with full precision using `float32`.

## Dropping the quaternion W component

Every rotation can be represented by two distinct and opposite quaternions: a quaternion and its negated opposite. This is possible because [quaternions represent a hypersphere](https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation#The_hypersphere_of_rotations). As such, a component can be dropped and trivially reconstructed with a square root simply by ensuring that the component is positive and the quaternion normalized during compression.

### Quat Full

Same as Vector3 96 above to store `[X, Y, Z]`.

### Quat Variable

See [Vector3 Variable](rotation_and_vector_formats.md#vector3-variable)

## Dropping the largest quaternion component

The rotation is rebuilt from the three stored components: `c = sqrt(1.0 - a * a - b * b - d * d)`. The error of the rebuilt component grows as `1 / |c|`. When a rotation crosses `W = 0`, the error on `W` can be large: for example, a rotation error of 0.0007 radians when `W` is 0.000001. A child far from its parent multiplies this error by its distance. These formats drop another component to keep the dropped component far from 0. The dropped component is always positive. Its index uses 2 bits: `[W, X, Y, Z] = [0, 1, 2, 3]`. A track that drops `W` thus has the same data as with a format that always drops `W`.

These formats are the default since ACL 2.2: `get_default_compression_settings()` uses `quatf_drop_largest_variable`, and `default_transform_decompression_settings` supports only this rotation format.

### Quat Drop Largest Full

Each sample stores the three kept components with full precision using `float32`, and drops its largest component. Each component is in `[-1.0, 1.0]`, thus bit 30 of each `float32` is always 0. Bit 30 of the first two components holds the index of the dropped component.

### Quat Drop Largest Variable

Each segment of a track drops the component whose smallest absolute value is the largest in that segment. The three kept components use [Vector3 Variable](rotation_and_vector_formats.md#vector3-variable). The per sub-track metadata of each segment holds the index of the dropped component, it uses no additional memory. The clip range of each component bounds the components that it holds in each segment.

A constant rotation always drops its largest component, with either format. It holds the index in bit 30 of its first two components.
