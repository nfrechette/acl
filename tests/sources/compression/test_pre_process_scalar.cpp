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

#include <acl/compression/pre_process.h>
#include <acl/compression/track_array.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/track_extended_sample.h>

#include <rtm/vector4f.h>

#include <cstdint>
#include <cstring>
#include <utility>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_tracks = 3;
	constexpr uint32_t k_num_samples = 4;
	constexpr float k_sample_rate = 30.0F;
	constexpr float k_precision = 0.01F;

	// Track 0 is animated. Its last sample is near its first sample.
	// Track 1 is constant within the precision.
	// Track 2 is animated. The test sets its last sample.
	const float k_sample_values[k_num_tracks][k_num_samples] =
	{
		{ 1.0F, 5.0F, 2.0F, 1.005F },
		{ 3.0F, 3.004F, 2.998F, 3.002F },
		{ -1.0F, 2.0F, 7.0F, 0.0F },
	};

	// The interpolators of the extended tracks. A linear track ignores them.
	const sample_interpolator_t k_sample_interpolators[k_num_tracks][k_num_samples] =
	{
		{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::linear },
		{ sample_interpolator_t::linear, sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::constant },
		{ sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::constant, sample_interpolator_t::constant },
	};

	// With float4f, component N has the value (value + N * 10.0)
	inline void set_sample(float& sample, sample_interpolator_t /*interpolator*/, float value) { sample = value; }
	inline void set_sample(rtm::float4f& sample, sample_interpolator_t /*interpolator*/, float value) { sample = { value, value + 10.0F, value + 20.0F, value + 30.0F }; }

	template<typename sample_type_t>
	inline void set_sample(track_extended_sample_t<sample_type_t>& sample, sample_interpolator_t interpolator, float value)
	{
		sample.interpolator = interpolator;
		set_sample(sample.value, interpolator, value);
	}

	// Returns true if the value is binary equal to the value that 'set_sample' writes.
	// The expected value goes through memory. On x86 with x87 math (for example GCC with -m32),
	// an expression such as (value + 10.0) keeps more precision than a stored float.
	template<typename value_type_t>
	inline bool is_value_equal(const value_type_t& value, float expected_value)
	{
		value_type_t expected = {};
		set_sample(expected, sample_interpolator_t::linear, expected_value);
		return std::memcmp(&value, &expected, sizeof(value_type_t)) == 0;
	}

	// Returns true if the sample has the expected value and the expected interpolator.
	// A linear sample has no interpolator.
	inline bool is_sample_equal(const float& sample, sample_interpolator_t /*interpolator*/, float value) { return is_value_equal(sample, value); }
	inline bool is_sample_equal(const rtm::float4f& sample, sample_interpolator_t /*interpolator*/, float value) { return is_value_equal(sample, value); }

	// The extended sample has padding, thus we compare each member and not the whole sample
	template<typename sample_type_t>
	inline bool is_sample_equal(const track_extended_sample_t<sample_type_t>& sample, sample_interpolator_t interpolator, float value)
	{
		return sample.interpolator == interpolator && is_value_equal(sample.value, value);
	}

	template<class track_array_type>
	void test_pre_process_scalar(pre_process_precision_policy precision_policy, float track2_last_value, const float (&expected_values)[k_num_tracks][k_num_samples])
	{
		using track_type = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		track_array_type raw_tracks(allocator, k_num_tracks);
		for (uint32_t track_index = 0; track_index < k_num_tracks; ++track_index)
		{
			track_desc_scalarf desc;
			desc.output_index = track_index;
			desc.precision = k_precision;

			track_type track_ = track_type::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const bool is_track2_last_sample = track_index == 2 && sample_index == (k_num_samples - 1);
				const float value = is_track2_last_sample ? track2_last_value : k_sample_values[track_index][sample_index];
				set_sample(track_[sample_index], k_sample_interpolators[track_index][sample_index], value);
			}

			raw_tracks[track_index] = std::move(track_);
		}

		pre_process_settings_t settings;
		settings.actions = pre_process_actions::recommended;
		settings.precision_policy = precision_policy;

		const error_result result = pre_process_track_list(allocator, settings, raw_tracks);
		REQUIRE(result.empty());

		for (uint32_t track_index = 0; track_index < k_num_tracks; ++track_index)
		{
			const track_type& track_ = raw_tracks[track_index];
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				INFO("track index: " << track_index << ", sample index: " << sample_index);

				// Pre-process changes only the values, never the interpolators
				CHECK(is_sample_equal(track_[sample_index], k_sample_interpolators[track_index][sample_index], expected_values[track_index][sample_index]));
			}
		}
	}

	template<class track_array_type>
	void test_pre_process_scalar()
	{
		{
			INFO("lossless");

			// A lossless pre-process does not change the samples
			const float expected_values[k_num_tracks][k_num_samples] =
			{
				{ 1.0F, 5.0F, 2.0F, 1.005F },
				{ 3.0F, 3.004F, 2.998F, 3.002F },
				{ -1.0F, 2.0F, 7.0F, -1.008F },
			};

			test_pre_process_scalar<track_array_type>(pre_process_precision_policy::lossless, -1.008F, expected_values);
		}

		{
			INFO("lossy, looping");

			// The last sample of each track gets the value of the first sample.
			// Track 1 is constant, each sample gets the value of the first sample.
			const float expected_values[k_num_tracks][k_num_samples] =
			{
				{ 1.0F, 5.0F, 2.0F, 1.0F },
				{ 3.0F, 3.0F, 3.0F, 3.0F },
				{ -1.0F, 2.0F, 7.0F, -1.0F },
			};

			test_pre_process_scalar<track_array_type>(pre_process_precision_policy::lossy, -1.008F, expected_values);
		}

		{
			INFO("lossy, not looping");

			// Track 2 does not loop, thus no last sample changes.
			// Track 1 is constant, each sample gets the value of the first sample.
			const float expected_values[k_num_tracks][k_num_samples] =
			{
				{ 1.0F, 5.0F, 2.0F, 1.005F },
				{ 3.0F, 3.0F, 3.0F, 3.0F },
				{ -1.0F, 2.0F, 7.0F, -1.5F },
			};

			test_pre_process_scalar<track_array_type>(pre_process_precision_policy::lossy, -1.5F, expected_values);
		}
	}
}

TEST_CASE("pre-process scalar float1f", "[compression][pre_process]")
{
	test_pre_process_scalar<track_array_float1f>();
}

TEST_CASE("pre-process scalar float4f", "[compression][pre_process]")
{
	test_pre_process_scalar<track_array_float4f>();
}

TEST_CASE("pre-process scalar float1f extended samples", "[compression][pre_process][extended_samples]")
{
	test_pre_process_scalar<track_array_float1f_ex>();
}

TEST_CASE("pre-process scalar float4f extended samples", "[compression][pre_process][extended_samples]")
{
	test_pre_process_scalar<track_array_float4f_ex>();
}
