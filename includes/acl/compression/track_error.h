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
#include <type_traits>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// SFINAE boilerplate to detect if a template argument derives from acl::uniformly_sampled::DecompressionContext.
		//////////////////////////////////////////////////////////////////////////
		template<class T>
		using is_decompression_context = typename std::enable_if<std::is_base_of<acl::decompression_context<typename T::settings_type>, T>::value, std::nullptr_t>::type;
	}

	//////////////////////////////////////////////////////////////////////////
	// A struct that contains the track index that has the worst error,
	// its error, and the sample time at which it happens.
	//////////////////////////////////////////////////////////////////////////
	struct track_error
	{
		track_error() : index(k_invalid_track_index), error(0.0F), sample_time(0.0F) {}

		//////////////////////////////////////////////////////////////////////////
		// The track index with the worst error.
		uint32_t index;

		//////////////////////////////////////////////////////////////////////////
		// The worst error for the track index.
		float error;

		//////////////////////////////////////////////////////////////////////////
		// The sample time that has the worst error.
		float sample_time;
	};

	//////////////////////////////////////////////////////////////////////////
	// Calculates the worst compression error between a raw track array and its
	// compressed tracks.
	//
	// Note: This function uses SFINAE to prevent it from matching when it shouldn't.
	template<class decompression_context_type, acl_impl::is_decompression_context<decompression_context_type> = nullptr>
	inline track_error calculate_compression_error(IAllocator& allocator, const track_array& raw_tracks, decompression_context_type& context)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(context.is_initialized(), "Context isn't initialized");

		const uint32_t num_samples = raw_tracks.get_num_samples_per_track();
		if (num_samples == 0)
			return track_error();	// Cannot measure any error

		const uint32_t num_tracks = raw_tracks.get_num_tracks();
		if (num_tracks == 0)
			return track_error();	// Cannot measure any error

		track_error result;
		result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

		const float duration = raw_tracks.get_duration();
		const float sample_rate = raw_tracks.get_sample_rate();
		const track_type8 track_type = raw_tracks.get_track_type();

		debug_track_writer raw_tracks_writer(allocator, track_type, num_tracks);
		debug_track_writer lossy_tracks_writer(allocator, track_type, num_tracks);

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

				rtm::vector4f error;

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
					error = zero;
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

	//////////////////////////////////////////////////////////////////////////
	// Calculates the worst compression error between two compressed tracks instances.
	//
	// Note: This function uses SFINAE to prevent it from matching when it shouldn't.
	template<class decompression_context_type0, class decompression_context_type1, acl_impl::is_decompression_context<decompression_context_type0> = nullptr, acl_impl::is_decompression_context<decompression_context_type1> = nullptr>
	inline track_error calculate_compression_error(IAllocator& allocator, decompression_context_type0& context0, decompression_context_type1& context1)
	{
		using namespace acl_impl;

		ACL_ASSERT(context0.is_initialized(), "Context isn't initialized");
		ACL_ASSERT(context1.is_initialized(), "Context isn't initialized");

		const compressed_tracks* tracks0 = context0.get_compressed_tracks();

		const uint32_t num_samples = tracks0->get_num_samples_per_track();
		if (num_samples == 0)
			return track_error();	// Cannot measure any error

		const uint32_t num_tracks = tracks0->get_num_tracks();
		if (num_tracks == 0)
			return track_error();	// Cannot measure any error

		track_error result;
		result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

		const float duration = tracks0->get_duration();
		const float sample_rate = tracks0->get_sample_rate();
		const track_type8 track_type = tracks0->get_track_type();

		debug_track_writer tracks_writer0(allocator, track_type, num_tracks);
		debug_track_writer tracks_writer1(allocator, track_type, num_tracks);

		const rtm::vector4f zero = rtm::vector_zero();

		// Measure our error
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

			// We use the nearest sample to accurately measure the loss that happened, if any
			context0.seek(sample_time, sample_rounding_policy::nearest);
			context0.decompress_tracks(tracks_writer0);

			context1.seek(sample_time, sample_rounding_policy::nearest);
			context1.decompress_tracks(tracks_writer1);

			// Validate decompress_tracks
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				rtm::vector4f error;

				switch (track_type)
				{
				case track_type8::float1f:
				{
					const float raw_value = tracks_writer0.read_float1(track_index);
					const float lossy_value = tracks_writer1.read_float1(track_index);
					error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
					break;
				}
				case track_type8::float2f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float2(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float2(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float3f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float3(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float3(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float4f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float4(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float4(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				case track_type8::vector4f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_vector4(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_vector4(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				default:
					ACL_ASSERT(false, "Unsupported track type");
					error = zero;
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

	//////////////////////////////////////////////////////////////////////////
	// Calculates the worst compression error between two raw track arrays.
	inline track_error calculate_compression_error(IAllocator& allocator, const track_array& raw_tracks0, const track_array& raw_tracks1)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks0.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(raw_tracks1.is_valid().empty(), "Raw tracks are invalid");

		const uint32_t num_samples = raw_tracks0.get_num_samples_per_track();
		if (num_samples == 0)
			return track_error();	// Cannot measure any error

		const uint32_t num_tracks = raw_tracks0.get_num_tracks();
		if (num_tracks == 0 || num_tracks != raw_tracks1.get_num_tracks())
			return track_error();	// Cannot measure any error

		const track_type8 track_type = raw_tracks0.get_track_type();
		if (track_type != raw_tracks1.get_track_type())
			return track_error();	// Cannot measure any error

		track_error result;
		result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

		const float duration = raw_tracks0.get_duration();
		const float sample_rate = raw_tracks0.get_sample_rate();

		debug_track_writer tracks_writer0(allocator, track_type, num_tracks);
		debug_track_writer tracks_writer1(allocator, track_type, num_tracks);

		const rtm::vector4f zero = rtm::vector_zero();

		// Measure our error
		for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
		{
			const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

			// We use the nearest sample to accurately measure the loss that happened, if any
			raw_tracks0.sample_tracks(sample_time, sample_rounding_policy::nearest, tracks_writer0);
			raw_tracks1.sample_tracks(sample_time, sample_rounding_policy::nearest, tracks_writer1);

			// Validate decompress_tracks
			for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
			{
				rtm::vector4f error;

				switch (track_type)
				{
				case track_type8::float1f:
				{
					const float raw_value = tracks_writer0.read_float1(track_index);
					const float lossy_value = tracks_writer1.read_float1(track_index);
					error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
					break;
				}
				case track_type8::float2f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float2(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float2(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float3f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float3(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float3(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, zero);
					break;
				}
				case track_type8::float4f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_float4(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_float4(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				case track_type8::vector4f:
				{
					const rtm::vector4f raw_value = tracks_writer0.read_vector4(track_index);
					const rtm::vector4f lossy_value = tracks_writer1.read_vector4(track_index);
					error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
					break;
				}
				default:
					ACL_ASSERT(false, "Unsupported track type");
					error = zero;
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
