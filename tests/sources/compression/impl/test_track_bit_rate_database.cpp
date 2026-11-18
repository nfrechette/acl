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

#include <acl/compression/compression_settings.h>
#include <acl/compression/track_array.h>
#include <acl/compression/impl/clip_context.h>
#include <acl/compression/impl/track_bit_rate_database.h>
#include <acl/core/ansi_allocator.h>
#include <acl/core/iallocator.h>

#include <rtm/quatf.h>
#include <rtm/vector4f.h>
#include <rtm/vqvf.h>

#include <cstddef>
#include <cstdint>
#include <utility>

using namespace acl;
using namespace acl::acl_impl;

namespace
{
	// An allocator that measures the sum of the live allocation sizes
	class size_allocator final : public iallocator
	{
	public:
		virtual void* allocate(size_t size, size_t alignment = k_default_alignment) override
		{
			m_size += size;
			return m_allocator.allocate(size, alignment);
		}

		virtual void deallocate(void* ptr, size_t size) override
		{
			if (ptr == nullptr)
				return;

			m_size -= size;
			m_allocator.deallocate(ptr, size);
		}

		size_t get_size() const { return m_size; }

	private:
		ansi_allocator m_allocator;
		size_t m_size = 0;
	};

	track_array_vqvf make_clip(iallocator& allocator, uint32_t num_transforms, uint32_t num_samples)
	{
		track_array_vqvf tracks(allocator, num_transforms);

		for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
		{
			track_desc_transformf desc;
			desc.output_index = transform_index;
			desc.parent_index = transform_index == 0 ? k_invalid_track_index : (transform_index - 1);

			track_vqvf track = track_vqvf::make_reserve(desc, allocator, num_samples, 30.0F);
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				track[sample_index] = rtm::vqv_set(rtm::quat_from_euler(0.1F * float(sample_index), 0.2F, 0.3F), rtm::vector_set(float(sample_index), 1.0F, 2.0F), rtm::vector_set(1.0F));

			tracks[transform_index] = std::move(track);
		}

		return tracks;
	}
}

TEST_CASE("track_bit_rate_database allocated size", "[compression][impl]")
{
	ansi_allocator allocator;

	const compression_settings settings = get_default_compression_settings();

	// The bitsets use more than one entry for each track with more than 32 samples
	const uint32_t num_transforms = 5;
	const uint32_t sample_counts[] = { 1, 16, 31, 40 };

	for (const uint32_t num_samples : sample_counts)
	{
		const track_array_vqvf tracks = make_clip(allocator, num_transforms, num_samples);

		clip_context clip;
		REQUIRE(initialize_clip_context(allocator, tracks, settings, additive_clip_format8::none, 1.0F, clip));

		const bool has_scale_values[] = { true, false };
		for (const bool has_scale : has_scale_values)
		{
			clip.has_scale = has_scale;

			size_allocator database_allocator;
			{
				const track_bit_rate_database database(database_allocator, settings.rotation_format, settings.translation_format, settings.scale_format,
					clip.segments->bone_streams, clip.segments->bone_streams, num_transforms, num_samples);

				// The sizes are equal to the bytes that the database allocates
				INFO(num_samples << " samples, has scale: " << has_scale);
				CHECK(database.get_allocated_size() == database_allocator.get_size());
				CHECK(track_bit_rate_database::get_required_size(num_transforms, num_samples, has_scale) == database_allocator.get_size());
			}

			CHECK(database_allocator.get_size() == 0);
		}

		// Without samples, the database is not used and allocates nothing
		{
			size_allocator database_allocator;
			const track_bit_rate_database database(database_allocator, settings.rotation_format, settings.translation_format, settings.scale_format,
				clip.segments->bone_streams, clip.segments->bone_streams, num_transforms, 0);

			CHECK(database_allocator.get_size() == 0);
			CHECK(database.get_allocated_size() == 0);
			CHECK(track_bit_rate_database::get_required_size(num_transforms, 0, true) == 0);
		}

		destroy_clip_context(clip);
	}
}
