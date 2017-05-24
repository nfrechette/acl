// Raw clip filenames should have the form: *.acl.js
// Note: a clip raw file isn't a true valid javascript file. It uses simplified json syntax.

// Compressed clip filenames should have the form: *.acl.zip
// Note: a compressed clip is just a zipped raw clip.

// Each clip file contains the information of a single clip.

// Clip general information
// Must come first, before bones and tracks
clip =
{
	// Clip properties can come in any order

	// Clip name, handy for debugging. Optional, filename will be used if missing.
	name = "A clip"

	// Number of samples per track. All tracks must have the same number of samples.
	// Clip duration in seconds = (num_samples - 1) / sample_rate
	// Regardless of sample_rate, if we have a single sample, we have a 0.0 duration
	// and thus represent a static pose.
	num_samples = 73

	// Clip sample rate in samples per second. All tracks must have the same sample rate.
	sample_rate = 30

	// Error threshold in object space
	error_threshold = 0.01

	// Reference frame used by transforms: "object" space or parent bone "local" space
	reference_frame = "object"
}

// Reference skeleton, list of bones (any order)
// Must come before tracks
bones =
[
	{
		// Bone properties can come in any order

		// Bone name
		name = "root"

		// Parent bone. An empty string denotes the root bone, there can be only one root bone.
		// There must be a root bone.
		parent = ""

		// Virtual vertex distance used by hierarchical error function
		vertex_distance = 1.0

		// Bind pose transform information. All three are optional.
		bind_rotation = [ 0.0, 0.0, 0.0, 1.0 ]
		bind_translation = [ 0.0, 0.0, 0.0 ]
		bind_scale = [ 1.0, 1.0, 1.0 ]
	}
	{
		name = "bone1"
		parent = "root"
		vertex_distance = 1.0
	}
]

// Animation data, list of tracks (any order)
tracks =
[
	{
		// Track properties can come in any order

		// Bone name
		name = "root"

		// Rotation track, optional
		rotations =
		[
			[ 0.0, 0.0, 0.0, 1.0 ]
			[ 1.0, 0.0, 0.0, 0.0 ]
			// The number of samples here must match num_samples
		]

		// Translation track, optional
		translations =
		[
			[ 0.0, 0.0, 0.0 ]
			[ 1.0, 0.0, 0.0 ]
			// The number of samples here must match num_samples
		]

		// Scale track, optional
		scales =
		[
			[ 1.0, 1.0, 1.0 ]
			[ 1.0, 1.0, 1.0 ]
			// The number of samples here must match num_samples
		]
	}
]
