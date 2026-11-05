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
#include <acl/compression/convert.h>
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
	// The sample rate is 4.0, the samples are at 0.0, 0.25, 0.5, and 0.75.
	// Sample 0 and 1 are constant, sample 2 and 3 are linear.
	constexpr uint32_t k_num_samples = 4;
	constexpr float k_sample_rate = 4.0F;
	const float k_sample_values[k_num_samples] = { 1.0F, 5.0F, 2.0F, 8.0F };

	constexpr float k_value_threshold = 0.001F;

	struct sample_test_case
	{
		float sample_time;
		sample_rounding_policy rounding_policy;
		float expected_value;
	};

	// The sample times are not on a sample. At a sample time, the ceil policy selects the next sample.
	const sample_test_case k_test_cases[] =
	{
		// Between sample 0 (constant) and 1, alpha = 0.4
		{ 0.1F, sample_rounding_policy::none, 1.0F },
		{ 0.1F, sample_rounding_policy::floor, 1.0F },
		{ 0.1F, sample_rounding_policy::ceil, 5.0F },
		{ 0.1F, sample_rounding_policy::nearest, 1.0F },

		// Between sample 0 (constant) and 1, alpha = 0.8
		{ 0.2F, sample_rounding_policy::none, 1.0F },
		{ 0.2F, sample_rounding_policy::floor, 1.0F },
		{ 0.2F, sample_rounding_policy::ceil, 5.0F },
		{ 0.2F, sample_rounding_policy::nearest, 5.0F },

		// Between sample 1 (constant) and 2, alpha = 0.6
		{ 0.4F, sample_rounding_policy::none, 5.0F },
		{ 0.4F, sample_rounding_policy::floor, 5.0F },
		{ 0.4F, sample_rounding_policy::ceil, 2.0F },
		{ 0.4F, sample_rounding_policy::nearest, 2.0F },

		// Between sample 2 (linear) and 3, alpha = 0.4
		{ 0.6F, sample_rounding_policy::none, 4.4F },
		{ 0.6F, sample_rounding_policy::floor, 2.0F },
		{ 0.6F, sample_rounding_policy::ceil, 8.0F },
		{ 0.6F, sample_rounding_policy::nearest, 2.0F },
	};

	inline void set_sample(track_extended_sample_t<float>& sample, sample_interpolator_t interpolator, float value)
	{
		sample.interpolator = interpolator;
		sample.value = value;
	}

	inline void set_sample(track_extended_sample_t<rtm::float4f>& sample, sample_interpolator_t interpolator, float value)
	{
		sample.interpolator = interpolator;
		sample.value = { value, value * 2.0F, value * 3.0F, value * 4.0F };
	}

	// Returns true if the track value is near the expected value.
	// With float4f, component N must be near (expected_value * (N + 1)).
	inline bool is_value_near(const debug_track_writer& writer, track_type8 track_type, uint32_t track_index, float expected_value)
	{
		if (track_type == track_type8::float1f)
			return rtm::scalar_near_equal(writer.read_float1(track_index), expected_value, k_value_threshold);

		const rtm::vector4f expected = rtm::vector_set(expected_value, expected_value * 2.0F, expected_value * 3.0F, expected_value * 4.0F);
		return rtm::vector_all_near_equal(writer.read_float4(track_index), expected, k_value_threshold);
	}

	// Sets the rounding policy of a writer.
	// A writer without support for a per track rounding policy uses the policy of the seek.
	inline void set_rounding_policy(debug_track_writer& /*writer*/, sample_rounding_policy /*rounding_policy*/) {}
	inline void set_rounding_policy(debug_track_writer_per_track_rounding& writer, sample_rounding_policy rounding_policy) { writer.rounding_policy = rounding_policy; }

	// Track 0 is animated, track 1 is constant
	constexpr uint32_t k_num_tracks = 2;
	constexpr float k_constant_track_value = 3.0F;

	template<class decompression_settings_type, class track_writer_type>
	void validate_extended_samples(iallocator& allocator, const track_array& raw_tracks, const compressed_tracks& tracks, bool use_per_track_rounding)
	{
		const track_type8 track_type = raw_tracks.get_track_type();

		decompression_context<decompression_settings_type> context;
		REQUIRE(context.initialize(tracks));

		track_writer_type raw_tracks_writer(allocator, track_type, k_num_tracks);
		track_writer_type raw_track_writer(allocator, track_type, k_num_tracks);
		track_writer_type lossy_tracks_writer(allocator, track_type, k_num_tracks);
		track_writer_type lossy_track_writer(allocator, track_type, k_num_tracks);

		for (const sample_test_case& test_case : k_test_cases)
		{
			INFO("sample time: " << test_case.sample_time << ", rounding policy: " << static_cast<int>(test_case.rounding_policy));

			// With a per track rounding policy, the seek uses 'per_track' and the writer sets the policy
			const sample_rounding_policy seek_rounding_policy = use_per_track_rounding ? sample_rounding_policy::per_track : test_case.rounding_policy;

			set_rounding_policy(raw_tracks_writer, test_case.rounding_policy);
			set_rounding_policy(raw_track_writer, test_case.rounding_policy);
			set_rounding_policy(lossy_tracks_writer, test_case.rounding_policy);
			set_rounding_policy(lossy_track_writer, test_case.rounding_policy);

			raw_tracks.sample_tracks(test_case.sample_time, seek_rounding_policy, raw_tracks_writer);

			context.seek(test_case.sample_time, seek_rounding_policy);
			context.decompress_tracks(lossy_tracks_writer);

			for (uint32_t track_index = 0; track_index < k_num_tracks; ++track_index)
			{
				INFO("track index: " << track_index);

				raw_tracks.sample_track(track_index, test_case.sample_time, seek_rounding_policy, raw_track_writer);
				context.decompress_track(track_index, lossy_track_writer);

				const float expected_value = track_index == 0 ? test_case.expected_value : k_constant_track_value;

				CHECK(is_value_near(raw_tracks_writer, track_type, track_index, expected_value));
				CHECK(is_value_near(raw_track_writer, track_type, track_index, expected_value));
				CHECK(is_value_near(lossy_tracks_writer, track_type, track_index, expected_value));
				CHECK(is_value_near(lossy_track_writer, track_type, track_index, expected_value));
			}
		}
	}

	template<class track_array_type>
	void test_extended_samples()
	{
		using track_type = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		track_desc_scalarf desc0;
		desc0.output_index = 0;
		desc0.precision = 0.0001F;

		track_type track0 = track_type::make_reserve(desc0, allocator, k_num_samples, k_sample_rate);
		set_sample(track0[0], sample_interpolator_t::constant, k_sample_values[0]);
		set_sample(track0[1], sample_interpolator_t::constant, k_sample_values[1]);
		set_sample(track0[2], sample_interpolator_t::linear, k_sample_values[2]);
		set_sample(track0[3], sample_interpolator_t::linear, k_sample_values[3]);

		track_desc_scalarf desc1 = desc0;
		desc1.output_index = 1;

		track_type track1 = track_type::make_reserve(desc1, allocator, k_num_samples, k_sample_rate);
		for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			set_sample(track1[sample_index], sample_interpolator_t::constant, k_constant_track_value);

		track_array_type raw_tracks(allocator, k_num_tracks);
		raw_tracks[0] = std::move(track0);
		raw_tracks[1] = std::move(track1);

		// The variable format tests the extended samples with a variable bit rate
		const compression_settings settings = get_default_compression_settings();
		output_stats stats;
		compressed_tracks* tracks = nullptr;
		const error_result result = compress_track_list(allocator, raw_tracks, settings, tracks, stats);
		REQUIRE(result.empty());
		REQUIRE(tracks != nullptr);

		const track_metadata* per_track_metadata = get_scalar_tracks_header(*tracks).get_track_metadata();
		CHECK(get_tracks_header(*tracks).get_has_extended_samples());
		CHECK(per_track_metadata[0].get_sample_interpolation_mode() == sample_interpolation_mode_t::mixed);
		CHECK(per_track_metadata[0].get_bit_rate() != 0);

		// A constant track has no animated data, thus it does not need sample interpolators
		CHECK(per_track_metadata[1].get_sample_interpolation_mode() == sample_interpolation_mode_t::linear);
		CHECK(per_track_metadata[1].get_bit_rate() == 0);

		// The animated track must use a variable bit rate, not the raw bit rate
		CHECK(!is_raw_bit_rate(per_track_metadata[0].get_bit_rate()));

		// The default settings do not support a per track rounding policy
		validate_extended_samples<default_scalar_decompression_settings, debug_track_writer>(allocator, raw_tracks, *tracks, false);

		// The debug settings support a per track rounding policy
		validate_extended_samples<debug_scalar_decompression_settings, debug_track_writer>(allocator, raw_tracks, *tracks, false);
		validate_extended_samples<debug_scalar_decompression_settings, debug_track_writer_per_track_rounding>(allocator, raw_tracks, *tracks, true);

		allocator.deallocate(tracks, tracks->get_size());
	}

	inline bool is_sample_value_equal(float value, float expected_value)
	{
		return value == expected_value;
	}

	inline bool is_sample_value_equal(const rtm::float4f& value, const rtm::float4f& expected_value)
	{
		return value.x == expected_value.x && value.y == expected_value.y && value.z == expected_value.z && value.w == expected_value.w;
	}

	template<class track_array_type>
	void test_extended_samples_conversion()
	{
		using typed_track = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		// Track 0: each sample is linear, track 1: each sample is constant, track 2: both interpolators
		constexpr uint32_t num_tracks = 3;
		const sample_interpolator_t interpolators[num_tracks][k_num_samples] =
		{
			{ sample_interpolator_t::linear, sample_interpolator_t::linear, sample_interpolator_t::linear, sample_interpolator_t::linear },
			{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::constant },
			{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::linear },
		};

		track_array_type raw_tracks(allocator, num_tracks);
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			track_desc_scalarf desc;
			desc.output_index = track_index;
			desc.precision = 0.0001F;

			typed_track track_ = typed_track::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				set_sample(track_[sample_index], interpolators[track_index][sample_index], k_sample_values[sample_index] + float(track_index));

			raw_tracks[track_index] = std::move(track_);
		}

		// Raw tracks -> compressed tracks
		compressed_tracks* tracks = nullptr;
		REQUIRE(convert_track_list(allocator, raw_tracks, tracks).empty());
		REQUIRE(tracks != nullptr);

		const track_metadata* per_track_metadata = get_scalar_tracks_header(*tracks).get_track_metadata();
		CHECK(per_track_metadata[0].get_sample_interpolation_mode() == sample_interpolation_mode_t::linear);
		CHECK(per_track_metadata[1].get_sample_interpolation_mode() == sample_interpolation_mode_t::constant);
		CHECK(per_track_metadata[2].get_sample_interpolation_mode() == sample_interpolation_mode_t::mixed);

		// Compressed tracks -> raw tracks, each sample must keep its interpolator and its exact value
		track_array converted_tracks;
		REQUIRE(convert_track_list(allocator, *tracks, converted_tracks).empty());
		REQUIRE(converted_tracks.get_num_tracks() == num_tracks);

		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			INFO("track index: " << track_index);

			REQUIRE(converted_tracks[track_index].get_interpolator() == track_interpolator_t::extended);

			const typed_track& raw_track = raw_tracks[track_index];
			const typed_track& converted_track = track_cast<typed_track>(converted_tracks[track_index]);
			REQUIRE(converted_track.get_num_samples() == k_num_samples);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				INFO("sample index: " << sample_index);

				CHECK(converted_track[sample_index].interpolator == raw_track[sample_index].interpolator);
				CHECK(is_sample_value_equal(converted_track[sample_index].value, raw_track[sample_index].value));
			}
		}

		allocator.deallocate(tracks, tracks->get_size());
	}

	// With the wrap policy, the last sample interpolates to the first sample.
	// The interval from the last sample (0.75) to the first sample is [0.75, 1.0).
	// Track 0: the last sample is constant, track 1: the last sample is linear, track 2: each sample is constant.
	// The value of track N is (sample value + N * 10.0).
	constexpr uint32_t k_num_wrap_tracks = 3;
	constexpr float k_wrap_track_offset = 10.0F;

	const sample_interpolator_t k_wrap_interpolators[k_num_wrap_tracks][k_num_samples] =
	{
		{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::constant },
		{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::linear, sample_interpolator_t::linear },
		{ sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::constant, sample_interpolator_t::constant },
	};

	struct wrap_test_case
	{
		float sample_time;
		sample_rounding_policy rounding_policy;
		float expected_values[k_num_wrap_tracks];	// Without the track offset
	};

	const wrap_test_case k_wrap_test_cases[] =
	{
		// Between sample 2 and 3, alpha = 0.4. Only track 2 is constant.
		{ 0.6F, sample_rounding_policy::none, { 4.4F, 4.4F, 2.0F } },
		{ 0.6F, sample_rounding_policy::floor, { 2.0F, 2.0F, 2.0F } },
		{ 0.6F, sample_rounding_policy::ceil, { 8.0F, 8.0F, 8.0F } },
		{ 0.6F, sample_rounding_policy::nearest, { 2.0F, 2.0F, 2.0F } },

		// Between sample 3 and sample 0, alpha = 0.4
		{ 0.85F, sample_rounding_policy::none, { 8.0F, 5.2F, 8.0F } },
		{ 0.85F, sample_rounding_policy::floor, { 8.0F, 8.0F, 8.0F } },
		{ 0.85F, sample_rounding_policy::ceil, { 1.0F, 1.0F, 1.0F } },
		{ 0.85F, sample_rounding_policy::nearest, { 8.0F, 8.0F, 8.0F } },

		// Between sample 3 and sample 0, alpha = 0.8
		{ 0.95F, sample_rounding_policy::none, { 8.0F, 2.4F, 8.0F } },
		{ 0.95F, sample_rounding_policy::floor, { 8.0F, 8.0F, 8.0F } },
		{ 0.95F, sample_rounding_policy::ceil, { 1.0F, 1.0F, 1.0F } },
		{ 0.95F, sample_rounding_policy::nearest, { 1.0F, 1.0F, 1.0F } },
	};

	template<class decompression_settings_type, class track_writer_type>
	void validate_extended_samples_wrap(iallocator& allocator, const track_array& raw_tracks, const compressed_tracks& tracks, bool use_per_track_rounding)
	{
		const track_type8 track_type = raw_tracks.get_track_type();

		decompression_context<decompression_settings_type> context;
		REQUIRE(context.initialize(tracks));

		track_writer_type raw_tracks_writer(allocator, track_type, k_num_wrap_tracks);
		track_writer_type lossy_tracks_writer(allocator, track_type, k_num_wrap_tracks);

		for (const wrap_test_case& test_case : k_wrap_test_cases)
		{
			INFO("sample time: " << test_case.sample_time << ", rounding policy: " << static_cast<int>(test_case.rounding_policy));

			// With a per track rounding policy, the seek uses 'per_track' and the writer sets the policy
			const sample_rounding_policy seek_rounding_policy = use_per_track_rounding ? sample_rounding_policy::per_track : test_case.rounding_policy;

			set_rounding_policy(raw_tracks_writer, test_case.rounding_policy);
			set_rounding_policy(lossy_tracks_writer, test_case.rounding_policy);

			raw_tracks.sample_tracks(test_case.sample_time, seek_rounding_policy, raw_tracks_writer);

			context.seek(test_case.sample_time, seek_rounding_policy);
			context.decompress_tracks(lossy_tracks_writer);

			for (uint32_t track_index = 0; track_index < k_num_wrap_tracks; ++track_index)
			{
				INFO("track index: " << track_index);

				const float expected_value = test_case.expected_values[track_index] + float(track_index) * k_wrap_track_offset;

				CHECK(is_value_near(raw_tracks_writer, track_type, track_index, expected_value));
				CHECK(is_value_near(lossy_tracks_writer, track_type, track_index, expected_value));
			}
		}
	}

	template<class track_array_type>
	void test_extended_samples_wrap()
	{
		using typed_track = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		track_array_type raw_tracks(allocator, k_num_wrap_tracks);
		for (uint32_t track_index = 0; track_index < k_num_wrap_tracks; ++track_index)
		{
			track_desc_scalarf desc;
			desc.output_index = track_index;
			desc.precision = 0.0001F;

			typed_track track_ = typed_track::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				const float value = k_sample_values[sample_index] + float(track_index) * k_wrap_track_offset;
				set_sample(track_[sample_index], k_wrap_interpolators[track_index][sample_index], value);
			}

			raw_tracks[track_index] = std::move(track_);
		}

		raw_tracks.set_looping_policy(sample_looping_policy::wrap);

		const compression_settings settings = get_default_compression_settings();
		output_stats stats;
		compressed_tracks* tracks = nullptr;
		const error_result result = compress_track_list(allocator, raw_tracks, settings, tracks, stats);
		REQUIRE(result.empty());
		REQUIRE(tracks != nullptr);

		CHECK(tracks->get_looping_policy() == sample_looping_policy::wrap);

		const track_metadata* per_track_metadata = get_scalar_tracks_header(*tracks).get_track_metadata();
		CHECK(per_track_metadata[0].get_sample_interpolation_mode() == sample_interpolation_mode_t::mixed);
		CHECK(per_track_metadata[1].get_sample_interpolation_mode() == sample_interpolation_mode_t::mixed);
		CHECK(per_track_metadata[2].get_sample_interpolation_mode() == sample_interpolation_mode_t::constant);

		validate_extended_samples_wrap<default_scalar_decompression_settings, debug_track_writer>(allocator, raw_tracks, *tracks, false);
		validate_extended_samples_wrap<debug_scalar_decompression_settings, debug_track_writer_per_track_rounding>(allocator, raw_tracks, *tracks, true);

		allocator.deallocate(tracks, tracks->get_size());
	}

	// Returns true if the two writers have near values for a track
	inline bool are_values_near(const debug_track_writer& writer0, const debug_track_writer& writer1, track_type8 track_type, uint32_t track_index)
	{
		if (track_type == track_type8::float1f)
			return rtm::scalar_near_equal(writer0.read_float1(track_index), writer1.read_float1(track_index), k_value_threshold);

		return rtm::vector_all_near_equal(writer0.read_float4(track_index), writer1.read_float4(track_index), k_value_threshold);
	}

	struct mode_test_case
	{
		const char* name;
		sample_looping_policy looping_policy;
		float values[k_num_samples];
		sample_interpolator_t interpolators[k_num_samples];
		sample_interpolation_mode_t expected_mode;
	};

	constexpr sample_interpolator_t k_c = sample_interpolator_t::constant;
	constexpr sample_interpolator_t k_l = sample_interpolator_t::linear;

	const mode_test_case k_mode_test_cases[] =
	{
		// Without the wrap policy, no interval follows the last sample
		{ "last sample, clamp", sample_looping_policy::clamp, { 1.0F, 5.0F, 2.0F, 8.0F }, { k_c, k_c, k_c, k_l }, sample_interpolation_mode_t::constant },
		{ "last sample, non-looping", sample_looping_policy::non_looping, { 1.0F, 5.0F, 2.0F, 8.0F }, { k_l, k_l, k_l, k_c }, sample_interpolation_mode_t::linear },

		// With the wrap policy, the last sample interpolates to the first sample
		{ "last sample, wrap", sample_looping_policy::wrap, { 1.0F, 5.0F, 2.0F, 8.0F }, { k_c, k_c, k_c, k_l }, sample_interpolation_mode_t::mixed },

		// When the next sample has the same value, the interpolator has no effect
		{ "equal values, linear sample", sample_looping_policy::clamp, { 1.0F, 5.0F, 5.0F, 8.0F }, { k_c, k_l, k_c, k_c }, sample_interpolation_mode_t::constant },
		{ "equal values, constant sample", sample_looping_policy::clamp, { 1.0F, 5.0F, 5.0F, 8.0F }, { k_l, k_c, k_l, k_l }, sample_interpolation_mode_t::linear },

		// When we wrap, the last sample and the first sample have the same value
		{ "equal values, wrap", sample_looping_policy::wrap, { 1.0F, 5.0F, 2.0F, 1.0F }, { k_c, k_c, k_c, k_l }, sample_interpolation_mode_t::constant },

		// Each interpolator has an effect
		{ "mixed", sample_looping_policy::clamp, { 1.0F, 5.0F, 2.0F, 8.0F }, { k_l, k_c, k_l, k_c }, sample_interpolation_mode_t::mixed },
	};

	constexpr sample_rounding_policy k_rounding_policies[] =
	{
		sample_rounding_policy::none,
		sample_rounding_policy::floor,
		sample_rounding_policy::ceil,
		sample_rounding_policy::nearest,
	};

	template<class track_array_type>
	void test_extended_samples_mode()
	{
		using typed_track = typename track_array_type::track_member_type;

		ansi_allocator allocator;

		for (const mode_test_case& test_case : k_mode_test_cases)
		{
			INFO("test case: " << test_case.name);

			track_desc_scalarf desc;
			desc.output_index = 0;
			desc.precision = 0.0001F;

			typed_track track_ = typed_track::make_reserve(desc, allocator, k_num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				set_sample(track_[sample_index], test_case.interpolators[sample_index], test_case.values[sample_index]);

			track_array_type raw_tracks(allocator, 1);
			raw_tracks[0] = std::move(track_);
			raw_tracks.set_looping_policy(test_case.looping_policy);

			const compression_settings settings = get_default_compression_settings();
			output_stats stats;
			compressed_tracks* tracks = nullptr;
			const error_result result = compress_track_list(allocator, raw_tracks, settings, tracks, stats);
			REQUIRE(result.empty());
			REQUIRE(tracks != nullptr);

			const track_metadata* per_track_metadata = get_scalar_tracks_header(*tracks).get_track_metadata();
			CHECK(per_track_metadata[0].get_sample_interpolation_mode() == test_case.expected_mode);

			// Only a track with a linear mode has no extended samples
			CHECK(get_tracks_header(*tracks).get_has_extended_samples() == (test_case.expected_mode != sample_interpolation_mode_t::linear));

			// The output must not change: the decompressed values must match the raw values
			const track_type8 track_type = raw_tracks.get_track_type();
			debug_track_writer raw_writer(allocator, track_type, 1);
			debug_track_writer lossy_writer(allocator, track_type, 1);

			decompression_context<default_scalar_decompression_settings> context;
			REQUIRE(context.initialize(*tracks));

			const float duration = raw_tracks.get_finite_duration();
			const uint32_t num_sample_times = 41;
			for (uint32_t sample_time_index = 0; sample_time_index < num_sample_times; ++sample_time_index)
			{
				const float sample_time = rtm::scalar_min(duration * float(sample_time_index) / float(num_sample_times - 1), duration);

				for (sample_rounding_policy rounding_policy : k_rounding_policies)
				{
					INFO("sample time: " << sample_time << ", rounding policy: " << static_cast<int>(rounding_policy));

					raw_tracks.sample_tracks(sample_time, rounding_policy, raw_writer);

					context.seek(sample_time, rounding_policy);
					context.decompress_tracks(lossy_writer);

					CHECK(are_values_near(raw_writer, lossy_writer, track_type, 0));
				}
			}

			allocator.deallocate(tracks, tracks->get_size());
		}
	}
}

TEST_CASE("extended samples float1f", "[compression][extended_samples]")
{
	test_extended_samples<track_array_float1f_ex>();
}

TEST_CASE("extended samples float4f", "[compression][extended_samples]")
{
	test_extended_samples<track_array_float4f_ex>();
}

TEST_CASE("extended samples conversion float1f", "[compression][extended_samples]")
{
	test_extended_samples_conversion<track_array_float1f_ex>();
}

TEST_CASE("extended samples conversion float4f", "[compression][extended_samples]")
{
	test_extended_samples_conversion<track_array_float4f_ex>();
}

TEST_CASE("extended samples wrap float1f", "[compression][extended_samples]")
{
	test_extended_samples_wrap<track_array_float1f_ex>();
}

TEST_CASE("extended samples wrap float4f", "[compression][extended_samples]")
{
	test_extended_samples_wrap<track_array_float4f_ex>();
}

TEST_CASE("extended samples mode float1f", "[compression][extended_samples]")
{
	test_extended_samples_mode<track_array_float1f_ex>();
}

TEST_CASE("extended samples mode float4f", "[compression][extended_samples]")
{
	test_extended_samples_mode<track_array_float4f_ex>();
}
