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

#include "acl/core/additive_utils.h"
#include "acl/core/bitset.h"
#include "acl/core/iallocator.h"
#include "acl/core/iterator.h"
#include "acl/core/error.h"
#include "acl/core/track_formats.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/impl/segment_context.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		//////////////////////////////////////////////////////////////////////////
		// Simple iterator utility class to allow easy looping
		class bone_chain_iterator
		{
		public:
			bone_chain_iterator(const uint32_t* chain, bitset_description chain_desc, uint32_t bone_index, uint32_t offset)
				: m_bone_chain(chain)
				, m_bone_chain_desc(chain_desc)
				, m_bone_index(bone_index)
				, m_offset(offset)
			{}

			bone_chain_iterator& operator++()
			{
				ACL_ASSERT(m_offset <= m_bone_index, "Cannot increment the iterator, it is no longer valid");

				// Skip the current bone
				m_offset++;

				// Iterate until we find the next bone part of the chain or until we reach the end of the chain
				// TODO: Use clz or similar to find the next set bit starting at the current index
				while (m_offset < m_bone_index && !bitset_test(m_bone_chain, m_bone_chain_desc, m_offset))
					m_offset++;

				return *this;
			}

			uint32_t operator*() const
			{
				ACL_ASSERT(m_offset <= m_bone_index, "Returned bone index doesn't belong to the bone chain");
				ACL_ASSERT(bitset_test(m_bone_chain, m_bone_chain_desc, m_offset), "Returned bone index doesn't belong to the bone chain");
				return m_offset;
			}

			// We only compare the offset in the bone chain. Two iterators on the same bone index
			// from two different or equal chains will be equal.
			bool operator==(const bone_chain_iterator& other) const { return m_offset == other.m_offset; }
			bool operator!=(const bone_chain_iterator& other) const { return m_offset != other.m_offset; }

		private:
			const uint32_t*		m_bone_chain;
			bitset_description	m_bone_chain_desc;
			uint32_t			m_bone_index;
			uint32_t			m_offset;
		};

		//////////////////////////////////////////////////////////////////////////
		// Simple bone chain container to allow easy looping
		//
		// A bone chain allows looping over all bones up to a specific bone starting
		// at the root bone.
		//////////////////////////////////////////////////////////////////////////
		struct bone_chain
		{
			bone_chain(const uint32_t* chain, bitset_description chain_desc, uint32_t bone_index)
				: m_bone_chain(chain)
				, m_bone_chain_desc(chain_desc)
				, m_bone_index(bone_index)
			{
				// We don't know where this bone chain starts, find the root bone
				// TODO: Use clz or similar to find the next set bit starting at the current index
				uint32_t root_index = 0;
				while (!bitset_test(chain, chain_desc, root_index))
					root_index++;

				m_root_index = root_index;
			}

			acl_impl::bone_chain_iterator begin() const { return acl_impl::bone_chain_iterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_root_index); }
			acl_impl::bone_chain_iterator end() const { return acl_impl::bone_chain_iterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_bone_index + 1); }

			const uint32_t*		m_bone_chain;
			bitset_description	m_bone_chain_desc;
			uint32_t			m_root_index;
			uint32_t			m_bone_index;
		};

		struct transform_metadata
		{
			const uint32_t* transform_chain				= nullptr;
			uint32_t parent_index						= k_invalid_track_index;
			float precision								= 0.0F;
			float shell_distance						= 0.0F;
		};

		struct clip_context
		{
			segment_context* segments					= nullptr;
			transform_range* ranges						= nullptr;
			transform_metadata* metadata				= nullptr;
			uint32_t* leaf_transform_chains				= nullptr;

			uint32_t num_segments						= 0;
			uint32_t num_bones							= 0;
			uint32_t num_samples						= 0;
			float sample_rate							= 0.0F;

			float duration								= 0.0F;

			bool are_rotations_normalized				= false;
			bool are_translations_normalized			= false;
			bool are_scales_normalized					= false;
			bool has_scale								= false;
			bool has_additive_base						= false;

			additive_clip_format8 additive_format		= additive_clip_format8::none;

			uint32_t num_leaf_transforms				= 0;

			iallocator* allocator						= nullptr;	// Never null if the context is initialized

			// Stat tracking
			uint32_t decomp_touched_bytes				= 0;
			uint32_t decomp_touched_cache_lines			= 0;

			//////////////////////////////////////////////////////////////////////////

			bool is_initialized() const { return allocator != nullptr; }
			iterator<segment_context> segment_iterator() { return iterator<segment_context>(segments, num_segments); }
			const_iterator<segment_context> segment_iterator() const { return const_iterator<segment_context>(segments, num_segments); }

			bone_chain get_bone_chain(uint32_t bone_index) const
			{
				ACL_ASSERT(bone_index < num_bones, "Invalid bone index: %u >= %u", bone_index, num_bones);
				const transform_metadata& meta = metadata[bone_index];
				return bone_chain(meta.transform_chain, bitset_description::make_from_num_bits(num_bones), bone_index);
			}
		};

		inline bool initialize_clip_context(iallocator& allocator, const track_array_qvvf& track_list, const compression_settings& settings, additive_clip_format8 additive_format, clip_context& out_clip_context)
		{
			const uint32_t num_transforms = track_list.get_num_tracks();
			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();

			// Create a single segment with the whole clip
			out_clip_context.segments = allocate_type_array<segment_context>(allocator, 1);
			out_clip_context.ranges = nullptr;
			out_clip_context.metadata = allocate_type_array<transform_metadata>(allocator, num_transforms);
			out_clip_context.leaf_transform_chains = nullptr;
			out_clip_context.num_segments = 1;
			out_clip_context.num_bones = num_transforms;
			out_clip_context.num_samples = num_samples;
			out_clip_context.sample_rate = sample_rate;
			out_clip_context.duration = track_list.get_duration();
			out_clip_context.are_rotations_normalized = false;
			out_clip_context.are_translations_normalized = false;
			out_clip_context.are_scales_normalized = false;
			out_clip_context.has_additive_base = additive_format != additive_clip_format8::none;
			out_clip_context.additive_format = additive_format;
			out_clip_context.num_leaf_transforms = 0;
			out_clip_context.allocator = &allocator;

			bool has_scale = false;
			bool are_samples_valid = true;
			const rtm::vector4f default_scale = get_default_scale(additive_format);

			segment_context& segment = out_clip_context.segments[0];

			transform_streams* bone_streams = allocate_type_array<transform_streams>(allocator, num_transforms);

			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const track_qvvf& track = track_list[transform_index];
				const track_desc_transformf& desc = track.get_description();

				transform_streams& bone_stream = bone_streams[transform_index];

				bone_stream.segment = &segment;
				bone_stream.bone_index = transform_index;
				bone_stream.parent_bone_index = desc.parent_index;
				bone_stream.output_index = desc.output_index;

				bone_stream.rotations = rotation_track_stream(allocator, num_samples, sizeof(rtm::quatf), sample_rate, rotation_format8::quatf_full);
				bone_stream.translations = translation_track_stream(allocator, num_samples, sizeof(rtm::vector4f), sample_rate, vector_format8::vector3f_full);
				bone_stream.scales = scale_track_stream(allocator, num_samples, sizeof(rtm::vector4f), sample_rate, vector_format8::vector3f_full);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const rtm::qvvf& transform = track[sample_index];

					// If we request raw data and we are already normalized, retain the original value
					// otherwise we normalize for safety
					rtm::quatf rotation;
					if (settings.rotation_format != rotation_format8::quatf_full || !rtm::quat_is_normalized(transform.rotation))
						rotation = rtm::quat_normalize(transform.rotation);
					else
						rotation = transform.rotation;

					are_samples_valid &= rtm::quat_is_finite(rotation);
					are_samples_valid &= rtm::vector_is_finite3(transform.translation);
					are_samples_valid &= rtm::vector_is_finite3(transform.scale);

					bone_stream.rotations.set_raw_sample(sample_index, rotation);
					bone_stream.translations.set_raw_sample(sample_index, transform.translation);
					bone_stream.scales.set_raw_sample(sample_index, transform.scale);
				}

				{
					const rtm::qvvf first_transform = num_samples != 0 ? track[0] : rtm::qvv_identity();
					const rtm::quatf first_rotation = rtm::quat_normalize(first_transform.rotation);

					// If we request raw data, use a 0.0 threshold for safety
					const float constant_rotation_threshold_angle = settings.rotation_format != rotation_format8::quatf_full ? desc.constant_rotation_threshold_angle : 0.0F;
					const float constant_translation_threshold = settings.translation_format != vector_format8::vector3f_full ? desc.constant_translation_threshold : 0.0F;
					const float constant_scale_threshold = settings.scale_format != vector_format8::vector3f_full ? desc.constant_scale_threshold : 0.0F;

					bone_stream.is_rotation_constant = num_samples <= 1;
					bone_stream.is_rotation_default = bone_stream.is_rotation_constant && rtm::quat_near_identity(first_rotation, constant_rotation_threshold_angle);
					bone_stream.is_translation_constant = num_samples <= 1;
					bone_stream.is_translation_default = bone_stream.is_translation_constant && rtm::vector_all_near_equal3(first_transform.translation, rtm::vector_zero(), constant_translation_threshold);
					bone_stream.is_scale_constant = num_samples <= 1;
					bone_stream.is_scale_default = bone_stream.is_scale_constant && rtm::vector_all_near_equal3(first_transform.scale, default_scale, constant_scale_threshold);
				}

				has_scale |= !bone_stream.is_scale_default;

				transform_metadata& metadata = out_clip_context.metadata[transform_index];
				metadata.transform_chain = nullptr;
				metadata.parent_index = desc.parent_index;
				metadata.precision = desc.precision;
				metadata.shell_distance = desc.shell_distance;
			}

			out_clip_context.has_scale = has_scale;
			out_clip_context.decomp_touched_bytes = 0;
			out_clip_context.decomp_touched_cache_lines = 0;

			segment.bone_streams = bone_streams;
			segment.clip = &out_clip_context;
			segment.ranges = nullptr;
			segment.contributing_error = nullptr;
			segment.num_samples = num_samples;
			segment.num_bones = num_transforms;
			segment.clip_sample_offset = 0;
			segment.segment_index = 0;
			segment.are_rotations_normalized = false;
			segment.are_translations_normalized = false;
			segment.are_scales_normalized = false;

			segment.animated_rotation_bit_size = 0;
			segment.animated_translation_bit_size = 0;
			segment.animated_scale_bit_size = 0;
			segment.animated_pose_bit_size = 0;
			segment.animated_data_size = 0;
			segment.range_data_size = 0;
			segment.total_header_size = 0;

			// Initialize our hierarchy information
			if (num_transforms != 0)
			{
				// Calculate which bones are leaf bones that have no children
				bitset_description bone_bitset_desc = bitset_description::make_from_num_bits(num_transforms);
				const size_t bitset_size = bone_bitset_desc.get_size();
				uint32_t* is_leaf_bitset = allocate_type_array<uint32_t>(allocator, bitset_size);
				bitset_reset(is_leaf_bitset, bone_bitset_desc, false);

				// By default  and if we find a child, we'll mark it as non-leaf
				bitset_set_range(is_leaf_bitset, bone_bitset_desc, 0, num_transforms, true);

#if defined(ACL_HAS_ASSERT_CHECKS)
				uint32_t num_root_bones = 0;
#endif

				// Move and validate the input data
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					const transform_metadata& metadata = out_clip_context.metadata[transform_index];

					const bool is_root = metadata.parent_index == k_invalid_track_index;

					// If we have a parent, mark it as not being a leaf bone (it has at least one child)
					if (!is_root)
						bitset_set(is_leaf_bitset, bone_bitset_desc, metadata.parent_index, false);

#if defined(ACL_HAS_ASSERT_CHECKS)
					if (is_root)
						num_root_bones++;
#endif
				}

				const uint32_t num_leaf_transforms = bitset_count_set_bits(is_leaf_bitset, bone_bitset_desc);
				out_clip_context.num_leaf_transforms = num_leaf_transforms;

				uint32_t* leaf_transform_chains = allocate_type_array<uint32_t>(allocator, num_leaf_transforms * bitset_size);
				out_clip_context.leaf_transform_chains = leaf_transform_chains;

				uint32_t leaf_index = 0;
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					if (!bitset_test(is_leaf_bitset, bone_bitset_desc, transform_index))
						continue;	// Skip non-leaf bones

					uint32_t* bone_chain = leaf_transform_chains + (leaf_index * bitset_size);
					bitset_reset(bone_chain, bone_bitset_desc, false);

					uint32_t chain_bone_index = transform_index;
					while (chain_bone_index != k_invalid_track_index)
					{
						bitset_set(bone_chain, bone_bitset_desc, chain_bone_index, true);

						transform_metadata& metadata = out_clip_context.metadata[chain_bone_index];

						// We assign a bone chain the first time we find a bone that isn't part of one already
						if (metadata.transform_chain == nullptr)
							metadata.transform_chain = bone_chain;

						chain_bone_index = metadata.parent_index;
					}

					leaf_index++;
				}

				ACL_ASSERT(num_root_bones > 0, "No root bone found. The root bones must have a parent index = 0xFFFF");
				ACL_ASSERT(leaf_index == num_leaf_transforms, "Invalid number of leaf bone found");
				deallocate_type_array(allocator, is_leaf_bitset, bitset_size);
			}

			return are_samples_valid;
		}

		inline void destroy_clip_context(clip_context& context)
		{
			if (context.allocator == nullptr)
				return;	// Not initialized

			iallocator& allocator = *context.allocator;

			for (segment_context& segment : context.segment_iterator())
				destroy_segment_context(allocator, segment);

			deallocate_type_array(allocator, context.segments, context.num_segments);
			deallocate_type_array(allocator, context.ranges, context.num_bones);
			deallocate_type_array(allocator, context.metadata, context.num_bones);

			bitset_description bone_bitset_desc = bitset_description::make_from_num_bits(context.num_bones);
			deallocate_type_array(allocator, context.leaf_transform_chains, size_t(context.num_leaf_transforms) * bone_bitset_desc.get_size());
		}

		constexpr bool segment_context_has_scale(const segment_context& segment) { return segment.clip->has_scale; }
		constexpr bool bone_streams_has_scale(const transform_streams& bone_streams) { return segment_context_has_scale(*bone_streams.segment); }
	}
}

ACL_IMPL_FILE_PRAGMA_POP
