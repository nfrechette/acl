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
#include <acl/compression/compression_memory_desc.h>
#include <acl/compression/track_array.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/core/ansi_allocator.h>

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqvf.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

using namespace acl;

namespace
{
	constexpr float k_sample_rate = 30.0F;

	// An allocator that measures the peak of the sum of the live allocation sizes
	// The peak does not include the allocations that were live when 'reset_peak' was called
	class peak_allocator final : public iallocator
	{
	public:
		virtual void* allocate(size_t size, size_t alignment = k_default_alignment) override
		{
			m_size += size;
			m_peak_size = m_size > m_peak_size ? m_size : m_peak_size;
			return m_allocator.allocate(size, alignment);
		}

		virtual void deallocate(void* ptr, size_t size) override
		{
			if (ptr == nullptr)
				return;

			m_size -= size;
			m_allocator.deallocate(ptr, size);
		}

		void reset_peak()
		{
			m_base_size = m_size;
			m_peak_size = m_size;
		}

		size_t get_peak_size() const { return m_peak_size - m_base_size; }

	private:
		ansi_allocator m_allocator;
		size_t m_size = 0;
		size_t m_base_size = 0;
		size_t m_peak_size = 0;
	};

	// The hierarchy is a binary tree
	track_desc_transformf make_desc(uint32_t transform_index)
	{
		track_desc_transformf desc;
		desc.output_index = transform_index;
		desc.parent_index = transform_index == 0 ? k_invalid_track_index : ((transform_index - 1) / 2);
		desc.precision = 0.001F;
		desc.shell_distance = 3.0F;
		return desc;
	}

	// Each sub-track of each transform is animated, this gives the largest peak memory usage
	rtm::vqvf get_sample(uint32_t transform_index, uint32_t sample_index)
	{
		const float time = float(sample_index) / k_sample_rate;
		const float offset = float(transform_index) * 0.37F;
		const rtm::quatf rotation = rtm::quat_from_euler(std::sin(time + offset), 0.5F * std::cos(time * 1.3F + offset), 0.2F + 0.1F * time);
		const rtm::vector4f translation = rtm::vector_set(1.0F + std::sin(time * 2.0F + offset), offset * time, 2.0F * std::cos(time + offset));
		const rtm::vector4f scale = rtm::vector_set(1.0F + 0.2F * std::sin(time + offset), 0.9F + 0.1F * time, 1.1F + 0.3F * std::cos(time * 0.7F + offset));
		return rtm::vqv_set(rotation, translation, scale);
	}

	// The shapes of the clips: few or many transforms, one segment or many segments
	// When the clip loops, its last sample is equal to its first sample
	struct clip_shape_t
	{
		uint32_t num_transforms;
		uint32_t num_samples;
		bool is_looping;
	};

	const clip_shape_t k_clip_shapes[] =
	{
		{ 1, 1, false },
		{ 3, 2, false },
		{ 24, 17, false },
		{ 16, 31, false },
		{ 16, 32, false },		// Two segments of 16 samples
		{ 16, 32, true },		// The looping optimization removes a sample: the clip has one segment of 31 samples
		{ 8, 100, false },
	};

	// A smaller set of shapes for the variants of the settings and of the clip: one sample, two samples, and two segments
	// Each variant is a separate test case, thus the test cases can run in parallel
	const clip_shape_t k_variant_clip_shapes[] =
	{
		{ 1, 1, false },
		{ 3, 2, false },
		{ 16, 32, false },
	};

	template<class track_array_type, class track_type, class sample_fn_type>
	track_array_type make_clip(iallocator& allocator, const clip_shape_t& shape, sample_fn_type sample_fn)
	{
		const uint32_t num_transforms = shape.num_transforms;
		const uint32_t num_samples = shape.num_samples;

		track_array_type track_list(allocator, num_transforms);

		for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
		{
			track_type track_ = track_type::make_reserve(make_desc(transform_index), allocator, num_samples, k_sample_rate);

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const bool is_last_sample = sample_index == num_samples - 1;
				const uint32_t clip_sample_index = shape.is_looping && is_last_sample ? 0 : sample_index;
				track_[sample_index] = sample_fn(get_sample(transform_index, clip_sample_index));
			}

			track_.set_name(string(allocator, "a track with a name"));
			track_list[transform_index] = std::move(track_);
		}

		track_list.set_name(string(allocator, "a track list with a name"));
		return track_list;
	}

	track_array_vqvf make_vqv_clip(iallocator& allocator, const clip_shape_t& shape)
	{
		return make_clip<track_array_vqvf, track_vqvf>(allocator, shape, [](const rtm::vqvf& sample) { return sample; });
	}

	track_array_vqf make_vq_clip(iallocator& allocator, const clip_shape_t& shape)
	{
		return make_clip<track_array_vqf, track_vqf>(allocator, shape, [](const rtm::vqvf& sample) { return rtm::vq_set(sample.rotation, sample.translation); });
	}

	track_array_vqsf make_vqs_clip(iallocator& allocator, const clip_shape_t& shape)
	{
		return make_clip<track_array_vqsf, track_vqsf>(allocator, shape, [](const rtm::vqvf& sample) { return rtm::vqs_set(sample.rotation, sample.translation, rtm::vector_get_x(sample.scale)); });
	}

	track_array_float3f make_scalar_clip(iallocator& allocator, uint32_t num_tracks, uint32_t num_samples)
	{
		track_array_float3f track_list(allocator, num_tracks);

		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			track_desc_scalarf desc;
			desc.output_index = track_index;
			desc.precision = 0.001F;

			track_float3f track_ = track_float3f::make_reserve(desc, allocator, num_samples, k_sample_rate);
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				rtm::vector_store3(get_sample(track_index, sample_index).translation, &track_[sample_index].x);

			track_list[track_index] = std::move(track_);
		}

		return track_list;
	}

	// For a clip with only animated sub-tracks, the estimate must not be much larger than the peak
	// A small clip uses little memory, the fixed size of the estimate is larger than its peak
	constexpr double k_max_estimate_ratio = 2.0;
	constexpr size_t k_max_estimate_fixed_size = 16 * 1024;

	// Returns the largest number of ancestors of a transform
	uint32_t find_max_depth(const track_array& track_list)
	{
		uint32_t max_depth = 0;
		for (const track& track_ : track_list)
		{
			uint32_t depth = 0;
			uint32_t parent_index = track_.get_description<track_desc_transformf>().parent_index;
			while (parent_index != k_invalid_track_index)
			{
				depth++;
				parent_index = track_list[parent_index].get_description<track_desc_transformf>().parent_index;
			}

			max_depth = depth > max_depth ? depth : max_depth;
		}

		return max_depth;
	}

	// Returns a description of the track list without its hierarchy, as a caller can fill it before it loads the tracks
	compression_memory_desc_t make_desc_without_hierarchy(const track_array& track_list, const transform_base_desc_t* transform_base)
	{
		compression_memory_desc_t desc;
		desc.track_type = track_list.get_track_type();
		desc.num_tracks = track_list.get_num_tracks();
		desc.num_samples_per_track = track_list.get_num_samples_per_track();

		uint32_t total_name_length = uint32_t(track_list.get_name().size());
		for (const track& track_ : track_list)
			total_name_length += uint32_t(track_.get_name().size());
		desc.total_name_length = total_name_length;

		if (track_list.get_track_category() == track_category8::transformf)
			desc.transform_space = track_list.get_transform_space();

		if (transform_base != nullptr)
		{
			desc.base_format = transform_base->format;
			desc.num_base_tracks = transform_base->tracks->get_num_tracks();
			desc.num_base_samples_per_track = transform_base->tracks->get_num_samples_per_track();
		}

		return desc;
	}

	// Compresses and checks that the estimates are not smaller than the measured peak memory usage
	void check_estimate(peak_allocator& allocator, const track_array& track_list, const compression_settings& settings, const transform_base_desc_t* transform_base, const char* name)
	{
		const size_t estimate = transform_base != nullptr
			? estimate_compression_peak_memory_usage(track_list, settings, *transform_base)
			: estimate_compression_peak_memory_usage(track_list, settings);

		// Without the hierarchy, the estimate uses a single chain of transforms
		compression_memory_desc_t desc = make_desc_without_hierarchy(track_list, transform_base);
		const size_t estimate_without_hierarchy = estimate_compression_peak_memory_usage(desc, settings);

		// With all the values of the track list, the estimate is equal to the estimate of the track list
		if (track_list.get_track_category() == track_category8::transformf)
			desc.max_hierarchy_depth = find_max_depth(track_list);

		const size_t estimate_with_hierarchy = estimate_compression_peak_memory_usage(desc, settings);

		allocator.reset_peak();

		compressed_tracks* compressed_tracks_ = nullptr;
		output_stats stats;
		const error_result result = transform_base != nullptr
			? compress_track_list(allocator, track_list, settings, *transform_base, compressed_tracks_, stats)
			: compress_track_list(allocator, track_list, settings, compressed_tracks_, stats);
		REQUIRE(result.empty());

		const size_t peak_size = allocator.get_peak_size();
		allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());

		INFO(name << ": " << track_list.get_num_tracks() << " tracks, " << track_list.get_num_samples_per_track() << " samples, estimate " << estimate
			<< ", estimate without hierarchy " << estimate_without_hierarchy << ", peak " << peak_size);
		CHECK(estimate >= peak_size);
		CHECK(double(estimate) <= double(peak_size) * k_max_estimate_ratio + double(k_max_estimate_fixed_size));
		CHECK(estimate_without_hierarchy >= estimate);
		CHECK(estimate_with_hierarchy == estimate);
	}

	// The compression level changes how many bit rates the search tries, not the size of its buffers
	// The lowest level keeps the test fast
	compression_settings get_transform_settings()
	{
		compression_settings settings = get_default_compression_settings();
		settings.level = compression_level8::lowest;
		return settings;
	}

	// Compresses a VQV clip of each shape with the settings
	template<size_t num_shapes>
	void check_vqv_estimates(const clip_shape_t (&shapes)[num_shapes], const compression_settings& settings, const char* name)
	{
		peak_allocator allocator;

		for (const clip_shape_t& shape : shapes)
		{
			const track_array_vqvf vqv_tracks = make_vqv_clip(allocator, shape);
			check_estimate(allocator, vqv_tracks, settings, nullptr, name);
		}
	}
}

TEST_CASE("estimate the peak memory usage of transform compression", "[compression][transform]")
{
	check_vqv_estimates(k_clip_shapes, get_transform_settings(), "default");
}

TEST_CASE("estimate the peak memory usage of transform compression with full precision formats", "[compression][transform]")
{
	// Full precision formats do not split the clip into segments
	compression_settings settings = get_transform_settings();
	settings.rotation_format = rotation_format8::quatf_full;
	settings.translation_format = vector_format8::vector3f_full;
	settings.scale_format = vector_format8::vector3f_full;
	settings.keyframe_stripping = compression_keyframe_stripping_settings();

	check_vqv_estimates(k_variant_clip_shapes, settings, "full precision");
}

TEST_CASE("estimate the peak memory usage of transform compression with database support", "[compression][transform]")
{
	compression_settings settings = get_transform_settings();
	settings.enable_database_support = true;
	settings.keyframe_stripping = compression_keyframe_stripping_settings();

	check_vqv_estimates(k_variant_clip_shapes, settings, "database");
}

TEST_CASE("estimate the peak memory usage of transform compression with metadata", "[compression][transform]")
{
	compression_settings settings = get_transform_settings();
	settings.metadata.include_track_list_name = true;
	settings.metadata.include_track_names = true;
	settings.metadata.include_parent_track_indices = true;
	settings.metadata.include_track_descriptions = true;
	settings.metadata.include_contributing_error = true;

	check_vqv_estimates(k_variant_clip_shapes, settings, "metadata");
}

TEST_CASE("estimate the peak memory usage of transform compression with the matrix3x4f runtime type", "[compression][transform]")
{
	compression_settings settings = get_transform_settings();
	settings.transform_runtime_type = transform_runtime_type_t::matrix3x4f;

	check_vqv_estimates(k_variant_clip_shapes, settings, "matrix3x4f");
}

TEST_CASE("estimate the peak memory usage of transform compression with the vqmf runtime type", "[compression][transform]")
{
	compression_settings settings = get_transform_settings();
	settings.transform_runtime_type = transform_runtime_type_t::vqmf;

	check_vqv_estimates(k_variant_clip_shapes, settings, "vqmf");
}

// This test uses the deprecated legacy error metric of the settings
ACL_IMPL_DEPRECATION_WARNING_PUSH

TEST_CASE("estimate the peak memory usage of transform compression with the legacy error metric", "[compression][transform]")
{
	// The legacy matrix error metric does not support the bit rate algorithm v2
	vqvf_matrix3x4f_transform_error_metric legacy_metric;
	compression_settings settings = get_transform_settings();
	settings.error_metric = &legacy_metric;

	check_vqv_estimates(k_variant_clip_shapes, settings, "legacy metric");
}

ACL_IMPL_DEPRECATION_WARNING_POP

TEST_CASE("estimate the peak memory usage of vqf compression", "[compression][transform]")
{
	peak_allocator allocator;
	const compression_settings settings = get_transform_settings();

	for (const clip_shape_t& shape : k_variant_clip_shapes)
	{
		const track_array_vqf vq_tracks = make_vq_clip(allocator, shape);
		check_estimate(allocator, vq_tracks, settings, nullptr, "vqf");
	}
}

TEST_CASE("estimate the peak memory usage of vqsf compression", "[compression][transform]")
{
	peak_allocator allocator;
	const compression_settings settings = get_transform_settings();

	for (const clip_shape_t& shape : k_variant_clip_shapes)
	{
		const track_array_vqsf vqs_tracks = make_vqs_clip(allocator, shape);
		check_estimate(allocator, vqs_tracks, settings, nullptr, "vqsf");
	}
}

TEST_CASE("estimate the peak memory usage of additive transform compression", "[compression][transform]")
{
	peak_allocator allocator;
	const compression_settings settings = get_transform_settings();

	for (const clip_shape_t& shape : k_variant_clip_shapes)
	{
		const track_array_vqvf vqv_tracks = make_vqv_clip(allocator, shape);

		// A base with a different number of samples
		const clip_shape_t base_shape = { shape.num_transforms, shape.num_samples + 5, false };
		const track_array_vqvf base_tracks = make_vqv_clip(allocator, base_shape);
		transform_base_desc_t transform_base;
		transform_base.format = additive_clip_format8::additive1;
		transform_base.tracks = &base_tracks;
		check_estimate(allocator, vqv_tracks, settings, &transform_base, "additive1 base");
	}
}

TEST_CASE("estimate the peak memory usage of transform compression in mesh space", "[compression][transform]")
{
	peak_allocator allocator;
	const compression_settings settings = get_transform_settings();

	for (const clip_shape_t& shape : k_variant_clip_shapes)
	{
		track_array_vqvf mesh_tracks = make_vqv_clip(allocator, shape);
		mesh_tracks.set_transform_space(transform_space_t::mesh);
		check_estimate(allocator, mesh_tracks, settings, nullptr, "mesh space");
	}
}

TEST_CASE("estimate the peak memory usage of scalar compression", "[compression][scalar]")
{
	peak_allocator allocator;

	const compression_settings settings = get_default_compression_settings();

	for (const clip_shape_t& shape : k_clip_shapes)
	{
		const track_array_float3f track_list = make_scalar_clip(allocator, shape.num_transforms, shape.num_samples);
		check_estimate(allocator, track_list, settings, nullptr, "float3f");
	}
}

TEST_CASE("estimate the peak memory usage of compression without data", "[compression]")
{
	ansi_allocator allocator;

	const compression_settings settings = get_default_compression_settings();

	// An empty track list uses little memory
	const track_array empty_tracks;
	const size_t empty_estimate = estimate_compression_peak_memory_usage(empty_tracks, settings);
	CHECK(empty_estimate > 0);
	CHECK(empty_estimate < 16 * 1024);

	// Scalar tracks have no base
	const track_array_float3f scalar_tracks = make_scalar_clip(allocator, 2, 10);
	CHECK(estimate_compression_peak_memory_usage(scalar_tracks, settings, transform_base_desc_t()) == 0);

	// More samples need more memory
	const track_array_vqvf short_tracks = make_vqv_clip(allocator, clip_shape_t{ 10, 10, false });
	const track_array_vqvf long_tracks = make_vqv_clip(allocator, clip_shape_t{ 10, 1000, false });
	CHECK(estimate_compression_peak_memory_usage(short_tracks, settings) < estimate_compression_peak_memory_usage(long_tracks, settings));

	// A description without tracks uses little memory
	const compression_memory_desc_t empty_desc;
	const size_t empty_desc_estimate = estimate_compression_peak_memory_usage(empty_desc, settings);
	CHECK(empty_desc_estimate > 0);
	CHECK(empty_desc_estimate < 16 * 1024);

	// A smaller depth gives a smaller estimate, a depth larger than the number of transforms is a single chain
	compression_memory_desc_t desc;
	desc.num_tracks = 200;
	desc.num_samples_per_track = 30;
	const size_t unknown_depth_estimate = estimate_compression_peak_memory_usage(desc, settings);

	desc.max_hierarchy_depth = 10;
	CHECK(estimate_compression_peak_memory_usage(desc, settings) < unknown_depth_estimate);

	desc.max_hierarchy_depth = 1000;
	CHECK(estimate_compression_peak_memory_usage(desc, settings) == unknown_depth_estimate);

	// Scalar tracks do not use the hierarchy
	desc.track_type = track_type8::float3f;
	const size_t scalar_estimate = estimate_compression_peak_memory_usage(desc, settings);
	desc.max_hierarchy_depth = k_unknown_hierarchy_depth;
	CHECK(estimate_compression_peak_memory_usage(desc, settings) == scalar_estimate);
}
