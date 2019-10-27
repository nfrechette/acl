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
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_packing.h"
#include "acl/math/transform_32.h"
#include "acl/compression/impl/track_database.h"
#include "acl/compression/stream/track_stream.h"
#include "acl/compression/stream/normalize_streams.h"
#include "acl/compression/stream/convert_rotation_streams.h"
#include "acl/compression/stream/segment_context.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace impl
	{
		inline Vector4_32 ACL_SIMD_CALL load_rotation_sample(const uint8_t* ptr, RotationFormat8 format, uint8_t bit_rate, bool is_normalized)
		{
			switch (format)
			{
			case RotationFormat8::Quat_128:
				return unpack_vector4_128(ptr);
			case RotationFormat8::QuatDropW_96:
				return unpack_vector3_96_unsafe(ptr);
			case RotationFormat8::QuatDropW_48:
				return is_normalized ? unpack_vector3_u48_unsafe(ptr) : unpack_vector3_s48_unsafe(ptr);
			case RotationFormat8::QuatDropW_32:
				return unpack_vector3_32(11, 11, 10, is_normalized, ptr);
			case RotationFormat8::QuatDropW_Variable:
			{
				if (is_constant_bit_rate(bit_rate))
				{
					ACL_ASSERT(is_normalized, "Cannot drop a constant track if it isn't normalized");
					return unpack_vector3_u48_unsafe(ptr);
				}
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					if (is_normalized)
						return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
					else
						return unpack_vector3_sXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			}
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return vector_zero_32();
			}
		}

		inline Vector4_32 ACL_SIMD_CALL load_vector_sample(const uint8_t* ptr, VectorFormat8 format, uint8_t bit_rate)
		{
			switch (format)
			{
			case VectorFormat8::Vector3_96:
				return unpack_vector3_96_unsafe(ptr);
			case VectorFormat8::Vector3_48:
				return unpack_vector3_u48_unsafe(ptr);
			case VectorFormat8::Vector3_32:
				return unpack_vector3_32(11, 11, 10, true, ptr);
			case VectorFormat8::Vector3_Variable:
				ACL_ASSERT(bit_rate != k_invalid_bit_rate, "Invalid bit rate!");
				if (is_constant_bit_rate(bit_rate))
					return unpack_vector3_u48_unsafe(ptr);
				else if (is_raw_bit_rate(bit_rate))
					return unpack_vector3_96_unsafe(ptr);
				else
				{
					const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
					return unpack_vector3_uXX_unsafe(num_bits_at_bit_rate, ptr, 0);
				}
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(format));
				return vector_zero_32();
			}
		}

		inline Quat_32 ACL_SIMD_CALL rotation_to_quat_32(Vector4_32Arg0 rotation, RotationFormat8 format)
		{
			switch (format)
			{
			case RotationFormat8::Quat_128:
				return vector_to_quat(rotation);
			case RotationFormat8::QuatDropW_96:
			case RotationFormat8::QuatDropW_48:
			case RotationFormat8::QuatDropW_32:
			case RotationFormat8::QuatDropW_Variable:
				return quat_from_positive_w(rotation);
			default:
				ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(format));
				return quat_identity_32();
			}
		}
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized;

		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();
		const uint8_t bit_rate = bone_steams.rotations.get_bit_rate();

		if (format == RotationFormat8::QuatDropW_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_rotation = impl::load_rotation_sample(quantized_ptr, format, bit_rate, are_rotations_normalized);

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	namespace acl_impl
	{
		inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const track_database& database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index)
		{
			const RotationFormat8 format = database.get_rotation_format();
			ACL_ASSERT(format == RotationFormat8::Quat_128 || format == RotationFormat8::QuatDropW_96, "Unexpected rotation format");

			Vector4_32 packed_rotation = database.get_rotation(segment, transform_index, sample_index);

			const qvvf_ranges& clip_transform_range = database.get_range(transform_index);
			if (clip_transform_range.are_rotations_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_rotations_normalized)
					packed_rotation = vector_mul_add(packed_rotation, segment_transform_range.rotation_extent, segment_transform_range.rotation_min);

				packed_rotation = vector_mul_add(packed_rotation, clip_transform_range.rotation_extent, clip_transform_range.rotation_min);
			}

			return impl::rotation_to_quat_32(packed_rotation, format);
		}
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized;
		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();

		Vector4_32 rotation;
		if (is_constant_bit_rate(bit_rate))
		{
			const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset);
			rotation = impl::load_rotation_sample(quantized_ptr, RotationFormat8::Quat_128, k_invalid_bit_rate, are_rotations_normalized);
			rotation = convert_rotation(rotation, RotationFormat8::Quat_128, format);
		}
		else if (is_raw_bit_rate(bit_rate))
		{
			const uint8_t* quantized_ptr = raw_bone_steams.rotations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
			rotation = impl::load_rotation_sample(quantized_ptr, RotationFormat8::Quat_128, k_invalid_bit_rate, are_rotations_normalized);
			rotation = convert_rotation(rotation, RotationFormat8::Quat_128, format);
		}
		else
		{
			const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
			rotation = impl::load_rotation_sample(quantized_ptr, format, k_invalid_bit_rate, are_rotations_normalized);
		}

		// Pack and unpack at our desired bit rate
		const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		Vector4_32 packed_rotation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");
			ACL_ASSERT(segment->are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_bone_range.rotation);

			packed_rotation = decay_vector3_u48(normalized_rotation);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_rotation = rotation;
		else if (are_rotations_normalized)
			packed_rotation = decay_vector3_uXX(rotation, num_bits_at_bit_rate);
		else
			packed_rotation = decay_vector3_sXX(rotation, num_bits_at_bit_rate);

		if (are_rotations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_rotations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL vector_sqrt(Vector4_32Arg0 input)
		{
#if defined(ACL_SSE2_INTRINSICS)
			return _mm_sqrt_ps(input);
#else
			return vector_set(scalar_sqrt(vector_get_x(input)), scalar_sqrt(vector_get_y(input)), scalar_sqrt(vector_get_z(input)), scalar_sqrt(vector_get_w(input)));
#endif
		}

		inline Vector4_32 ACL_SIMD_CALL quat_from_positive_w_soa(Vector4_32Arg0 rotations_x, Vector4_32Arg1 rotations_y, Vector4_32Arg2 rotations_z)
		{
			const Vector4_32 w_squared = vector_sub(vector_sub(vector_sub(vector_set(1.0f), vector_mul(rotations_x, rotations_x)), vector_mul(rotations_y, rotations_y)), vector_mul(rotations_z, rotations_z));
			// w_squared can be negative either due to rounding or due to quantization imprecision, we take the absolute value
			// to ensure the resulting quaternion is always normalized with a positive W component
			return vector_sqrt(vector_abs(w_squared));
		}


		inline Quat_32 ACL_SIMD_CALL get_decayed_rotation_sample(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate)
		{
			const RotationFormat8 raw_format = raw_database.get_rotation_format();
			const RotationFormat8 mutable_format = mutable_database.get_rotation_format();

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			bool is_clip_normalized;
			bool is_segment_normalized;

			Vector4_32 packed_rotation;
			if (is_constant_bit_rate(desired_bit_rate))
			{
				Vector4_32 rotation = raw_database.get_rotation(segment, transform_index, 0);
				rotation = convert_rotation(rotation, raw_format, mutable_format);

				ACL_ASSERT(clip_transform_range.are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

				const Vector4_32 clip_range_min = clip_transform_range.rotation_min;
				const Vector4_32 clip_range_extent = clip_transform_range.rotation_extent;

				const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_range_min, clip_range_extent);

				packed_rotation = decay_vector3_u48(normalized_rotation);

				is_clip_normalized = clip_transform_range.are_rotations_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				const Vector4_32 rotation = raw_database.get_rotation(segment, transform_index, sample_index);
				packed_rotation = convert_rotation(rotation, raw_format, mutable_format);

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const Vector4_32 rotation = mutable_database.get_rotation(segment, transform_index, sample_index);

				if (clip_transform_range.are_rotations_normalized)
					packed_rotation = decay_vector3_uXX(rotation, num_bits_at_bit_rate);
				else
					packed_rotation = decay_vector3_sXX(rotation, num_bits_at_bit_rate);

				is_clip_normalized = clip_transform_range.are_rotations_normalized;
				is_segment_normalized = segment_transform_range.are_rotations_normalized;
			}

			if (is_segment_normalized)
				packed_rotation = vector_mul_add(packed_rotation, segment_transform_range.rotation_extent, segment_transform_range.rotation_min);

			if (is_clip_normalized)
				packed_rotation = vector_mul_add(packed_rotation, clip_transform_range.rotation_extent, clip_transform_range.rotation_min);

			return impl::rotation_to_quat_32(packed_rotation, mutable_format);
		}

		inline void ACL_SIMD_CALL get_decayed_rotation_sample_soa(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate, Quat_32* out_rotations)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			Vector4_32 rotations_x;
			Vector4_32 rotations_y;
			Vector4_32 rotations_z;

			const RotationFormat8 raw_format = raw_database.get_rotation_format();
			const RotationFormat8 mutable_format = mutable_database.get_rotation_format();

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			bool is_clip_normalized;
			bool is_segment_normalized;

			if (is_constant_bit_rate(desired_bit_rate))
			{
				Vector4_32 rotation = raw_database.get_rotation(segment, transform_index, 0);
				rotation = convert_rotation(rotation, raw_format, mutable_format);

				ACL_ASSERT(clip_transform_range.are_rotations_normalized, "Cannot drop a constant track if it isn't normalized");

				const Vector4_32 clip_range_min = clip_transform_range.rotation_min;
				const Vector4_32 clip_range_extent = clip_transform_range.rotation_extent;

				const Vector4_32 normalized_rotation = normalize_sample(rotation, clip_range_min, clip_range_extent);

				const Vector4_32 packed_rotation = decay_vector3_u48(normalized_rotation);

				rotations_x = vector_mix_xxxx(packed_rotation);
				rotations_y = vector_mix_yyyy(packed_rotation);
				rotations_z = vector_mix_zzzz(packed_rotation);

				is_clip_normalized = clip_transform_range.are_rotations_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				const Vector4_32* samples_w;
				raw_database.get_rotations(segment, transform_index, samples_x, samples_y, samples_z, samples_w);

				const uint32_t entry_index = sample_index / 4;
				rotations_x = samples_x[entry_index];
				rotations_y = samples_y[entry_index];
				rotations_z = samples_z[entry_index];
				Vector4_32 rotations_w = samples_w[entry_index];	// We don't care about it, it'll be reconstructed lated

				quat_ensure_positive_w_soa(rotations_x, rotations_y, rotations_z, rotations_w);

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				mutable_database.get_rotations(segment, transform_index, samples_x, samples_y, samples_z);

				const uint32_t entry_index = sample_index / 4;
				rotations_x = samples_x[entry_index];
				rotations_y = samples_y[entry_index];
				rotations_z = samples_z[entry_index];

				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const QuantizationScales scales(num_bits_at_bit_rate);
				if (clip_transform_range.are_rotations_normalized)
					decay_vector3_uXX_soa(rotations_x, rotations_y, rotations_z, scales);
				else
					decay_vector3_sXX_soa(rotations_x, rotations_y, rotations_z, scales);

				is_clip_normalized = clip_transform_range.are_rotations_normalized;
				is_segment_normalized = segment_transform_range.are_rotations_normalized;
			}

			if (is_clip_normalized)
			{
				if (is_segment_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.rotation_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.rotation_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

					rotations_x = vector_mul_add(rotations_x, range_extent_x, range_min_x);
					rotations_y = vector_mul_add(rotations_y, range_extent_y, range_min_y);
					rotations_z = vector_mul_add(rotations_z, range_extent_z, range_min_z);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.rotation_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.rotation_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

				rotations_x = vector_mul_add(rotations_x, range_extent_x, range_min_x);
				rotations_y = vector_mul_add(rotations_y, range_extent_y, range_min_y);
				rotations_z = vector_mul_add(rotations_z, range_extent_z, range_min_z);
			}

			Vector4_32 rotations_w = quat_from_positive_w_soa(rotations_x, rotations_y, rotations_z);

			quat_normalize_soa(rotations_x, rotations_y, rotations_z, rotations_w, rotations_x, rotations_y, rotations_z, rotations_w);

			// Do 16 byte wide stores
			const Vector4_32 rotations_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(rotations_x, rotations_y);
			const Vector4_32 rotations_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(rotations_x, rotations_y);
			const Vector4_32 rotations_z0w0z1w1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(rotations_z, rotations_w);
			const Vector4_32 rotations_z2w2z3w3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(rotations_z, rotations_w);
			const Vector4_32 rotation0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(rotations_x0y0x1y1, rotations_z0w0z1w1);
			const Vector4_32 rotation1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(rotations_x0y0x1y1, rotations_z0w0z1w1);
			const Vector4_32 rotation2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(rotations_x2y2x3y3, rotations_z2w2z3w3);
			const Vector4_32 rotation3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(rotations_x2y2x3y3, rotations_z2w2z3w3);

			out_rotations[0] = vector_to_quat(rotation0);
			out_rotations[1] = vector_to_quat(rotation1);
			out_rotations[2] = vector_to_quat(rotation2);
			out_rotations[3] = vector_to_quat(rotation3);
		}
	}

	inline Quat_32 ACL_SIMD_CALL get_rotation_sample(const BoneStreams& bone_steams, uint32_t sample_index, RotationFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_rotations_normalized = clip_context->are_rotations_normalized && !bone_steams.is_rotation_constant;
		const uint8_t* quantized_ptr = bone_steams.rotations.get_raw_sample_ptr(sample_index);
		const RotationFormat8 format = bone_steams.rotations.get_rotation_format();

		const Vector4_32 rotation = impl::load_rotation_sample(quantized_ptr, format, k_invalid_bit_rate, are_rotations_normalized);

		// Pack and unpack in our desired format
		Vector4_32 packed_rotation;

		switch (desired_format)
		{
		case RotationFormat8::Quat_128:
		case RotationFormat8::QuatDropW_96:
			packed_rotation = rotation;
			break;
		case RotationFormat8::QuatDropW_48:
			packed_rotation = are_rotations_normalized ? decay_vector3_u48(rotation) : decay_vector3_s48(rotation);
			break;
		case RotationFormat8::QuatDropW_32:
			packed_rotation = are_rotations_normalized ? decay_vector3_u32(rotation, 11, 11, 10) : decay_vector3_s32(rotation, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(desired_format));
			packed_rotation = vector_zero_32();
			break;
		}

		if (are_rotations_normalized)
		{
			if (segment->are_rotations_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.rotation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.rotation.get_extent();

				packed_rotation = vector_mul_add(packed_rotation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.rotation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.rotation.get_extent();

			packed_rotation = vector_mul_add(packed_rotation, clip_range_extent, clip_range_min);
		}

		return impl::rotation_to_quat_32(packed_rotation, format);
	}

	namespace acl_impl
	{
		inline Quat_32 ACL_SIMD_CALL get_decayed_rotation_sample(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, RotationFormat8 desired_format)
		{
			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			Vector4_32 rotation = mutable_database.get_rotation(segment, transform_index, sample_index);

			const RotationFormat8 rotation_format = mutable_database.get_rotation_format();
			if (rotation_format == RotationFormat8::Quat_128 && get_rotation_variant(desired_format) == RotationVariant8::QuatDropW)
				rotation = convert_rotation(rotation, rotation_format, desired_format);

			// Pack and unpack in our desired format
			Vector4_32 packed_rotation;

			switch (desired_format)
			{
			case RotationFormat8::Quat_128:
			case RotationFormat8::QuatDropW_96:
				packed_rotation = rotation;
				break;
			case RotationFormat8::QuatDropW_48:
				packed_rotation = clip_transform_range.are_rotations_normalized ? decay_vector3_u48(rotation) : decay_vector3_s48(rotation);
				break;
			case RotationFormat8::QuatDropW_32:
				packed_rotation = clip_transform_range.are_rotations_normalized ? decay_vector3_u32(rotation, 11, 11, 10) : decay_vector3_s32(rotation, 11, 11, 10);
				break;
			default:
				ACL_ASSERT(false, "Unexpected rotation format: %s", get_rotation_format_name(desired_format));
				packed_rotation = vector_zero_32();
				break;
			}

			if (segment_transform_range.are_rotations_normalized)
				packed_rotation = vector_mul_add(packed_rotation, segment_transform_range.rotation_extent, segment_transform_range.rotation_min);

			if (clip_transform_range.are_rotations_normalized)
				packed_rotation = vector_mul_add(packed_rotation, clip_transform_range.rotation_extent, clip_transform_range.rotation_min);

			return impl::rotation_to_quat_32(packed_rotation, desired_format);
		}

		inline void ACL_SIMD_CALL get_decayed_rotation_sample_soa(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, RotationFormat8 desired_format, Quat_32* out_rotations)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			const uint32_t entry_index = sample_index / 4;

			const Vector4_32* samples_x;
			const Vector4_32* samples_y;
			const Vector4_32* samples_z;
			const Vector4_32* samples_w;
			mutable_database.get_rotations(segment, transform_index, samples_x, samples_y, samples_z, samples_w);

			Vector4_32 rotations_x = samples_x[entry_index];
			Vector4_32 rotations_y = samples_y[entry_index];
			Vector4_32 rotations_z = samples_z[entry_index];
			Vector4_32 rotations_w = samples_w[entry_index];

			const RotationFormat8 rotation_format = mutable_database.get_rotation_format();
			if (rotation_format == RotationFormat8::Quat_128 && get_rotation_variant(desired_format) == RotationVariant8::QuatDropW)
				quat_ensure_positive_w_soa(rotations_x, rotations_y, rotations_z, rotations_w);

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);

			const StaticQuantizationScales<16> scales16;
			const StaticQuantizationScales<11> scales11;
			const StaticQuantizationScales<10> scales10;

			// Pack and unpack in our desired format
			switch (desired_format)
			{
			case RotationFormat8::Quat_128:
			case RotationFormat8::QuatDropW_96:
				// Nothing to do
				break;
			case RotationFormat8::QuatDropW_48:
				if (clip_transform_range.are_rotations_normalized)
					decay_vector3_u48_soa(rotations_x, rotations_y, rotations_z, scales16);
				else
					decay_vector3_s48_soa(rotations_x, rotations_y, rotations_z, scales16);
				break;
			case RotationFormat8::QuatDropW_32:
				if (clip_transform_range.are_rotations_normalized)
					decay_vector3_u32_soa(rotations_x, rotations_y, rotations_z, scales11, scales11, scales10);
				else
					decay_vector3_s32_soa(rotations_x, rotations_y, rotations_z, scales11, scales11, scales10);
				break;
			default:
				ACL_ASSERT(false, "Unexpected rotation format: %s", get_rotation_format_name(desired_format));
				break;
			}

			if (clip_transform_range.are_rotations_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_rotations_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.rotation_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);
					const Vector4_32 range_min_w = vector_broadcast(range_min + 3);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.rotation_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);
					const Vector4_32 range_extent_w = vector_broadcast(range_extent + 3);

					rotations_x = vector_mul_add(rotations_x, range_extent_x, range_min_x);
					rotations_y = vector_mul_add(rotations_y, range_extent_y, range_min_y);
					rotations_z = vector_mul_add(rotations_z, range_extent_z, range_min_z);
					rotations_w = vector_mul_add(rotations_w, range_extent_w, range_min_w);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.rotation_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);
				const Vector4_32 range_min_w = vector_broadcast(range_min + 3);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.rotation_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);
				const Vector4_32 range_extent_w = vector_broadcast(range_extent + 3);

				rotations_x = vector_mul_add(rotations_x, range_extent_x, range_min_x);
				rotations_y = vector_mul_add(rotations_y, range_extent_y, range_min_y);
				rotations_z = vector_mul_add(rotations_z, range_extent_z, range_min_z);
				rotations_w = vector_mul_add(rotations_w, range_extent_w, range_min_w);
			}

			if (desired_format != RotationFormat8::Quat_128)
				rotations_w = quat_from_positive_w_soa(rotations_x, rotations_y, rotations_z);

			quat_normalize_soa(rotations_x, rotations_y, rotations_z, rotations_w, rotations_x, rotations_y, rotations_z, rotations_w);

			// Do 16 byte wide stores
			const Vector4_32 rotations_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(rotations_x, rotations_y);
			const Vector4_32 rotations_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(rotations_x, rotations_y);
			const Vector4_32 rotations_z0w0z1w1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(rotations_z, rotations_w);
			const Vector4_32 rotations_z2w2z3w3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(rotations_z, rotations_w);
			const Vector4_32 rotation0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(rotations_x0y0x1y1, rotations_z0w0z1w1);
			const Vector4_32 rotation1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(rotations_x0y0x1y1, rotations_z0w0z1w1);
			const Vector4_32 rotation2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::B>(rotations_x2y2x3y3, rotations_z2w2z3w3);
			const Vector4_32 rotation3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::C, VectorMix::D>(rotations_x2y2x3y3, rotations_z2w2z3w3);

			out_rotations[0] = vector_to_quat(rotation0);
			out_rotations[1] = vector_to_quat(rotation1);
			out_rotations[2] = vector_to_quat(rotation2);
			out_rotations[3] = vector_to_quat(rotation3);
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_translations_normalized = clip_context->are_translations_normalized;

		const VectorFormat8 format = bone_steams.translations.get_vector_format();
		const uint8_t bit_rate = bone_steams.translations.get_bit_rate();

		if (format == VectorFormat8::Vector3_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_translation = impl::load_vector_sample(quantized_ptr, format, bit_rate);

		if (are_translations_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const track_database& database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index)
		{
#if defined(ACL_HAS_ASSERT_CHECKS)
			const VectorFormat8 format = database.get_translation_format();
			ACL_ASSERT(format == VectorFormat8::Vector3_96, "Unexpected translation format");
#endif

			Vector4_32 translation = database.get_translation(segment, transform_index, sample_index);

			const qvvf_ranges& clip_transform_range = database.get_range(transform_index);
			if (clip_transform_range.are_translations_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_translations_normalized)
					translation = vector_mul_add(translation, segment_transform_range.translation_extent, segment_transform_range.translation_min);

				translation = vector_mul_add(translation, clip_transform_range.translation_extent, clip_transform_range.translation_min);
			}

			return translation;
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const VectorFormat8 format = bone_steams.translations.get_vector_format();

		const uint8_t* quantized_ptr;
		if (is_constant_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset);
		else if (is_raw_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.translations.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
		else
			quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);

		const Vector4_32 translation = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		ACL_ASSERT(clip_context->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		Vector4_32 packed_translation;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(segment->are_translations_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_translation = normalize_sample(translation, clip_bone_range.translation);

			packed_translation = decay_vector3_u48(normalized_translation);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_translation = translation;
		else
		{
			const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			packed_translation = decay_vector3_uXX(translation, num_bits_at_bit_rate);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			if (segment->are_translations_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_decayed_translation_sample(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate)
		{
			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			ACL_ASSERT(clip_transform_range.are_translations_normalized, "Cannot drop a constant track if it isn't normalized");

			bool is_clip_normalized;
			bool is_segment_normalized;

			Vector4_32 packed_translation;
			if (is_constant_bit_rate(desired_bit_rate))
			{
				const Vector4_32 translation = raw_database.get_translation(segment, transform_index, 0);

				const Vector4_32 clip_range_min = clip_transform_range.translation_min;
				const Vector4_32 clip_range_extent = clip_transform_range.translation_extent;

				const Vector4_32 normalized_translation = normalize_sample(translation, clip_range_min, clip_range_extent);

				packed_translation = decay_vector3_u48(normalized_translation);

				is_clip_normalized = clip_transform_range.are_translations_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				packed_translation = raw_database.get_translation(segment, transform_index, sample_index);

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const Vector4_32 translation = mutable_database.get_translation(segment, transform_index, sample_index);

				packed_translation = decay_vector3_uXX(translation, num_bits_at_bit_rate);

				is_clip_normalized = clip_transform_range.are_translations_normalized;
				is_segment_normalized = segment_transform_range.are_translations_normalized;
			}

			if (is_segment_normalized)
				packed_translation = vector_mul_add(packed_translation, segment_transform_range.translation_extent, segment_transform_range.translation_min);

			if (is_clip_normalized)
				packed_translation = vector_mul_add(packed_translation, clip_transform_range.translation_extent, clip_transform_range.translation_min);

			return packed_translation;
		}

		inline void ACL_SIMD_CALL get_decayed_translation_sample_soa(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate, Vector4_32* out_translations)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			ACL_ASSERT(clip_transform_range.are_translations_normalized, "Cannot drop a constant track if it isn't normalized");

			Vector4_32 translations_x;
			Vector4_32 translations_y;
			Vector4_32 translations_z;

			bool is_clip_normalized;
			bool is_segment_normalized;

			if (is_constant_bit_rate(desired_bit_rate))
			{
				Vector4_32 translation = raw_database.get_translation(segment, transform_index, 0);

				const Vector4_32 clip_range_min = clip_transform_range.translation_min;
				const Vector4_32 clip_range_extent = clip_transform_range.translation_extent;

				const Vector4_32 normalized_translation = normalize_sample(translation, clip_range_min, clip_range_extent);

				const Vector4_32 packed_translation = decay_vector3_u48(normalized_translation);

				translations_x = vector_mix_xxxx(packed_translation);
				translations_y = vector_mix_yyyy(packed_translation);
				translations_z = vector_mix_zzzz(packed_translation);

				is_clip_normalized = clip_transform_range.are_translations_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				raw_database.get_translations(segment, transform_index, samples_x, samples_y, samples_z);

				const uint32_t entry_index = sample_index / 4;
				translations_x = samples_x[entry_index];
				translations_y = samples_y[entry_index];
				translations_z = samples_z[entry_index];

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				mutable_database.get_translations(segment, transform_index, samples_x, samples_y, samples_z);

				const uint32_t entry_index = sample_index / 4;
				translations_x = samples_x[entry_index];
				translations_y = samples_y[entry_index];
				translations_z = samples_z[entry_index];

				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const QuantizationScales scales(num_bits_at_bit_rate);
				decay_vector3_uXX_soa(translations_x, translations_y, translations_z, scales);

				is_clip_normalized = clip_transform_range.are_translations_normalized;
				is_segment_normalized = segment_transform_range.are_translations_normalized;
			}

			if (is_clip_normalized)
			{
				if (is_segment_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.translation_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.translation_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

					translations_x = vector_mul_add(translations_x, range_extent_x, range_min_x);
					translations_y = vector_mul_add(translations_y, range_extent_y, range_min_y);
					translations_z = vector_mul_add(translations_z, range_extent_z, range_min_z);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.translation_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.translation_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

				translations_x = vector_mul_add(translations_x, range_extent_x, range_min_x);
				translations_y = vector_mul_add(translations_y, range_extent_y, range_min_y);
				translations_z = vector_mul_add(translations_z, range_extent_z, range_min_z);
			}

			// Do 16 byte wide stores
			const Vector4_32 translations_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(translations_x, translations_y);
			const Vector4_32 translations_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(translations_x, translations_y);
			const Vector4_32 translation0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::A>(translations_x0y0x1y1, translations_z);
			const Vector4_32 translation1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::B, VectorMix::B>(translations_x0y0x1y1, translations_z);
			const Vector4_32 translation2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::C, VectorMix::C>(translations_x2y2x3y3, translations_z);
			const Vector4_32 translation3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::D, VectorMix::D>(translations_x2y2x3y3, translations_z);

			out_translations[0] = vector_to_quat(translation0);
			out_translations[1] = vector_to_quat(translation1);
			out_translations[2] = vector_to_quat(translation2);
			out_translations[3] = vector_to_quat(translation3);
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_translation_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_translations_normalized = clip_context->are_translations_normalized && !bone_steams.is_translation_constant;
		const uint8_t* quantized_ptr = bone_steams.translations.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.translations.get_vector_format();

		const Vector4_32 translation = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		Vector4_32 packed_translation;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_translation = translation;
			break;
		case VectorFormat8::Vector3_48:
			ACL_ASSERT(are_translations_normalized, "Translations must be normalized to support this format");
			packed_translation = decay_vector3_u48(translation);
			break;
		case VectorFormat8::Vector3_32:
			ACL_ASSERT(are_translations_normalized, "Translations must be normalized to support this format");
			packed_translation = decay_vector3_u32(translation, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_translation = vector_zero_32();
			break;
		}

		if (are_translations_normalized)
		{
			if (segment->are_translations_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				Vector4_32 segment_range_min = segment_bone_range.translation.get_min();
				Vector4_32 segment_range_extent = segment_bone_range.translation.get_extent();

				packed_translation = vector_mul_add(packed_translation, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.translation.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.translation.get_extent();

			packed_translation = vector_mul_add(packed_translation, clip_range_extent, clip_range_min);
		}

		return packed_translation;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_decayed_translation_sample(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, VectorFormat8 desired_format)
		{
			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			const Vector4_32 translation = mutable_database.get_translation(segment, transform_index, sample_index);

			// Pack and unpack in our desired format
			Vector4_32 packed_translation;

			switch (desired_format)
			{
			case VectorFormat8::Vector3_96:
				packed_translation = translation;
				break;
			case VectorFormat8::Vector3_48:
				ACL_ASSERT(clip_transform_range.are_translations_normalized, "Translations must be normalized to support this format");
				packed_translation = decay_vector3_u48(translation);
				break;
			case VectorFormat8::Vector3_32:
				ACL_ASSERT(clip_transform_range.are_translations_normalized, "Translations must be normalized to support this format");
				packed_translation = decay_vector3_u32(translation, 11, 11, 10);
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
				packed_translation = vector_zero_32();
				break;
			}

			if (segment_transform_range.are_translations_normalized)
				packed_translation = vector_mul_add(packed_translation, segment_transform_range.translation_extent, segment_transform_range.translation_min);

			if (clip_transform_range.are_translations_normalized)
				packed_translation = vector_mul_add(packed_translation, clip_transform_range.translation_extent, clip_transform_range.translation_min);

			return packed_translation;
		}

		inline void ACL_SIMD_CALL get_decayed_translation_sample_soa(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, VectorFormat8 desired_format, Vector4_32* out_translations)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			const uint32_t entry_index = sample_index / 4;

			const Vector4_32* samples_x;
			const Vector4_32* samples_y;
			const Vector4_32* samples_z;
			mutable_database.get_translations(segment, transform_index, samples_x, samples_y, samples_z);

			Vector4_32 translations_x = samples_x[entry_index];
			Vector4_32 translations_y = samples_y[entry_index];
			Vector4_32 translations_z = samples_z[entry_index];

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);

			const StaticQuantizationScales<16> scales16;
			const StaticQuantizationScales<11> scales11;
			const StaticQuantizationScales<10> scales10;

			// Pack and unpack in our desired format
			switch (desired_format)
			{
			case VectorFormat8::Vector3_96:
				// Nothing to do
				break;
			case VectorFormat8::Vector3_48:
				ACL_ASSERT(clip_transform_range.are_translations_normalized, "Translations must be normalized to support this format");
				decay_vector3_u48_soa(translations_x, translations_y, translations_z, scales16);
				break;
			case VectorFormat8::Vector3_32:
				ACL_ASSERT(clip_transform_range.are_translations_normalized, "Translations must be normalized to support this format");
				decay_vector3_u32_soa(translations_x, translations_y, translations_z, scales11, scales11, scales10);
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
				break;
			}

			if (clip_transform_range.are_translations_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_translations_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.translation_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.translation_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

					translations_x = vector_mul_add(translations_x, range_extent_x, range_min_x);
					translations_y = vector_mul_add(translations_y, range_extent_y, range_min_y);
					translations_z = vector_mul_add(translations_z, range_extent_z, range_min_z);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.translation_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.translation_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

				translations_x = vector_mul_add(translations_x, range_extent_x, range_min_x);
				translations_y = vector_mul_add(translations_y, range_extent_y, range_min_y);
				translations_z = vector_mul_add(translations_z, range_extent_z, range_min_z);
			}

			// Do 16 byte wide stores
			const Vector4_32 translations_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(translations_x, translations_y);
			const Vector4_32 translations_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(translations_x, translations_y);
			const Vector4_32 translation0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::A>(translations_x0y0x1y1, translations_z);
			const Vector4_32 translation1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::B, VectorMix::B>(translations_x0y0x1y1, translations_z);
			const Vector4_32 translation2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::C, VectorMix::C>(translations_x2y2x3y3, translations_z);
			const Vector4_32 translation3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::D, VectorMix::D>(translations_x2y2x3y3, translations_z);

			out_translations[0] = vector_to_quat(translation0);
			out_translations[1] = vector_to_quat(translation1);
			out_translations[2] = vector_to_quat(translation2);
			out_translations[3] = vector_to_quat(translation3);
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_scales_normalized = clip_context->are_scales_normalized;

		const VectorFormat8 format = bone_steams.scales.get_vector_format();
		const uint8_t bit_rate = bone_steams.scales.get_bit_rate();

		if (format == VectorFormat8::Vector3_Variable && is_constant_bit_rate(bit_rate))
			sample_index = 0;

		const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

		Vector4_32 packed_scale = impl::load_vector_sample(quantized_ptr, format, bit_rate);

		if (are_scales_normalized && !is_raw_bit_rate(bit_rate))
		{
			if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const track_database& database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index)
		{
#if defined(ACL_HAS_ASSERT_CHECKS)
			const VectorFormat8 format = database.get_scale_format();
			ACL_ASSERT(format == VectorFormat8::Vector3_96, "Unexpected scale format");
#endif

			Vector4_32 scale = database.get_scale(segment, transform_index, sample_index);

			const qvvf_ranges& clip_transform_range = database.get_range(transform_index);
			if (clip_transform_range.are_scales_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_scales_normalized)
					scale = vector_mul_add(scale, segment_transform_range.scale_extent, segment_transform_range.scale_min);

				scale = vector_mul_add(scale, clip_transform_range.scale_extent, clip_transform_range.scale_min);
			}

			return scale;
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, const BoneStreams& raw_bone_steams, uint32_t sample_index, uint8_t bit_rate)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const VectorFormat8 format = bone_steams.scales.get_vector_format();

		const uint8_t* quantized_ptr;
		if (is_constant_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset);
		else if (is_raw_bit_rate(bit_rate))
			quantized_ptr = raw_bone_steams.scales.get_raw_sample_ptr(segment->clip_sample_offset + sample_index);
		else
			quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);

		const Vector4_32 scale = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		ACL_ASSERT(clip_context->are_scales_normalized, "Scales must be normalized to support variable bit rates.");

		// Pack and unpack at our desired bit rate
		Vector4_32 packed_scale;

		if (is_constant_bit_rate(bit_rate))
		{
			ACL_ASSERT(segment->are_scales_normalized, "Translations must be normalized to support variable bit rates.");

			const BoneRanges& clip_bone_range = segment->clip->ranges[bone_steams.bone_index];
			const Vector4_32 normalized_scale = normalize_sample(scale, clip_bone_range.scale);

			packed_scale = decay_vector3_u48(normalized_scale);
		}
		else if (is_raw_bit_rate(bit_rate))
			packed_scale = scale;
		else
		{
			const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			packed_scale = decay_vector3_uXX(scale, num_bits_at_bit_rate);
		}

		if (!is_raw_bit_rate(bit_rate))
		{
			if (segment->are_scales_normalized && !is_constant_bit_rate(bit_rate))
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				const Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				const Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			const Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			const Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_decayed_scale_sample(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate)
		{
			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			ACL_ASSERT(clip_transform_range.are_scales_normalized, "Cannot drop a constant track if it isn't normalized");

			bool is_clip_normalized;
			bool is_segment_normalized;

			Vector4_32 packed_scale;
			if (is_constant_bit_rate(desired_bit_rate))
			{
				const Vector4_32 scale = raw_database.get_scale(segment, transform_index, 0);

				const Vector4_32 clip_range_min = clip_transform_range.scale_min;
				const Vector4_32 clip_range_extent = clip_transform_range.scale_extent;

				const Vector4_32 normalized_scale = normalize_sample(scale, clip_range_min, clip_range_extent);

				packed_scale = decay_vector3_u48(normalized_scale);

				is_clip_normalized = clip_transform_range.are_scales_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				packed_scale = raw_database.get_scale(segment, transform_index, sample_index);

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const Vector4_32 scale = mutable_database.get_scale(segment, transform_index, sample_index);

				packed_scale = decay_vector3_uXX(scale, num_bits_at_bit_rate);

				is_clip_normalized = clip_transform_range.are_scales_normalized;
				is_segment_normalized = segment_transform_range.are_scales_normalized;
			}

			if (is_segment_normalized)
				packed_scale = vector_mul_add(packed_scale, segment_transform_range.scale_extent, segment_transform_range.scale_min);

			if (is_clip_normalized)
				packed_scale = vector_mul_add(packed_scale, clip_transform_range.scale_extent, clip_transform_range.scale_min);

			return packed_scale;
		}

		inline void ACL_SIMD_CALL get_decayed_scale_sample_soa(const track_database& raw_database, const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, uint8_t desired_bit_rate, Vector4_32* out_scales)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			ACL_ASSERT(clip_transform_range.are_scales_normalized, "Cannot drop a constant track if it isn't normalized");

			Vector4_32 scales_x;
			Vector4_32 scales_y;
			Vector4_32 scales_z;

			bool is_clip_normalized;
			bool is_segment_normalized;

			if (is_constant_bit_rate(desired_bit_rate))
			{
				Vector4_32 scale = raw_database.get_scale(segment, transform_index, 0);

				const Vector4_32 clip_range_min = clip_transform_range.scale_min;
				const Vector4_32 clip_range_extent = clip_transform_range.scale_extent;

				const Vector4_32 normalized_scale = normalize_sample(scale, clip_range_min, clip_range_extent);

				const Vector4_32 packed_scale = decay_vector3_u48(normalized_scale);

				scales_x = vector_mix_xxxx(packed_scale);
				scales_y = vector_mix_yyyy(packed_scale);
				scales_z = vector_mix_zzzz(packed_scale);

				is_clip_normalized = clip_transform_range.are_scales_normalized;
				is_segment_normalized = false;
			}
			else if (is_raw_bit_rate(desired_bit_rate))
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				raw_database.get_scales(segment, transform_index, samples_x, samples_y, samples_z);

				const uint32_t entry_index = sample_index / 4;
				scales_x = samples_x[entry_index];
				scales_y = samples_y[entry_index];
				scales_z = samples_z[entry_index];

				is_clip_normalized = false;
				is_segment_normalized = false;
			}
			else
			{
				const Vector4_32* samples_x;
				const Vector4_32* samples_y;
				const Vector4_32* samples_z;
				mutable_database.get_scales(segment, transform_index, samples_x, samples_y, samples_z);

				const uint32_t entry_index = sample_index / 4;
				scales_x = samples_x[entry_index];
				scales_y = samples_y[entry_index];
				scales_z = samples_z[entry_index];

				const uint32_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(desired_bit_rate);
				const QuantizationScales scales(num_bits_at_bit_rate);
				decay_vector3_uXX_soa(scales_x, scales_y, scales_z, scales);

				is_clip_normalized = clip_transform_range.are_scales_normalized;
				is_segment_normalized = segment_transform_range.are_scales_normalized;
			}

			if (is_clip_normalized)
			{
				if (is_segment_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.scale_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.scale_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

					scales_x = vector_mul_add(scales_x, range_extent_x, range_min_x);
					scales_y = vector_mul_add(scales_y, range_extent_y, range_min_y);
					scales_z = vector_mul_add(scales_z, range_extent_z, range_min_z);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.scale_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.scale_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

				scales_x = vector_mul_add(scales_x, range_extent_x, range_min_x);
				scales_y = vector_mul_add(scales_y, range_extent_y, range_min_y);
				scales_z = vector_mul_add(scales_z, range_extent_z, range_min_z);
			}

			// Do 16 byte wide stores
			const Vector4_32 scales_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(scales_x, scales_y);
			const Vector4_32 scales_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(scales_x, scales_y);
			const Vector4_32 scale0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::A>(scales_x0y0x1y1, scales_z);
			const Vector4_32 scale1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::B, VectorMix::B>(scales_x0y0x1y1, scales_z);
			const Vector4_32 scale2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::C, VectorMix::C>(scales_x2y2x3y3, scales_z);
			const Vector4_32 scale3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::D, VectorMix::D>(scales_x2y2x3y3, scales_z);

			out_scales[0] = vector_to_quat(scale0);
			out_scales[1] = vector_to_quat(scale1);
			out_scales[2] = vector_to_quat(scale2);
			out_scales[3] = vector_to_quat(scale3);
		}
	}

	inline Vector4_32 ACL_SIMD_CALL get_scale_sample(const BoneStreams& bone_steams, uint32_t sample_index, VectorFormat8 desired_format)
	{
		const SegmentContext* segment = bone_steams.segment;
		const ClipContext* clip_context = segment->clip;
		const bool are_scales_normalized = clip_context->are_scales_normalized && !bone_steams.is_scale_constant;
		const uint8_t* quantized_ptr = bone_steams.scales.get_raw_sample_ptr(sample_index);
		const VectorFormat8 format = bone_steams.scales.get_vector_format();

		const Vector4_32 scale = impl::load_vector_sample(quantized_ptr, format, k_invalid_bit_rate);

		// Pack and unpack in our desired format
		Vector4_32 packed_scale;

		switch (desired_format)
		{
		case VectorFormat8::Vector3_96:
			packed_scale = scale;
			break;
		case VectorFormat8::Vector3_48:
			ACL_ASSERT(are_scales_normalized, "Scales must be normalized to support this format");
			packed_scale = decay_vector3_u48(scale);
			break;
		case VectorFormat8::Vector3_32:
			ACL_ASSERT(are_scales_normalized, "Scales must be normalized to support this format");
			packed_scale = decay_vector3_u32(scale, 11, 11, 10);
			break;
		default:
			ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
			packed_scale = scale;
			break;
		}

		if (are_scales_normalized)
		{
			if (segment->are_scales_normalized)
			{
				const BoneRanges& segment_bone_range = segment->ranges[bone_steams.bone_index];

				Vector4_32 segment_range_min = segment_bone_range.scale.get_min();
				Vector4_32 segment_range_extent = segment_bone_range.scale.get_extent();

				packed_scale = vector_mul_add(packed_scale, segment_range_extent, segment_range_min);
			}

			const BoneRanges& clip_bone_range = clip_context->ranges[bone_steams.bone_index];

			Vector4_32 clip_range_min = clip_bone_range.scale.get_min();
			Vector4_32 clip_range_extent = clip_bone_range.scale.get_extent();

			packed_scale = vector_mul_add(packed_scale, clip_range_extent, clip_range_min);
		}

		return packed_scale;
	}

	namespace acl_impl
	{
		inline Vector4_32 ACL_SIMD_CALL get_decayed_scale_sample(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, VectorFormat8 desired_format)
		{
			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);
			const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];

			const Vector4_32 scale = mutable_database.get_scale(segment, transform_index, sample_index);

			// Pack and unpack in our desired format
			Vector4_32 packed_scale;

			switch (desired_format)
			{
			case VectorFormat8::Vector3_96:
				packed_scale = scale;
				break;
			case VectorFormat8::Vector3_48:
				ACL_ASSERT(clip_transform_range.are_scales_normalized, "Scales must be normalized to support this format");
				packed_scale = decay_vector3_u48(scale);
				break;
			case VectorFormat8::Vector3_32:
				ACL_ASSERT(clip_transform_range.are_scales_normalized, "Scales must be normalized to support this format");
				packed_scale = decay_vector3_u32(scale, 11, 11, 10);
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
				packed_scale = vector_zero_32();
				break;
			}

			if (segment_transform_range.are_scales_normalized)
				packed_scale = vector_mul_add(packed_scale, segment_transform_range.scale_extent, segment_transform_range.scale_min);

			if (clip_transform_range.are_scales_normalized)
				packed_scale = vector_mul_add(packed_scale, clip_transform_range.scale_extent, clip_transform_range.scale_min);

			return packed_scale;
		}

		inline void ACL_SIMD_CALL get_decayed_scale_sample_soa(const track_database& mutable_database, const segment_context& segment, uint32_t transform_index, uint32_t sample_index, VectorFormat8 desired_format, Vector4_32* out_scales)
		{
			ACL_ASSERT((sample_index % 4) == 0, "SOA decay requires a multiple of 4 sample index");

			const uint32_t entry_index = sample_index / 4;

			const Vector4_32* samples_x;
			const Vector4_32* samples_y;
			const Vector4_32* samples_z;
			mutable_database.get_scales(segment, transform_index, samples_x, samples_y, samples_z);

			Vector4_32 scales_x = samples_x[entry_index];
			Vector4_32 scales_y = samples_y[entry_index];
			Vector4_32 scales_z = samples_z[entry_index];

			const qvvf_ranges& clip_transform_range = mutable_database.get_range(transform_index);

			const StaticQuantizationScales<16> scales16;
			const StaticQuantizationScales<11> scales11;
			const StaticQuantizationScales<10> scales10;

			// Pack and unpack in our desired format
			switch (desired_format)
			{
			case VectorFormat8::Vector3_96:
				// Nothing to do
				break;
			case VectorFormat8::Vector3_48:
				ACL_ASSERT(clip_transform_range.are_scales_normalized, "Scales must be normalized to support this format");
				decay_vector3_u48_soa(scales_x, scales_y, scales_z, scales16);
				break;
			case VectorFormat8::Vector3_32:
				ACL_ASSERT(clip_transform_range.are_scales_normalized, "Scales must be normalized to support this format");
				decay_vector3_u32_soa(scales_x, scales_y, scales_z, scales11, scales11, scales10);
				break;
			default:
				ACL_ASSERT(false, "Invalid or unsupported vector format: %s", get_vector_format_name(desired_format));
				break;
			}

			if (clip_transform_range.are_scales_normalized)
			{
				const qvvf_ranges& segment_transform_range = segment.ranges[transform_index];
				if (segment_transform_range.are_scales_normalized)
				{
					// TODO: Use scalarf, better for ARM?
					const float* range_min = vector_as_float_ptr(segment_transform_range.scale_min);
					const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
					const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
					const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

					const float* range_extent = vector_as_float_ptr(segment_transform_range.scale_extent);
					const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
					const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
					const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

					scales_x = vector_mul_add(scales_x, range_extent_x, range_min_x);
					scales_y = vector_mul_add(scales_y, range_extent_y, range_min_y);
					scales_z = vector_mul_add(scales_z, range_extent_z, range_min_z);
				}

				// TODO: Use scalarf, better for ARM?
				const float* range_min = vector_as_float_ptr(clip_transform_range.scale_min);
				const Vector4_32 range_min_x = vector_broadcast(range_min + 0);
				const Vector4_32 range_min_y = vector_broadcast(range_min + 1);
				const Vector4_32 range_min_z = vector_broadcast(range_min + 2);

				const float* range_extent = vector_as_float_ptr(clip_transform_range.scale_extent);
				const Vector4_32 range_extent_x = vector_broadcast(range_extent + 0);
				const Vector4_32 range_extent_y = vector_broadcast(range_extent + 1);
				const Vector4_32 range_extent_z = vector_broadcast(range_extent + 2);

				scales_x = vector_mul_add(scales_x, range_extent_x, range_min_x);
				scales_y = vector_mul_add(scales_y, range_extent_y, range_min_y);
				scales_z = vector_mul_add(scales_z, range_extent_z, range_min_z);
			}

			// Do 16 byte wide stores
			const Vector4_32 scales_x0y0x1y1 = vector_mix<VectorMix::X, VectorMix::A, VectorMix::Y, VectorMix::B>(scales_x, scales_y);
			const Vector4_32 scales_x2y2x3y3 = vector_mix<VectorMix::Z, VectorMix::C, VectorMix::W, VectorMix::D>(scales_x, scales_y);
			const Vector4_32 scale0 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::A, VectorMix::A>(scales_x0y0x1y1, scales_z);
			const Vector4_32 scale1 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::B, VectorMix::B>(scales_x0y0x1y1, scales_z);
			const Vector4_32 scale2 = vector_mix<VectorMix::X, VectorMix::Y, VectorMix::C, VectorMix::C>(scales_x2y2x3y3, scales_z);
			const Vector4_32 scale3 = vector_mix<VectorMix::Z, VectorMix::W, VectorMix::D, VectorMix::D>(scales_x2y2x3y3, scales_z);

			out_scales[0] = vector_to_quat(scale0);
			out_scales[1] = vector_to_quat(scale1);
			out_scales[2] = vector_to_quat(scale2);
			out_scales[3] = vector_to_quat(scale3);
		}
	}

	namespace acl_impl
	{
		struct sample_context
		{
			uint32_t track_index;

			uint32_t sample_key;
			float sample_time;

			BoneBitRate bit_rates;
		};

		inline uint32_t get_uniform_sample_key(uint32_t num_samples_per_track_in_clip, float sample_rate, uint32_t num_samples_per_track_in_segment, uint32_t segment_start_offset, float sample_time)
		{
			uint32_t key0 = 0;
			uint32_t key1 = 0;
			float interpolation_alpha = 0.0f;

			// Our samples are uniform, grab the nearest samples
			find_linear_interpolation_samples_with_sample_rate(num_samples_per_track_in_clip, sample_rate, sample_time, SampleRoundingPolicy::Nearest, key0, key1, interpolation_alpha);

			// Offset for the current segment and clamp
			key0 = key0 - segment_start_offset;
			if (key0 >= num_samples_per_track_in_segment)
			{
				key0 = 0;
				interpolation_alpha = 1.0f;
			}

			key1 = key1 - segment_start_offset;
			if (key1 >= num_samples_per_track_in_segment)
			{
				key1 = num_samples_per_track_in_segment - 1;
				interpolation_alpha = 0.0f;
			}

			// When we sample uniformly, we always round to the nearest sample.
			// As such, we don't need to interpolate.
			return interpolation_alpha == 0.0f ? key0 : key1;
		}

		inline uint32_t get_uniform_sample_key(const SegmentContext& segment, float sample_time)
		{
			const ClipContext* clip_context = segment.clip;
			return get_uniform_sample_key(clip_context->num_samples, clip_context->sample_rate, segment.num_samples, segment.clip_sample_offset, sample_time);
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Quat_32 ACL_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream)
		{
			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = quat_identity_32();
			else if (bone_stream.is_rotation_constant)
				rotation = quat_normalize(get_rotation_sample(bone_stream, 0));
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.rotations.get_num_samples();
					const float sample_rate = bone_stream.rotations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Quat_32 sample0 = get_rotation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Quat_32 sample1 = get_rotation_sample(bone_stream, key1);
					rotation = quat_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					rotation = quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Quat_32 ACL_SIMD_CALL sample_rotation(const sample_context& context, const track_database& database, const segment_context& segment)
		{
			const qvvf_ranges& transform_range = database.get_range(context.track_index);

			Quat_32 rotation;
			if (transform_range.is_rotation_default)
				rotation = quat_identity_32();
			else if (transform_range.is_rotation_constant)
				rotation = quat_normalize(get_rotation_sample(database, segment, context.track_index, 0));
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = segment.num_samples_per_track;
					const float sample_rate = database.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Quat_32 sample0 = get_rotation_sample(database, segment, context.track_index, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Quat_32 sample1 = get_rotation_sample(database, segment, context.track_index, key1);
					rotation = quat_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					rotation = quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Quat_32 ACL_SIMD_CALL sample_rotation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_rotation_variable, RotationFormat8 rotation_format)
		{
			Quat_32 rotation;
			if (bone_stream.is_rotation_default)
				rotation = quat_identity_32();
			else if (bone_stream.is_rotation_constant)
			{
				if (is_rotation_variable)
					rotation = get_rotation_sample(raw_bone_stream, 0);
				else
					rotation = get_rotation_sample(raw_bone_stream, 0, rotation_format);

				rotation = quat_normalize(rotation);
			}
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.rotations.get_num_samples();
					const float sample_rate = bone_stream.rotations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				Quat_32 sample0;
				Quat_32 sample1;
				if (is_rotation_variable)
				{
					sample0 = get_rotation_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.rotation);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_rotation_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.rotation);
				}
				else
				{
					sample0 = get_rotation_sample(bone_stream, key0, rotation_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_rotation_sample(bone_stream, key1, rotation_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					rotation = quat_lerp(sample0, sample1, interpolation_alpha);
				else
					rotation = quat_normalize(sample0);
			}

			return rotation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream)
		{
			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = vector_zero_32();
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(bone_stream, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.translations.get_num_samples();
					const float sample_rate = bone_stream.translations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Vector4_32 sample0 = get_translation_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_translation_sample(bone_stream, key1);
					translation = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_translation(const sample_context& context, const track_database& database, const segment_context& segment)
		{
			const qvvf_ranges& transform_range = database.get_range(context.track_index);

			Vector4_32 translation;
			if (transform_range.is_translation_default)
				translation = vector_zero_32();
			else if (transform_range.is_translation_constant)
				translation = get_translation_sample(database, segment, context.track_index, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = segment.num_samples_per_track;
					const float sample_rate = database.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Vector4_32 sample0 = get_translation_sample(database, segment, context.track_index, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_translation_sample(database, segment, context.track_index, key1);
					translation = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_translation(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_translation_variable, VectorFormat8 translation_format)
		{
			Vector4_32 translation;
			if (bone_stream.is_translation_default)
				translation = vector_zero_32();
			else if (bone_stream.is_translation_constant)
				translation = get_translation_sample(raw_bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.translations.get_num_samples();
					const float sample_rate = bone_stream.translations.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_translation_variable)
				{
					sample0 = get_translation_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.translation);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_translation_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.translation);
				}
				else
				{
					sample0 = get_translation_sample(bone_stream, key0, translation_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_translation_sample(bone_stream, key1, translation_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					translation = vector_lerp(sample0, sample1, interpolation_alpha);
				else
					translation = sample0;
			}

			return translation;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, Vector4_32Arg0 default_scale)
		{
			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(bone_stream, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.scales.get_num_samples();
					const float sample_rate = bone_stream.scales.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Vector4_32 sample0 = get_scale_sample(bone_stream, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_scale_sample(bone_stream, key1);
					scale = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					scale = sample0;
			}

			return scale;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_scale(const sample_context& context, const track_database& database, const segment_context& segment)
		{
			const qvvf_ranges& transform_range = database.get_range(context.track_index);

			Vector4_32 scale;
			if (transform_range.is_scale_default)
				scale = database.get_default_scale();
			else if (transform_range.is_scale_constant)
				scale = get_scale_sample(database, segment, context.track_index, 0);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = segment.num_samples_per_track;
					const float sample_rate = database.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				const Vector4_32 sample0 = get_scale_sample(database, segment, context.track_index, key0);

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const Vector4_32 sample1 = get_scale_sample(database, segment, context.track_index, key1);
					scale = vector_lerp(sample0, sample1, interpolation_alpha);
				}
				else
					scale = sample0;
			}

			return scale;
		}

		template<SampleDistribution8 distribution>
		ACL_FORCE_INLINE Vector4_32 ACL_SIMD_CALL sample_scale(const sample_context& context, const BoneStreams& bone_stream, const BoneStreams& raw_bone_stream, bool is_scale_variable, VectorFormat8 scale_format, Vector4_32Arg0 default_scale)
		{
			Vector4_32 scale;
			if (bone_stream.is_scale_default)
				scale = default_scale;
			else if (bone_stream.is_scale_constant)
				scale = get_scale_sample(raw_bone_stream, 0, VectorFormat8::Vector3_96);
			else
			{
				uint32_t key0;
				uint32_t key1;
				float interpolation_alpha;
				if (static_condition<distribution == SampleDistribution8::Variable>::test())
				{
					const uint32_t num_samples = bone_stream.scales.get_num_samples();
					const float sample_rate = bone_stream.scales.get_sample_rate();

					find_linear_interpolation_samples_with_sample_rate(num_samples, sample_rate, context.sample_time, SampleRoundingPolicy::None, key0, key1, interpolation_alpha);
				}
				else
				{
					key0 = context.sample_key;
					key1 = 0;
					interpolation_alpha = 0.0f;
				}

				Vector4_32 sample0;
				Vector4_32 sample1;
				if (is_scale_variable)
				{
					sample0 = get_scale_sample(bone_stream, raw_bone_stream, key0, context.bit_rates.scale);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_scale_sample(bone_stream, raw_bone_stream, key1, context.bit_rates.scale);
				}
				else
				{
					sample0 = get_scale_sample(bone_stream, key0, scale_format);

					if (static_condition<distribution == SampleDistribution8::Variable>::test())
						sample1 = get_scale_sample(bone_stream, key1, scale_format);
				}

				if (static_condition<distribution == SampleDistribution8::Variable>::test())
					scale = vector_lerp(sample0, sample1, interpolation_alpha);
				else
					scale = sample0;
			}

			return scale;
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, Transform_32* out_local_pose)
	{
		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;

				const BoneStreams& bone_stream = bone_streams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
		else
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;

				const BoneStreams& bone_stream = bone_streams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
	}

	inline void sample_stream(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.track_index = bone_index;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		const BoneStreams& bone_stream = bone_streams[bone_index];

		Quat_32 rotation;
		Vector4_32 translation;
		Vector4_32 scale;
		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
			translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;
		}
		else
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
			translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;
		}

		out_local_pose[bone_index] = transform_set(rotation, translation, scale);
	}

	namespace acl_impl
	{
		inline void sample_database(const track_database& database, const segment_context& segment, float sample_time, uint32_t transform_index, Transform_32* out_local_pose)
		{
			const Vector4_32 default_scale = database.get_default_scale();
			const bool has_scale = database.has_scale();

			acl_impl::sample_context context;
			context.track_index = transform_index;
			context.sample_time = sample_time;

			Quat_32 rotation;
			Vector4_32 translation;
			Vector4_32 scale;
			if (segment.distribution == SampleDistribution8::Uniform)
			{
				const uint32_t num_samples_per_track_in_clip = database.get_num_samples_per_track();
				const uint32_t num_samples_per_track_in_segment = segment.num_samples_per_track;
				const uint32_t segment_sample_start_offset = segment.start_offset;
				const float sample_rate = database.get_sample_rate();

				context.sample_key = get_uniform_sample_key(num_samples_per_track_in_clip, sample_rate, num_samples_per_track_in_segment, segment_sample_start_offset, sample_time);

				rotation = sample_rotation<SampleDistribution8::Uniform>(context, database, segment);
				translation = sample_translation<SampleDistribution8::Uniform>(context, database, segment);
				scale = has_scale ? sample_scale<SampleDistribution8::Uniform>(context, database, segment) : default_scale;
			}
			else
			{
				context.sample_key = 0;

				rotation = sample_rotation<SampleDistribution8::Variable>(context, database, segment);
				translation = sample_translation<SampleDistribution8::Variable>(context, database, segment);
				scale = has_scale ? sample_scale<SampleDistribution8::Variable>(context, database, segment) : default_scale;
			}

			out_local_pose[transform_index] = transform_set(rotation, translation, scale);
		}
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, uint16_t num_bones, float sample_time, uint16_t bone_index, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;

				const BoneStreams& bone_stream = bone_streams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
		else
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;

				const BoneStreams& bone_stream = bone_streams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
	}

	namespace acl_impl
	{
		inline void sample_database_hierarchical(const track_database& database, const segment_context& segment, float sample_time, uint32_t target_transform_index, Transform_32* out_local_pose)
		{
			const Vector4_32 default_scale = database.get_default_scale();
			const bool has_scale = database.has_scale();

			acl_impl::sample_context context;
			context.sample_time = sample_time;

			if (segment.distribution == SampleDistribution8::Uniform)
			{
				const uint32_t num_samples_per_track_in_clip = database.get_num_samples_per_track();
				const uint32_t num_samples_per_track_in_segment = segment.num_samples_per_track;
				const uint32_t segment_sample_start_offset = segment.start_offset;
				const float sample_rate = database.get_sample_rate();

				context.sample_key = get_uniform_sample_key(num_samples_per_track_in_clip, sample_rate, num_samples_per_track_in_segment, segment_sample_start_offset, sample_time);

				uint32_t current_transform_index = target_transform_index;
				while (current_transform_index != k_invalid_bone_index)
				{
					context.track_index = current_transform_index;

					const Quat_32 rotation = sample_rotation<SampleDistribution8::Uniform>(context, database, segment);
					const Vector4_32 translation = sample_translation<SampleDistribution8::Uniform>(context, database, segment);
					const Vector4_32 scale = has_scale ? sample_scale<SampleDistribution8::Uniform>(context, database, segment) : default_scale;

					out_local_pose[current_transform_index] = transform_set(rotation, translation, scale);
					current_transform_index = database.get_parent_index(current_transform_index);
				}
			}
			else
			{
				context.sample_key = 0;

				uint32_t current_transform_index = target_transform_index;
				while (current_transform_index != k_invalid_bone_index)
				{
					context.track_index = current_transform_index;

					const Quat_32 rotation = sample_rotation<SampleDistribution8::Variable>(context, database, segment);
					const Vector4_32 translation = sample_translation<SampleDistribution8::Variable>(context, database, segment);
					const Vector4_32 scale = has_scale ? sample_scale<SampleDistribution8::Variable>(context, database, segment) : default_scale;

					out_local_pose[current_transform_index] = transform_set(rotation, translation, scale);
					current_transform_index = database.get_parent_index(current_transform_index);
				}
			}
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;
				context.bit_rates = bit_rates[bone_index];

				const BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
		else
		{
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				context.track_index = bone_index;
				context.bit_rates = bit_rates[bone_index];

				const BoneStreams& bone_stream = bone_streams[bone_index];
				const BoneStreams& raw_bone_steam = raw_bone_steams[bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_steam, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[bone_index] = transform_set(rotation, translation, scale);
			}
		}
	}

	inline void sample_stream(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.track_index = bone_index;
		context.sample_key = sample_key;
		context.sample_time = sample_time;
		context.bit_rates = bit_rates[bone_index];

		const BoneStreams& bone_stream = bone_streams[bone_index];
		const BoneStreams& raw_bone_stream = raw_bone_steams[bone_index];

		Quat_32 rotation;
		Vector4_32 translation;
		Vector4_32 scale;
		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
			translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;
		}
		else
		{
			rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
			translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
			scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;
		}

		out_local_pose[bone_index] = transform_set(rotation, translation, scale);
	}

	inline void sample_streams_hierarchical(const BoneStreams* bone_streams, const BoneStreams* raw_bone_steams, uint16_t num_bones, float sample_time, uint16_t bone_index, const BoneBitRate* bit_rates, RotationFormat8 rotation_format, VectorFormat8 translation_format, VectorFormat8 scale_format, Transform_32* out_local_pose)
	{
		(void)num_bones;

		const bool is_rotation_variable = is_rotation_format_variable(rotation_format);
		const bool is_translation_variable = is_vector_format_variable(translation_format);
		const bool is_scale_variable = is_vector_format_variable(scale_format);

		const SegmentContext* segment_context = bone_streams->segment;
		const Vector4_32 default_scale = get_default_scale(segment_context->clip->additive_format);
		const bool has_scale = segment_context->clip->has_scale;

		uint32_t sample_key;
		if (segment_context->distribution == SampleDistribution8::Uniform)
			sample_key = acl_impl::get_uniform_sample_key(*segment_context, sample_time);
		else
			sample_key = 0;

		acl_impl::sample_context context;
		context.sample_key = sample_key;
		context.sample_time = sample_time;

		if (segment_context->distribution == SampleDistribution8::Uniform)
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;
				context.bit_rates = bit_rates[current_bone_index];

				const BoneStreams& bone_stream = bone_streams[current_bone_index];
				const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Uniform>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
		else
		{
			uint16_t current_bone_index = bone_index;
			while (current_bone_index != k_invalid_bone_index)
			{
				context.track_index = current_bone_index;
				context.bit_rates = bit_rates[current_bone_index];

				const BoneStreams& bone_stream = bone_streams[current_bone_index];
				const BoneStreams& raw_bone_stream = raw_bone_steams[current_bone_index];

				const Quat_32 rotation = acl_impl::sample_rotation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_rotation_variable, rotation_format);
				const Vector4_32 translation = acl_impl::sample_translation<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_translation_variable, translation_format);
				const Vector4_32 scale = has_scale ? acl_impl::sample_scale<SampleDistribution8::Variable>(context, bone_stream, raw_bone_stream, is_scale_variable, scale_format, default_scale) : default_scale;

				out_local_pose[current_bone_index] = transform_set(rotation, translation, scale);
				current_bone_index = bone_stream.parent_bone_index;
			}
		}
	}

	inline void sample_streams(const BoneStreams* bone_streams, uint16_t num_bones, uint32_t sample_index, Transform_32* out_local_pose)
	{
		for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
		{
			const BoneStreams& bone_stream = bone_streams[bone_index];

			const uint32_t rotation_sample_index = !bone_stream.is_rotation_constant ? sample_index : 0;
			const Quat_32 rotation = get_rotation_sample(bone_stream, rotation_sample_index);

			const uint32_t translation_sample_index = !bone_stream.is_translation_constant ? sample_index : 0;
			const Vector4_32 translation = get_translation_sample(bone_stream, translation_sample_index);

			const uint32_t scale_sample_index = !bone_stream.is_scale_constant ? sample_index : 0;
			const Vector4_32 scale = get_scale_sample(bone_stream, scale_sample_index);

			out_local_pose[bone_index] = transform_set(rotation, translation, scale);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
