#pragma once

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

#include "acl/core/iallocator.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/error.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline bool is_rotation_track_constant(const RotationTrackStream& rotations, float threshold_angle)
	{
		// Calculating the average rotation and comparing every rotation in the track to it
		// to determine if we are within the threshold seems overkill. We can't use the min/max for the range
		// either because neither of those represents a valid rotation. Instead we grab
		// the first rotation, and compare everything else to it.
		auto sample_to_quat = [](const RotationTrackStream& track, uint32_t sample_index)
		{
			const Vector4_32 rotation = track.get_raw_sample<Vector4_32>(sample_index);

			switch (track.get_rotation_format())
			{
			case RotationFormat8::Quat_128:
				return vector_to_quat(rotation);
			case RotationFormat8::QuatDropW_96:
			case RotationFormat8::QuatDropW_48:
			case RotationFormat8::QuatDropW_32:
			case RotationFormat8::QuatDropW_Variable:
				return quat_from_positive_w(rotation);
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(track.get_rotation_format()));
				return vector_to_quat(rotation);
			}
		};

		const Quat_32 ref_rotation = sample_to_quat(rotations, 0);
		const Quat_32 inv_ref_rotation = quat_conjugate(ref_rotation);

		const uint32_t num_samples = rotations.get_num_samples();
		for (uint32_t sample_index = 1; sample_index < num_samples; ++sample_index)
		{
			const Quat_32 rotation = sample_to_quat(rotations, sample_index);
			const Quat_32 delta = quat_normalize(quat_mul(inv_ref_rotation, rotation));
			if (!quat_near_identity(delta, threshold_angle))
				return false;
		}

		return true;
	}

	inline void compact_constant_streams(IAllocator& allocator, ClipContext& clip_context, float rotation_threshold_angle, float translation_threshold, float scale_threshold)
	{
		ACL_ASSERT(clip_context.num_segments == 1, "ClipContext must contain a single segment!");
		SegmentContext& segment = clip_context.segments[0];

		const uint16_t num_bones = clip_context.num_bones;
		const Vector4_32 default_scale = get_default_scale(clip_context.additive_format);
		uint16_t num_default_bone_scales = 0;

		// When a stream is constant, we only keep the first sample
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			BoneStreams& bone_stream = segment.bone_streams[bone_index];
			BoneRanges& bone_range = clip_context.ranges[bone_index];

			// We expect all our samples to have the same width of sizeof(Vector4_32)
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %zu", bone_stream.rotations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %zu", bone_stream.translations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %zu", bone_stream.scales.get_sample_size(), sizeof(Vector4_32));

			if (is_rotation_track_constant(bone_stream.rotations, rotation_threshold_angle))
			{
				RotationTrackStream constant_stream(allocator, 1, bone_stream.rotations.get_sample_size(), bone_stream.rotations.get_sample_rate(), bone_stream.rotations.get_rotation_format());
				Vector4_32 rotation = bone_stream.rotations.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, rotation);

				bone_stream.rotations = std::move(constant_stream);
				bone_stream.is_rotation_constant = true;
				bone_stream.is_rotation_default = quat_near_identity(vector_to_quat(rotation), rotation_threshold_angle);

				bone_range.rotation = TrackStreamRange::from_min_extent(rotation, vector_zero_32());
			}

			if (bone_range.translation.is_constant(translation_threshold))
			{
				TranslationTrackStream constant_stream(allocator, 1, bone_stream.translations.get_sample_size(), bone_stream.translations.get_sample_rate(), bone_stream.translations.get_vector_format());
				Vector4_32 translation = bone_stream.translations.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, translation);

				bone_stream.translations = std::move(constant_stream);
				bone_stream.is_translation_constant = true;
				bone_stream.is_translation_default = vector_all_near_equal3(translation, vector_zero_32(), translation_threshold);

				bone_range.translation = TrackStreamRange::from_min_extent(translation, vector_zero_32());
			}

			if (bone_range.scale.is_constant(scale_threshold))
			{
				ScaleTrackStream constant_stream(allocator, 1, bone_stream.scales.get_sample_size(), bone_stream.scales.get_sample_rate(), bone_stream.scales.get_vector_format());
				Vector4_32 scale = bone_stream.scales.get_raw_sample<Vector4_32>(0);
				constant_stream.set_raw_sample(0, scale);

				bone_stream.scales = std::move(constant_stream);
				bone_stream.is_scale_constant = true;
				bone_stream.is_scale_default = vector_all_near_equal3(scale, default_scale, scale_threshold);

				bone_range.scale = TrackStreamRange::from_min_extent(scale, vector_zero_32());

				num_default_bone_scales += bone_stream.is_scale_default ? 1 : 0;
			}
		}

		clip_context.has_scale = num_default_bone_scales != num_bones;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
