#pragma once

#include "acl/memory.h"
#include "acl/assert.h"
#include "acl/algorithm_globals.h"
#include "acl/compression/compressed_clip_impl.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/animation_clip.h"

//////////////////////////////////////////////////////////////////////////
// Full Precision Encoder
//
// The goal of the full precision format is to be used as a reference
// point for compression speed, compressed size, and decompression speed.
// This will not be a raw format in that we will at least drop constant
// or bind pose tracks. As such, it is near-raw but not quite.
//
// This is the highest precision encoder and the fastest to compress.
//
// Data format:
//    TODO: Detail the format
//////////////////////////////////////////////////////////////////////////

namespace acl
{
	namespace internal
	{
		inline void write(const Quat_64& rotation, float* ptr, uint32_t& offset)
		{
			// TODO: Drop quaternion W
			ptr[offset++] = float(quat_get_x(rotation));
			ptr[offset++] = float(quat_get_y(rotation));
			ptr[offset++] = float(quat_get_z(rotation));
			ptr[offset++] = float(quat_get_w(rotation));
		}

		inline void write(const Vector4_64& translation, float* ptr, uint32_t& offset)
		{
			ptr[offset++] = float(vector_get_x(translation));
			ptr[offset++] = float(vector_get_y(translation));
			ptr[offset++] = float(vector_get_z(translation));
		}
	}

	struct FullPrecisionHeader
	{
		uint32_t	num_samples;
		uint32_t	sample_rate;
		uint32_t	num_animated_rotation_tracks;
		uint32_t	num_animated_translation_tracks;
	};

	inline CompressedClip* full_precision_encoder(Allocator& allocator, const AnimationClip& clip, const RigidSkeleton& skeleton)
	{
		uint16_t num_bones = clip.get_num_bones();
		uint32_t num_samples = clip.get_num_samples();

		uint32_t buffer_size = 0;
		buffer_size += sizeof(CompressedClip);
		buffer_size += sizeof(FullPrecisionHeader);
		buffer_size += num_bones * num_samples;

		uint8_t* buffer = allocate_type_array<uint8_t>(allocator, buffer_size, 16);

		FullPrecisionHeader* header = reinterpret_cast<FullPrecisionHeader*>(buffer + sizeof(CompressedClip));
		header->num_samples = num_samples;
		header->sample_rate = clip.get_sample_rate();
		header->num_animated_rotation_tracks = num_bones;
		header->num_animated_translation_tracks = num_bones;

		float* track_data = reinterpret_cast<float*>(buffer + sizeof(CompressedClip) + sizeof(FullPrecisionHeader));
		uint32_t track_data_offset = 0;
		uint32_t track_data_max_offset = num_bones * 6;	// 3 floats per rotation, 3 per translation

		// Data is sorted first by time, second by bone.
		// This ensures that all bones are contiguous in memory when we sample a particular time.
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				const AnimatedBone& bone = clip.get_animated_bone(bone_index);

				Quat_64 rotation = bone.rotation_track.get_sample(sample_index);
				Vector4_64 translation = bone.translation_track.get_sample(sample_index);

				internal::write(rotation, track_data, track_data_offset);
				internal::write(translation, track_data, track_data_offset);

				ensure(track_data_offset <= track_data_max_offset);
			}
		}

		return make_compressed_clip(buffer, buffer_size, AlgorithmType::FullPrecision);
	}
}
