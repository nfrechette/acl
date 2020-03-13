#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/impl/compiler_utils.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/error.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/debug_track_writer.h"
#include "acl/compression/track_array.h"
#include "acl/decompression/decompress.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	//////////////////////////////////////////////////////////////////////////
	// A struct that contains the raw track index that has the worst error,
	// its error, and the sample time at which it happens.
	//////////////////////////////////////////////////////////////////////////
	struct track_error
	{
		track_error() : index(k_invalid_track_index), error(0.0F), sample_time(0.0F) {}

		//////////////////////////////////////////////////////////////////////////
		// The raw track index with the worst error.
		uint32_t index;

		//////////////////////////////////////////////////////////////////////////
		// The worst error for the raw track index.
		float error;

		//////////////////////////////////////////////////////////////////////////
		// The sample time that has the worst error.
		float sample_time;
	};

	//////////////////////////////////////////////////////////////////////////
	// Calculates the worst compression error between a raw track array and its
	// compressed tracks.
	inline track_error calculate_compression_error(IAllocator& allocator, const track_array& raw_tracks, const compressed_tracks& tracks)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(tracks.is_valid(false).empty(), "Compressed tracks are invalid");

		const uint32_t num_samples = tracks.get_num_samples_per_track();
		if (num_samples == 0)
			return track_error();	// Cannot measure any error

		const uint32_t num_tracks = tracks.get_num_tracks();
		if (num_tracks == 0)
			return track_error();	// Cannot measure any error

		track_error result;
		result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

		const float duration = tracks.get_duration();
		const float sample_rate = tracks.get_sample_rate();
		const track_type8 track_type = raw_tracks.get_track_type();

		decompression_context<debug_decompression_settings> context;
		context.initialize(tracks);

		debug_track_writer raw_tracks_writer(allocator, track_type, num_tracks);
		debug_track_writer raw_track_writer(allocator, track_type, num_tracks);
		debug_track_writer lossy_tracks_writer(allocator, track_type, num_tracks);
		debug_track_writer lossy_track_writer(allocator, track_type, num_tracks);

		const rtm::vector4f zero = rtm::vector_zero();

		// Measure our error
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

			// We use the nearest sample to accurately measure the loss that happened, if any
			raw_tracks.sample_tracks(sample_time, sample_rounding_policy::nearest, raw_tracks_writer);

			context.seek(sample_time, sample_rounding_policy::nearest);
			context.decompress_tracks(lossy_tracks_writer);

			// Validate decompress_tracks
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				const track& track_ = raw_tracks[track_index];
				const uint32_t output_index = track_.get_output_index();
				if (output_index == k_invalid_track_index)
					continue;	// Track is being stripped, ignore it

				rtm::vector4f error = zero;

				switch (track_type)
				{
				case track_type8::float1f:
				{
					const float raw_value = raw_tracks_writer.read_float1(track_index);
					const float lossy_value = lossy_tracks_writer.read_float1(output_index);
					error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
					break;
				}
				case track_type8::float2f:
				{
					const rtm::vector4f raw_value = raw_tracks_writer.read_float2(track_index);
					const rtm::vector4f lossy_value = lossy_tracks_writer.read_float2(output_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float3f:
				{
					const rtm::vector4f raw_value = raw_tracks_writer.read_float3(track_index);
					const rtm::vector4f lossy_value = lossy_tracks_writer.read_float3(output_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float4f:
				{
					const rtm::vector4f raw_value = raw_tracks_writer.read_float4(track_index);
					const rtm::vector4f lossy_value = lossy_tracks_writer.read_float4(output_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				case track_type8::vector4f:
				{
					const rtm::vector4f raw_value = raw_tracks_writer.read_vector4(track_index);
					const rtm::vector4f lossy_value = lossy_tracks_writer.read_vector4(output_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				default:
					ACL_ASSERT(false, "Unsupported track type");
					break;
				}

				const float max_error = rtm::vector_get_max_component(error);
				if (max_error > result.error)
				{
					result.error = max_error;
					result.index = track_index;
					result.sample_time = sample_time;
				}
			}
		}

		return result;
	}
}

ACL_IMPL_FILE_PRAGMA_POP
