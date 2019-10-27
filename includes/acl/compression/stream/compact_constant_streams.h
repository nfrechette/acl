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
#include "acl/compression/impl/track_database.h"

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
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Vector4_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.translations.get_sample_size() == sizeof(Vector4_32), "Unexpected translation sample size. %u != %u", bone_stream.translations.get_sample_size(), sizeof(Vector4_32));
			ACL_ASSERT(bone_stream.scales.get_sample_size() == sizeof(Vector4_32), "Unexpected scale sample size. %u != %u", bone_stream.scales.get_sample_size(), sizeof(Vector4_32));

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

	namespace acl_impl
	{
		// Multiplication order is as follow: local_to_world = quat_mul(local_to_object, object_to_world)
		inline void ACL_SIMD_CALL quat_mul_soa(Vector4_32Arg0 lhs_x, Vector4_32Arg1 lhs_y, Vector4_32Arg2 lhs_z, Vector4_32Arg3 lhs_w,
			Vector4_32Arg4 rhs_x, Vector4_32Arg5 rhs_y, Vector4_32ArgN rhs_z, Vector4_32ArgN rhs_w,
			Vector4_32& out_x, Vector4_32& out_y, Vector4_32& out_z, Vector4_32& out_w)
		{
			out_x = vector_neg_mul_sub(rhs_z, lhs_y, vector_mul_add(rhs_y, lhs_z, vector_mul_add(rhs_x, lhs_w, vector_mul(rhs_w, lhs_x))));
			out_y = vector_mul_add(rhs_z, lhs_x, vector_mul_add(rhs_y, lhs_w, vector_neg_mul_sub(rhs_x, lhs_z, vector_mul(rhs_w, lhs_y))));
			out_z = vector_mul_add(rhs_z, lhs_w, vector_neg_mul_sub(rhs_y, lhs_x, vector_mul_add(rhs_x, lhs_y, vector_mul(rhs_w, lhs_z))));
			out_w = vector_neg_mul_sub(rhs_z, lhs_z, vector_neg_mul_sub(rhs_y, lhs_y, vector_neg_mul_sub(rhs_x, lhs_x, vector_mul(rhs_w, lhs_w))));
		}

		inline Vector4_32 ACL_SIMD_CALL vector_sqrt_reciprocal(Vector4_32Arg0 input)
		{
#if defined(ACL_SSE2_INTRINSICS)
			// Perform two passes of Newton-Raphson iteration on the hardware estimate
			__m128 half = _mm_set_ps1(0.5f);
			__m128 input_half = _mm_mul_ps(input, half);
			__m128 x0 = _mm_rsqrt_ps(input);

			// First iteration
			__m128 x1 = _mm_mul_ps(x0, x0);
			x1 = _mm_sub_ps(half, _mm_mul_ps(input_half, x1));
			x1 = _mm_add_ps(_mm_mul_ps(x0, x1), x0);

			// Second iteration
			__m128 x2 = _mm_mul_ps(x1, x1);
			x2 = _mm_sub_ps(half, _mm_mul_ps(input_half, x2));
			x2 = _mm_add_ps(_mm_mul_ps(x1, x2), x1);

			return x2;
#else
			// TODO
			return 1.0f / sqrt(input);
#endif
		}

		inline Vector4_32 ACL_SIMD_CALL vector_length_squared_soa(Vector4_32Arg0 input_x, Vector4_32Arg1 input_y, Vector4_32Arg2 input_z, Vector4_32Arg3 input_w)
		{
			return vector_mul_add(input_w, input_w, vector_mul_add(input_z, input_z, vector_mul_add(input_y, input_y, vector_mul(input_x, input_x))));
		}

		inline Vector4_32 ACL_SIMD_CALL vector_length_reciprocal_soa(Vector4_32Arg0 input_x, Vector4_32Arg1 input_y, Vector4_32Arg2 input_z, Vector4_32Arg3 input_w)
		{
			const Vector4_32 length_sq = vector_length_squared_soa(input_x, input_y, input_z, input_w);
			return vector_sqrt_reciprocal(length_sq);
		}

		inline void ACL_SIMD_CALL quat_normalize_soa(Vector4_32Arg0 input_x, Vector4_32Arg1 input_y, Vector4_32Arg2 input_z, Vector4_32Arg3 input_w,
			Vector4_32& out_x, Vector4_32& out_y, Vector4_32& out_z, Vector4_32& out_w)
		{
			const Vector4_32 inv_length = vector_length_reciprocal_soa(input_x, input_y, input_z, input_w);
			out_x = vector_mul(input_x, inv_length);
			out_y = vector_mul(input_y, inv_length);
			out_z = vector_mul(input_z, inv_length);
			out_w = vector_mul(input_w, inv_length);
		}

		inline float scalar_acos(float value)
		{
			return std::acos(value);
		}

		inline Vector4_32 ACL_SIMD_CALL vector_acos(Vector4_32Arg0 input)
		{
			// TODO optimize this
			return vector_set(scalar_acos(vector_get_x(input)), scalar_acos(vector_get_y(input)), scalar_acos(vector_get_z(input)), scalar_acos(vector_get_w(input)));
		}

		inline bool ACL_SIMD_CALL quat_near_identity_soa(Vector4_32Arg0 input_x, Vector4_32Arg1 input_y, Vector4_32Arg2 input_z, Vector4_32Arg3 input_w, float threshold_angle = 0.00284714461f)
		{
			(void)input_x;
			(void)input_y;
			(void)input_z;
			const Vector4_32 positive_w_angle = vector_mul(vector_acos(vector_abs(input_w)), 2.0f);
			return vector_all_less_than(positive_w_angle, vector_set(threshold_angle));
		}

		inline bool is_rotation_track_constant(track_database& database, segment_context* segments, uint32_t num_segments, uint32_t transform_index, float threshold_angle)
		{
			const Quat_32 ref_rotation = vector_to_quat(database.get_rotation(segments[0], transform_index, 0));
			const Vector4_32 inv_ref_rotation = quat_to_vector(quat_conjugate(ref_rotation));
			const Vector4_32 inv_ref_rotation_x = vector_mix_xxxx(inv_ref_rotation);
			const Vector4_32 inv_ref_rotation_y = vector_mix_yyyy(inv_ref_rotation);
			const Vector4_32 inv_ref_rotation_z = vector_mix_zzzz(inv_ref_rotation);
			const Vector4_32 inv_ref_rotation_w = vector_mix_wwww(inv_ref_rotation);

			// TODO: Trivial AVX or ISPC conversion
			for (uint32_t segment_index = 0; segment_index < num_segments; ++segment_index)
			{
				const segment_context& segment = segments[segment_index];

				Vector4_32* rotations_x;
				Vector4_32* rotations_y;
				Vector4_32* rotations_z;
				Vector4_32* rotations_w;
				database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

				const uint32_t num_soa_entries = segment.num_soa_entries;
				for (uint32_t entry_index = 0; entry_index < num_soa_entries; ++entry_index)
				{
					Vector4_32 delta_x;
					Vector4_32 delta_y;
					Vector4_32 delta_z;
					Vector4_32 delta_w;
					quat_mul_soa(inv_ref_rotation_x, inv_ref_rotation_y, inv_ref_rotation_z, inv_ref_rotation_w,
						rotations_x[entry_index], rotations_y[entry_index], rotations_z[entry_index], rotations_w[entry_index],
						delta_x, delta_y, delta_z, delta_w);
					quat_normalize_soa(delta_x, delta_y, delta_z, delta_w, delta_x, delta_y, delta_z, delta_w);

					if (!quat_near_identity_soa(delta_x, delta_y, delta_z, delta_w, threshold_angle))
						return false;
				}
			}

			return true;
		}

		inline bool ACL_SIMD_CALL is_range_extent_zero(Vector4_32Arg0 extent, float threshold) { return vector_all_less_than(vector_abs(extent), vector_set(threshold)); }

		inline void detect_constant_tracks(track_database& database, segment_context* segments, uint32_t num_segments, float rotation_threshold_angle, float translation_threshold, float scale_threshold)
		{
			const Vector4_32 zero = vector_zero_32();
			const Vector4_32 default_scale = database.get_default_scale();
			const bool has_scale = database.has_scale();

			const uint32_t num_transforms = database.get_num_transforms();
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				qvvf_ranges& transform_range = database.get_range(transform_index);

				if (is_rotation_track_constant(database, segments, num_segments, transform_index, rotation_threshold_angle))
				{
					const Vector4_32 rotation = database.get_rotation(segments[0], transform_index, 0);

					transform_range.is_rotation_constant = true;
					transform_range.is_rotation_default = quat_near_identity(vector_to_quat(rotation), rotation_threshold_angle);

					transform_range.rotation_max = transform_range.rotation_min;
					transform_range.rotation_extent = zero;
				}
				else
				{
					transform_range.is_rotation_constant = false;
					transform_range.is_rotation_default = false;
				}

				if (is_range_extent_zero(transform_range.translation_extent, translation_threshold))
				{
					const Vector4_32 translation = database.get_translation(segments[0], transform_index, 0);

					transform_range.is_translation_constant = true;
					transform_range.is_translation_default = vector_all_near_equal3(translation, vector_zero_32(), translation_threshold);

					transform_range.translation_max = transform_range.translation_min;
					transform_range.translation_extent = zero;
				}
				else
				{
					transform_range.is_translation_constant = false;
					transform_range.is_translation_default = false;
				}

				if (!has_scale)
				{
					transform_range.is_scale_constant = true;
					transform_range.is_scale_default = true;
				}
				else if (is_range_extent_zero(transform_range.scale_extent, scale_threshold))
				{
					const Vector4_32 scale = database.get_scale(segments[0], transform_index, 0);

					transform_range.is_scale_constant = true;
					transform_range.is_scale_default = vector_all_near_equal3(scale, default_scale, scale_threshold);

					transform_range.scale_max = transform_range.scale_min;
					transform_range.scale_extent = zero;
				}
				else
				{
					transform_range.is_scale_constant = false;
					transform_range.is_scale_default = false;
				}
			}
		}

		inline void detect_segment_constant_tracks(const track_database& database, segment_context& segment)
		{
			const uint32_t num_transforms = database.get_num_transforms();
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const qvvf_ranges& transform_clip_range = database.get_range(transform_index);
				qvvf_ranges& transform_segment_range = segment.ranges[transform_index];

				transform_segment_range.is_rotation_constant = transform_clip_range.is_rotation_constant;
				transform_segment_range.is_rotation_default = transform_clip_range.is_rotation_default;

				transform_segment_range.is_translation_constant = transform_clip_range.is_translation_constant;
				transform_segment_range.is_translation_default = transform_clip_range.is_translation_default;

				transform_segment_range.is_scale_constant = transform_clip_range.is_scale_constant;
				transform_segment_range.is_scale_default = transform_clip_range.is_scale_default;
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
