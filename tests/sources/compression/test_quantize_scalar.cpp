////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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


#include "catch2.impl.h"

#include <acl/compression/compress.h>
#include <acl/compression/track_array.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/impl/debug_track_writer.h>
#include <acl/decompression/decompress.h>

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <utility>

using namespace acl;
using namespace acl::acl_impl;

namespace
{
	constexpr uint32_t k_num_samples = 4;
	constexpr float k_sample_rate = 4.0F;
	constexpr float k_precision = 0.0001F;
	const float k_sample_values[k_num_samples] = { 1.0F, 5.0F, 2.0F, 8.0F };

	// Component N of sample I has the value (k_sample_values[I] * (N + 1))
	inline rtm::vector4f get_sample_value(uint32_t sample_index)
	{
		const float value = k_sample_values[sample_index];
		return rtm::vector_set(value, value * 2.0F, value * 3.0F, value * 4.0F);
	}

	inline void set_sample(float& sample, rtm::vector4f_arg0 value) { rtm::vector_store1(value, &sample); }
	inline void set_sample(rtm::float2f& sample, rtm::vector4f_arg0 value) { rtm::vector_store2(value, &sample); }
	inline void set_sample(rtm::float3f& sample, rtm::vector4f_arg0 value) { rtm::vector_store3(value, &sample); }
	inline void set_sample(rtm::float4f& sample, rtm::vector4f_arg0 value) { rtm::vector_store(value, &sample); }
	inline void set_sample(rtm::vector4f& sample, rtm::vector4f_arg0 value) { sample = value; }

	inline rtm::vector4f read_value(const debug_track_writer& writer, track_type8 track_type)
	{
		switch (track_type)
		{
		case track_type8::float1f:	return rtm::vector_set(writer.read_float1(0));
		case track_type8::float2f:	return writer.read_float2(0);
		case track_type8::float3f:	return writer.read_float3(0);
		case track_type8::float4f:	return writer.read_float4(0);
		default:					return writer.read_vector4(0);
		}
	}

	template<class track_array_type>
	void test_quantize_scalar_track()
	{
		using typed_track = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		track_desc_scalarf desc;
		desc.output_index = 0;
		desc.precision = k_precision;

		typed_track track_ = typed_track::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			set_sample(track_[sample_index], get_sample_value(sample_index));

		track_array_type raw_tracks(allocator, 1);
		raw_tracks[0] = std::move(track_);

		const track_type8 track_type = raw_tracks.get_track_type();
		const uint32_t num_components = get_track_num_sample_elements(track_type);

		// The default settings use the variable format, the raw settings use the full precision format
		const compression_settings settings_list[] = { get_default_compression_settings(), get_raw_compression_settings() };
		for (const compression_settings& settings : settings_list)
		{
			const bool is_raw = settings.scalar_format == scalar_format8::scalarf_full;
			INFO("scalar format: " << get_scalar_format_name(settings.scalar_format));

			output_stats stats;
			compressed_tracks* tracks = nullptr;
			const error_result result = compress_track_list(allocator, raw_tracks, settings, tracks, stats);
			REQUIRE(result.empty());
			REQUIRE(tracks != nullptr);

			// The precision applies to each component, a variable format must use a variable bit rate
			const track_metadata* per_track_metadata = get_scalar_tracks_header(*tracks).get_track_metadata();
			CHECK(per_track_metadata[0].get_bit_rate() != 0);
			CHECK(is_raw_bit_rate(per_track_metadata[0].get_bit_rate()) == is_raw);

			decompression_context<debug_scalar_decompression_settings> context;
			REQUIRE(context.initialize(*tracks));

			debug_track_writer writer(allocator, track_type, 1);

			// The nearest rounding policy returns each sample without interpolation
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				INFO("sample index: " << sample_index);

				const float sample_time = float(sample_index) / k_sample_rate;
				context.seek(sample_time, sample_rounding_policy::nearest);
				context.decompress_tracks(writer);

				float expected[4];
				float value[4];
				rtm::vector_store(get_sample_value(sample_index), &expected[0]);
				rtm::vector_store(read_value(writer, track_type), &value[0]);

				// The full precision format must return each value without loss
				for (uint32_t component_index = 0; component_index < num_components; ++component_index)
				{
					if (is_raw)
						CHECK(value[component_index] == expected[component_index]);
					else
						CHECK(rtm::scalar_near_equal(value[component_index], expected[component_index], k_precision));
				}
			}

			allocator.deallocate(tracks, tracks->get_size());
		}
	}
}

TEST_CASE("quantize scalar float1f", "[compression][quantize]")
{
	test_quantize_scalar_track<track_array_float1f>();
}

TEST_CASE("quantize scalar float2f", "[compression][quantize]")
{
	test_quantize_scalar_track<track_array_float2f>();
}

TEST_CASE("quantize scalar float3f", "[compression][quantize]")
{
	test_quantize_scalar_track<track_array_float3f>();
}

TEST_CASE("quantize scalar float4f", "[compression][quantize]")
{
	test_quantize_scalar_track<track_array_float4f>();
}

TEST_CASE("quantize scalar vector4f", "[compression][quantize]")
{
	test_quantize_scalar_track<track_array_vector4f>();
}
