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
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/compression/stream/clip_context.h"
#include "acl/compression/impl/track_database.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	inline Vector4_32 ACL_SIMD_CALL convert_rotation(Vector4_32Arg0 rotation, RotationFormat8 from, RotationFormat8 to)
	{
		ACL_ASSERT(from == RotationFormat8::Quat_128, "Source rotation format must be a full precision quaternion");
		(void)from;

		const RotationFormat8 high_precision_format = get_rotation_variant(to) == RotationVariant8::Quat ? RotationFormat8::Quat_128 : RotationFormat8::QuatDropW_96;
		switch (high_precision_format)
		{
		case RotationFormat8::Quat_128:
			// Original format, nothing to do
			return rotation;
		case RotationFormat8::QuatDropW_96:
			// Drop W, we just ensure it is positive and write it back, the W component can be ignored afterwards
			return quat_to_vector(quat_ensure_positive_w(vector_to_quat(rotation)));
		default:
			ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(to));
			return rotation;
		}
	}

	inline void convert_rotation_streams(IAllocator& allocator, SegmentContext& segment, RotationFormat8 rotation_format)
	{
		const RotationFormat8 high_precision_format = get_rotation_variant(rotation_format) == RotationVariant8::Quat ? RotationFormat8::Quat_128 : RotationFormat8::QuatDropW_96;

		for (BoneStreams& bone_stream : segment.bone_iterator())
		{
			// We convert our rotation stream in place. We assume that the original format is Quat_128 stored as Quat_32
			// For all other formats, we keep the same sample size and either keep Quat_32 or use Vector4_32
			ACL_ASSERT(bone_stream.rotations.get_sample_size() == sizeof(Quat_32), "Unexpected rotation sample size. %u != %u", bone_stream.rotations.get_sample_size(), sizeof(Quat_32));

			const uint32_t num_samples = bone_stream.rotations.get_num_samples();
			const float sample_rate = bone_stream.rotations.get_sample_rate();
			RotationTrackStream converted_stream(allocator, num_samples, sizeof(Quat_32), sample_rate, high_precision_format);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				Quat_32 rotation = bone_stream.rotations.get_raw_sample<Quat_32>(sample_index);

				switch (high_precision_format)
				{
				case RotationFormat8::Quat_128:
					// Original format, nothing to do
					break;
				case RotationFormat8::QuatDropW_96:
					// Drop W, we just ensure it is positive and write it back, the W component can be ignored afterwards
					rotation = quat_ensure_positive_w(rotation);
					break;
				default:
					ACL_ASSERT(false, "Invalid or unsupported rotation format: %s", get_rotation_format_name(high_precision_format));
					break;
				}

				converted_stream.set_raw_sample(sample_index, rotation);
			}

			bone_stream.rotations = std::move(converted_stream);
		}
	}

	inline void convert_rotation_streams(IAllocator& allocator, ClipContext& clip_context, RotationFormat8 rotation_format)
	{
		for (SegmentContext& segment : clip_context.segment_iterator())
			convert_rotation_streams(allocator, segment, rotation_format);
	}

	namespace acl_impl
	{
		inline void quat_ensure_positive_w_soa(Vector4_32& rotations_x, Vector4_32& rotations_y, Vector4_32& rotations_z, Vector4_32& rotations_w)
		{
			// result =  quat_get_w(input) >= 0.f ? input : quat_neg(input);

			// Avoid aliasing
			const Vector4_32 xxxx = rotations_x;
			const Vector4_32 yyyy = rotations_y;
			const Vector4_32 zzzz = rotations_z;
			const Vector4_32 wwww = rotations_w;

			const Vector4_32 w_positive_mask = vector_greater_equal(wwww, vector_zero_32());
			rotations_x = vector_blend(w_positive_mask, xxxx, vector_neg(xxxx));
			rotations_y = vector_blend(w_positive_mask, yyyy, vector_neg(yyyy));
			rotations_z = vector_blend(w_positive_mask, zzzz, vector_neg(zzzz));
			rotations_w = vector_blend(w_positive_mask, wwww, vector_neg(wwww));
		}

		inline void convert_drop_w_track(Vector4_32* inputs_x, Vector4_32* inputs_y, Vector4_32* inputs_z, Vector4_32* inputs_w, uint32_t num_soa_entries)
		{
			// Process two entries at a time to allow the compiler to re-order things to hide instruction latency
			// TODO: Trivial AVX or ISPC conversion
			uint32_t entry_index;
			for (entry_index = 0; entry_index < (num_soa_entries & 0xFFFFFFFEU); ++entry_index)
			{
				// Drop W, we just ensure it is positive and write it back, the W component can be ignored and trivially reconstructed afterwards
				quat_ensure_positive_w_soa(inputs_x[entry_index], inputs_y[entry_index], inputs_z[entry_index], inputs_w[entry_index]);

				entry_index++;
				quat_ensure_positive_w_soa(inputs_x[entry_index], inputs_y[entry_index], inputs_z[entry_index], inputs_w[entry_index]);
			}

			if (entry_index < num_soa_entries)
				quat_ensure_positive_w_soa(inputs_x[entry_index], inputs_y[entry_index], inputs_z[entry_index], inputs_w[entry_index]);
		}

		inline void convert_rotations(track_database& database, const segment_context& segment, RotationFormat8 rotation_format)
		{
			// We convert our rotations in place. We assume that the original format is RotationFormat8::Quat_128 stored as Quat_32
			const RotationVariant8 rotation_variant = get_rotation_variant(rotation_format);
			if (rotation_variant == RotationVariant8::Quat)
				return;	// Nothing to do

			ACL_ASSERT(rotation_variant == RotationVariant8::QuatDropW, "Unexpected variant");

			const uint32_t num_transforms = database.get_num_transforms();
			const uint32_t num_soa_entries = segment.num_soa_entries;
			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				Vector4_32* rotations_x;
				Vector4_32* rotations_y;
				Vector4_32* rotations_z;
				Vector4_32* rotations_w;
				database.get_rotations(segment, transform_index, rotations_x, rotations_y, rotations_z, rotations_w);

				convert_drop_w_track(rotations_x, rotations_y, rotations_z, rotations_w, num_soa_entries);
			}

			const RotationFormat8 highest_bit_rate = get_highest_variant_precision(rotation_variant);
			database.set_rotation_format(highest_bit_rate);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
