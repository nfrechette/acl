In order to save memory, quantization is used on sampled values to reduce their footprint. To do so, a number of formats are supported.

# Vector formats

Note that in order to be able to use some of these formats, track data needs to have [range reduction](range_reduction.md) applied at least at the clip level.

### Vector3 96

Simple full precision format. Each component `[X, Y, Z]` is stored with full floating point precision using `float32`.

### Vector3 48

Each component `[X, Y, Z]` is stored with `uint16_t`. This format requires range reduction to be enabled.

### Vector3 32

The code supports storing each component `[X, Y, Z]` on an arbitrary number of bits but we hard coded `[11, 11, 10]` for our purposes. This format requires range reduction to be enabled.

### Vector3 Variable

The compression algorithm will search for the optimal bit rate among **19** possible values. An algorithm will select which bit rate to use for each track while keeping the memory footprint and error as low as possible. This format requires range reduction to be enabled.

# Rotation formats

Internally, rotation formats reuse the vector formats with some tweaks.

### Quat 128

A full precision quaternion format. Each component `[X, Y, Z, W]` is stored with full precision using `float32`.

## Dropping the quaternion W component

Every rotation can be represented by two distinct and opposite quaternions: a quaternion and its negated opposite. This is possible because [quaternions represent a hypersphere](https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation#The_hypersphere_of_rotations). As such, a component can be dropped and trivially reconstructed with a square root simply by ensuring that the component is positive and the quaternion normalized during compression.

### Quat 96

Same as Vector3 96 above to store `[X, Y, Z]`.

### Quat 48

Same as Vector3 48 above to store `[X, Y, Z]`.

### Quat 32

Same as Vector3 32 above to store `[X, Y, Z]`.

### Quat Variable

See [Vector3 Variable](rotation_and_vector_formats.md#vector3-variable)
