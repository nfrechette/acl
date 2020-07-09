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
		class BoneChainIterator
		{
		public:
			BoneChainIterator(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index, uint16_t offset)
				: m_bone_chain(bone_chain)
				, m_bone_chain_desc(bone_chain_desc)
				, m_bone_index(bone_index)
				, m_offset(offset)
			{}

			BoneChainIterator& operator++()
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

			uint16_t operator*() const
			{
				ACL_ASSERT(m_offset <= m_bone_index, "Returned bone index doesn't belong to the bone chain");
				ACL_ASSERT(bitset_test(m_bone_chain, m_bone_chain_desc, m_offset), "Returned bone index doesn't belong to the bone chain");
				return m_offset;
			}

			// We only compare the offset in the bone chain. Two iterators on the same bone index
			// from two different or equal chains will be equal.
			bool operator==(const BoneChainIterator& other) const { return m_offset == other.m_offset; }
			bool operator!=(const BoneChainIterator& other) const { return m_offset != other.m_offset; }

		private:
			const uint32_t*		m_bone_chain;
			BitSetDescription	m_bone_chain_desc;
			uint16_t			m_bone_index;
			uint16_t			m_offset;
		};

		//////////////////////////////////////////////////////////////////////////
		// Simple bone chain container to allow easy looping
		//
		// A bone chain allows looping over all bones up to a specific bone starting
		// at the root bone.
		//////////////////////////////////////////////////////////////////////////
		struct BoneChain
		{
			BoneChain(const uint32_t* bone_chain, BitSetDescription bone_chain_desc, uint16_t bone_index)
				: m_bone_chain(bone_chain)
				, m_bone_chain_desc(bone_chain_desc)
				, m_bone_index(bone_index)
			{
				// We don't know where this bone chain starts, find the root bone
				// TODO: Use clz or similar to find the next set bit starting at the current index
				uint16_t root_index = 0;
				while (!bitset_test(bone_chain, bone_chain_desc, root_index))
					root_index++;

				m_root_index = root_index;
			}

			acl_impl::BoneChainIterator begin() const { return acl_impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_root_index); }
			acl_impl::BoneChainIterator end() const { return acl_impl::BoneChainIterator(m_bone_chain, m_bone_chain_desc, m_bone_index, m_bone_index + 1); }

			const uint32_t*		m_bone_chain;
			BitSetDescription	m_bone_chain_desc;
			uint16_t			m_root_index;
			uint16_t			m_bone_index;
		};

		struct transform_metadata
		{
			const uint32_t* transform_chain;
			uint16_t parent_index;
			float precision;
			float shell_distance;
		};

		struct ClipContext
		{
			SegmentContext* segments;
			BoneRanges* ranges;
			transform_metadata* metadata;
			uint32_t* leaf_transform_chains;

			uint16_t num_segments;
			uint16_t num_bones;
			uint16_t num_output_bones;
			uint32_t num_samples;
			float sample_rate;

			float duration;

			bool are_rotations_normalized;
			bool are_translations_normalized;
			bool are_scales_normalized;
			bool has_scale;
			bool has_additive_base;

			additive_clip_format8 additive_format;

			uint32_t num_leaf_transforms;

			// Stat tracking
			uint32_t decomp_touched_bytes;
			uint32_t decomp_touched_cache_lines;

			//////////////////////////////////////////////////////////////////////////

			Iterator<SegmentContext> segment_iterator() { return Iterator<SegmentContext>(segments, num_segments); }
			ConstIterator<SegmentContext> const_segment_iterator() const { return ConstIterator<SegmentContext>(segments, num_segments); }

			BoneChain get_bone_chain(uint32_t bone_index) const
			{
				ACL_ASSERT(bone_index < num_bones, "Invalid bone index: %u >= %u", bone_index, num_bones);
				const transform_metadata& meta = metadata[bone_index];
				return BoneChain(meta.transform_chain, BitSetDescription::make_from_num_bits(num_bones), (uint16_t)bone_index);
			}
		};

		inline void initialize_clip_context(IAllocator& allocator, const track_array_qvvf& track_list, additive_clip_format8 additive_format, ClipContext& out_clip_context)
		{
			const uint32_t num_transforms = track_list.get_num_tracks();
			const uint32_t num_samples = track_list.get_num_samples_per_track();
			const float sample_rate = track_list.get_sample_rate();

			ACL_ASSERT(num_transforms > 0, "Track array has no tracks!");
			ACL_ASSERT(num_samples > 0, "Track array has no samples!");

			// Create a single segment with the whole clip
			out_clip_context.segments = allocate_type_array<SegmentContext>(allocator, 1);
			out_clip_context.ranges = nullptr;
			out_clip_context.metadata = allocate_type_array<transform_metadata>(allocator, num_transforms);
			out_clip_context.leaf_transform_chains = nullptr;
			out_clip_context.num_segments = 1;
			out_clip_context.num_bones = safe_static_cast<uint16_t>(num_transforms);
			out_clip_context.num_output_bones = safe_static_cast<uint16_t>(num_transforms);
			out_clip_context.num_samples = num_samples;
			out_clip_context.sample_rate = sample_rate;
			out_clip_context.duration = track_list.get_duration();
			out_clip_context.are_rotations_normalized = false;
			out_clip_context.are_translations_normalized = false;
			out_clip_context.are_scales_normalized = false;
			out_clip_context.has_additive_base = additive_format != additive_clip_format8::none;
			out_clip_context.additive_format = additive_format;
			out_clip_context.num_leaf_transforms = 0;

			bool has_scale = false;
			const rtm::vector4f default_scale = get_default_scale(additive_format);

			SegmentContext& segment = out_clip_context.segments[0];

			BoneStreams* bone_streams = allocate_type_array<BoneStreams>(allocator, num_transforms);

			for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
			{
				const track_qvvf& track = track_list[transform_index];
				const track_desc_transformf& desc = track.get_description();

				BoneStreams& bone_stream = bone_streams[transform_index];

				bone_stream.segment = &segment;
				bone_stream.bone_index = safe_static_cast<uint16_t>(transform_index);
				bone_stream.parent_bone_index = desc.parent_index == k_invalid_track_index ? k_invalid_bone_index : safe_static_cast<uint16_t>(desc.parent_index);
				bone_stream.output_index = desc.output_index == k_invalid_track_index ? k_invalid_bone_index : safe_static_cast<uint16_t>(desc.output_index);

				bone_stream.rotations = RotationTrackStream(allocator, num_samples, sizeof(rtm::quatf), sample_rate, rotation_format8::quatf_full);
				bone_stream.translations = TranslationTrackStream(allocator, num_samples, sizeof(rtm::vector4f), sample_rate, vector_format8::vector3f_full);
				bone_stream.scales = ScaleTrackStream(allocator, num_samples, sizeof(rtm::vector4f), sample_rate, vector_format8::vector3f_full);

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const rtm::qvvf& transform = track[sample_index];

					bone_stream.rotations.set_raw_sample(sample_index, transform.rotation);
					bone_stream.translations.set_raw_sample(sample_index, transform.translation);
					bone_stream.scales.set_raw_sample(sample_index, transform.scale);
				}

				{
					const rtm::qvvf& first_transform = track[0];

					bone_stream.is_rotation_constant = num_samples == 1;
					bone_stream.is_rotation_default = bone_stream.is_rotation_constant && rtm::quat_near_identity(first_transform.rotation, desc.constant_rotation_threshold_angle);
					bone_stream.is_translation_constant = num_samples == 1;
					bone_stream.is_translation_default = bone_stream.is_translation_constant && rtm::vector_all_near_equal3(first_transform.translation, rtm::vector_zero(), desc.constant_translation_threshold);
					bone_stream.is_scale_constant = num_samples == 1;
					bone_stream.is_scale_default = bone_stream.is_scale_constant && rtm::vector_all_near_equal3(first_transform.scale, default_scale, desc.constant_scale_threshold);
				}

				has_scale |= !bone_stream.is_scale_default;

				if (bone_stream.is_stripped_from_output())
					out_clip_context.num_output_bones--;

				transform_metadata& metadata = out_clip_context.metadata[transform_index];
				metadata.transform_chain = nullptr;
				metadata.parent_index = desc.parent_index == k_invalid_track_index ? k_invalid_bone_index : safe_static_cast<uint16_t>(desc.parent_index);
				metadata.precision = desc.precision;
				metadata.shell_distance = desc.shell_distance;
			}

			out_clip_context.has_scale = has_scale;
			out_clip_context.decomp_touched_bytes = 0;
			out_clip_context.decomp_touched_cache_lines = 0;

			segment.bone_streams = bone_streams;
			segment.clip = &out_clip_context;
			segment.ranges = nullptr;
			segment.num_samples = safe_static_cast<uint16_t>(num_samples);
			segment.num_bones = safe_static_cast<uint16_t>(num_transforms);
			segment.clip_sample_offset = 0;
			segment.segment_index = 0;
			segment.distribution = SampleDistribution8::Uniform;
			segment.are_rotations_normalized = false;
			segment.are_translations_normalized = false;
			segment.are_scales_normalized = false;

			segment.animated_pose_bit_size = 0;
			segment.animated_data_size = 0;
			segment.range_data_size = 0;
			segment.total_header_size = 0;

			// Initialize our hierarchy information
			{
				// Calculate which bones are leaf bones that have no children
				BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(num_transforms);
				uint32_t* is_leaf_bitset = allocate_type_array<uint32_t>(allocator, bone_bitset_desc.get_size());
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

					const bool is_root = metadata.parent_index == k_invalid_bone_index;

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

				uint32_t* leaf_transform_chains = allocate_type_array<uint32_t>(allocator, size_t(num_leaf_transforms) * bone_bitset_desc.get_size());
				out_clip_context.leaf_transform_chains = leaf_transform_chains;

				uint32_t leaf_index = 0;
				for (uint32_t transform_index = 0; transform_index < num_transforms; ++transform_index)
				{
					if (!bitset_test(is_leaf_bitset, bone_bitset_desc, transform_index))
						continue;	// Skip non-leaf bones

					uint32_t* bone_chain = leaf_transform_chains + (leaf_index * bone_bitset_desc.get_size());
					bitset_reset(bone_chain, bone_bitset_desc, false);

					uint16_t chain_bone_index = safe_static_cast<uint16_t>(transform_index);
					while (chain_bone_index != k_invalid_bone_index)
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
				deallocate_type_array(allocator, is_leaf_bitset, bone_bitset_desc.get_size());
			}
		}

		inline void destroy_clip_context(IAllocator& allocator, ClipContext& clip_context)
		{
			for (SegmentContext& segment : clip_context.segment_iterator())
				destroy_segment_context(allocator, segment);

			deallocate_type_array(allocator, clip_context.segments, clip_context.num_segments);
			deallocate_type_array(allocator, clip_context.ranges, clip_context.num_bones);
			deallocate_type_array(allocator, clip_context.metadata, clip_context.num_bones);

			BitSetDescription bone_bitset_desc = BitSetDescription::make_from_num_bits(clip_context.num_bones);
			deallocate_type_array(allocator, clip_context.leaf_transform_chains, size_t(clip_context.num_leaf_transforms) * bone_bitset_desc.get_size());
		}

		constexpr bool segment_context_has_scale(const SegmentContext& segment) { return segment.clip->has_scale; }
		constexpr bool bone_streams_has_scale(const BoneStreams& bone_streams) { return segment_context_has_scale(*bone_streams.segment); }
	}
}

ACL_IMPL_FILE_PRAGMA_POP
