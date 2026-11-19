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

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqf.h>
#include <rtm/vqsf.h>
#include <rtm/vqvf.h>

#include <cstdint>
#include <utility>

using namespace acl;

namespace
{
	constexpr uint32_t k_num_transforms = 4;
	constexpr uint32_t k_num_samples = 10;
	constexpr float k_sample_rate = 30.0F;

	track_desc_transformf make_desc(uint32_t transform_index)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : (transform_index - 1);
		desc.precision = 0.001F;
		desc.shell_distance = 3.0F;
		return desc;
	}

	// The rotation of each transform depends on its index:
	//    - 0: W is 1.0 with small [xyz] components on the odd samples, animated on the even samples
	//    - 1: W is -1.0 with small [xyz] components on each sample
	//    - 2: W is 1.0 with large [xyz] components on each sample, the error exceeds the precision
	//    - 3: animated
	rtm::quatf get_rotation(uint32_t transform_index, uint32_t sample_index)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);

		switch (transform_index)
		{
		case 0:
			if ((sample_index % 2) != 0)
				return rtm::quat_set(1.0E-5F, -2.0E-5F, 1.0E-5F, 1.0F);
			return rtm::quat_from_euler(alpha, 0.5F * alpha, 0.2F);
		case 1:
			return rtm::quat_set(-1.0E-5F, 1.0E-5F, 2.0E-5F, -1.0F);
		case 2:
			return rtm::quat_set(0.1F, 0.0F, 0.0F, 1.0F);
		default:
			return rtm::quat_from_euler(alpha + 0.3F, 0.5F * alpha, 0.2F);
		}
	}

	rtm::vector4f get_translation(uint32_t transform_index, uint32_t sample_index)
	{
		const float alpha = float(sample_index) / float(k_num_samples - 1);
		return rtm::vector_set(1.0F + alpha, float(transform_index) * 0.3F, 2.0F * alpha);
	}

	// Sets a transform sample, the scale is the identity
	void set_sample(rtm::vqf& sample, rtm::quatf_arg0 rotation, rtm::vector4f_arg1 translation) { sample = rtm::vq_set(rotation, translation); }
	void set_sample(rtm::vqsf& sample, rtm::quatf_arg0 rotation, rtm::vector4f_arg1 translation) { sample = rtm::vqs_set(rotation, translation, 1.0F); }
	void set_sample(rtm::vqvf& sample, rtm::quatf_arg0 rotation, rtm::vector4f_arg1 translation) { sample = rtm::vqv_set(rotation, translation, rtm::vector_set(1.0F)); }

	// Returns the translation of a transform sample, a VQS sample has its scale in [w]
	rtm::vector4f get_sample_translation(const rtm::vqf& sample) { return sample.translation; }
	rtm::vector4f get_sample_translation(const rtm::vqsf& sample) { return sample.translation_scale; }
	rtm::vector4f get_sample_translation(const rtm::vqvf& sample) { return sample.translation; }

	template<class track_array_type>
	track_array_type make_tracks(iallocator& allocator)
	{
		using track_type = typename track_array_type::track_member_type;

		track_array_type tracks(allocator, k_num_transforms);

		for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
		{
			track_type track_ = track_type::make_reserve(make_desc(transform_index), allocator, k_num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
				set_sample(track_[sample_index], get_rotation(transform_index, sample_index), get_translation(transform_index, sample_index));

			tracks[transform_index] = std::move(track_);
		}

		return tracks;
	}

	template<class track_array_type>
	void test_sanitize_identity_rotations()
	{
		ansi_allocator allocator;

		pre_process_settings_t settings;
		settings.actions = pre_process_actions::sanitize_identity_rotations;

		// With the lossless policy, the rotations do not change
		{
			track_array_type tracks = make_tracks<track_array_type>(allocator);
			settings.precision_policy = pre_process_precision_policy::lossless;
			REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());

			for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
			{
				for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
					CHECK(rtm::quat_near_equal(tracks[transform_index][sample_index].rotation, get_rotation(transform_index, sample_index), 0.0F));
			}
		}

		// With the lossy policy, the [xyz] components of a rotation with W = 1.0 or W = -1.0 become 0.0
		{
			track_array_type tracks = make_tracks<track_array_type>(allocator);
			settings.precision_policy = pre_process_precision_policy::lossy;
			REQUIRE(pre_process_track_list(allocator, settings, tracks).empty());

			for (uint32_t sample_index = 0; sample_index < k_num_samples; ++sample_index)
			{
				// Transform 0: only the odd samples change
				const rtm::quatf expected_rotation0 = (sample_index % 2) != 0 ? rtm::quat_identity() : get_rotation(0, sample_index);
				CHECK(rtm::quat_near_equal(tracks[0][sample_index].rotation, expected_rotation0, 0.0F));

				// Transform 1: W keeps its sign
				CHECK(rtm::quat_near_equal(tracks[1][sample_index].rotation, rtm::quat_set(0.0F, 0.0F, 0.0F, -1.0F), 0.0F));

				// Transform 2: the error exceeds the precision, the rotation does not change
				CHECK(rtm::quat_near_equal(tracks[2][sample_index].rotation, get_rotation(2, sample_index), 0.0F));

				// Transform 3: no rotation has W = 1.0
				CHECK(rtm::quat_near_equal(tracks[3][sample_index].rotation, get_rotation(3, sample_index), 0.0F));

				// The translations do not change
				for (uint32_t transform_index = 0; transform_index < k_num_transforms; ++transform_index)
					CHECK(rtm::vector_all_near_equal3(get_sample_translation(tracks[transform_index][sample_index]), get_translation(transform_index, sample_index), 0.0F));
			}
		}
	}
}

TEST_CASE("pre-process sanitize identity rotations vqvf", "[compression][pre_process]")
{
	test_sanitize_identity_rotations<track_array_vqvf>();
}

TEST_CASE("pre-process sanitize identity rotations vqf", "[compression][pre_process]")
{
	test_sanitize_identity_rotations<track_array_vqf>();
}

TEST_CASE("pre-process sanitize identity rotations vqsf", "[compression][pre_process]")
{
	test_sanitize_identity_rotations<track_array_vqsf>();
}

TEST_CASE("pre-process recommended actions sanitize identity rotations", "[compression][pre_process]")
{
	CHECK(are_any_enum_flags_set(pre_process_actions::recommended, pre_process_actions::sanitize_identity_rotations));
}
