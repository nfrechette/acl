////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"

#include "acl/algorithm/full_precision_encoder.h"
#include "acl/algorithm/full_precision_decoder.h"

struct OutputWriterImpl : public acl::OutputWriter
{
	void WriteBoneRotation(uint32_t bone_index, const acl::Quat_32& rotation)
	{
	}

	void WriteBoneTranslation(uint32_t bone_index, const acl::Vector4_32& translation)
	{
	}
};

int main()
{
	using namespace acl;

	Allocator allocator;

	// Initialize our skeleton
	RigidSkeleton skeleton(allocator, 2);
	RigidBone* bones = skeleton.get_bones();

	bones[0].name = "root";
	bones[0].parent_index = 0xFFFF;
	bones[0].bind_rotation = quat_set(0.0, 0.0, 0.0, 1.0);
	bones[0].bind_translation = vector_set(0.0, 0.0, 0.0, 0.0);
	bones[0].vertex_distance = 0.01;

	bones[1].name = "bone1";
	bones[1].parent_index = 0;
	bones[1].bind_rotation = quat_set(1.0, 0.0, 0.0, 0.0);
	bones[1].bind_translation = vector_set(1.0, 0.0, 0.0, 0.0);
	bones[1].vertex_distance = 0.01;

	// Populate our clip with our raw samples
	AnimationClip clip(allocator, skeleton, 2, 30);
	AnimatedBone* clip_bones = clip.get_bones();

	clip_bones[0].rotation_track.set_sample(0, quat_set(0.0, 0.0, 0.0, 1.0), 0.0);
	clip_bones[0].rotation_track.set_sample(1, quat_set(0.0, 0.0, 0.0, 1.0), 1.0);
	clip_bones[0].translation_track.set_sample(0, vector_set(0.0, 0.0, 0.0, 0.0), 0.0);
	clip_bones[0].translation_track.set_sample(1, vector_set(0.0, 0.0, 0.0, 0.0), 1.0);
	clip_bones[1].rotation_track.set_sample(0, quat_set(0.0, 0.0, 0.0, 1.0), 0.0);
	clip_bones[1].rotation_track.set_sample(1, quat_set(0.0, 0.0, 0.0, 1.0), 1.0);
	clip_bones[1].translation_track.set_sample(0, vector_set(0.0, 0.0, 0.0, 0.0), 0.0);
	clip_bones[1].translation_track.set_sample(1, vector_set(0.0, 0.0, 0.0, 0.0), 1.0);

	// Compress & Decompress
	{
		CompressedClip* compressed_clip = full_precision_encoder(allocator, clip, skeleton);

		OutputWriterImpl output_writer;
		full_precision_decoder(*compressed_clip, 0.0, output_writer);

		allocator.deallocate(compressed_clip);
	}

	return 0;
}
