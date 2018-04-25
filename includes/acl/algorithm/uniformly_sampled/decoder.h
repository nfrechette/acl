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

#include "acl/core/bitset.h"
#include "acl/core/compressed_clip.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/quat_packing.h"
#include "acl/decompression/decompress_data.h"
#include "acl/decompression/output_writer.h"

#include <cstdint>

//////////////////////////////////////////////////////////////////////////
// See encoder for details
//////////////////////////////////////////////////////////////////////////

namespace acl
{
	namespace uniformly_sampled
	{
		// 2 ways to encore a track as default: a bitset or omit the track
		// the second method requires a track id to be present to distinguish the
		// remaining tracks.
		// For a character, about 50-90 tracks are animated.
		// We ideally want to support more than 255 tracks or bones.
		// 50 * 16 bits = 100 bytes
		// 90 * 16 bits = 180 bytes
		// On the other hand, a character has about 140-180 bones, or 280-360 tracks (rotation/translation only)
		// 280 * 1 bit = 35 bytes
		// 360 * 1 bit = 45 bytes
		// It is obvious that storing a bitset is much more compact
		// A bitset also allows us to process and write track values in the order defined when compressed
		// unlike the track id method which makes it impossible to know which values are default until
		// everything has been decompressed (at which point everything else is default).
		// For the track id method to be more compact, an unreasonable small number of tracks would need to be
		// animated or constant compared to the total possible number of tracks. Those are likely to be rare.

		namespace impl
		{
			constexpr size_t k_cache_line_size = 64;

			struct alignas(k_cache_line_size) DecompressionContext
			{
				// Read-only data
				const SegmentHeader* segment_headers;

				const uint32_t* constant_tracks_bitset;
				const uint8_t* constant_track_data;
				const uint32_t* default_tracks_bitset;

				const uint8_t* clip_range_data;

				const uint8_t* format_per_track_data[2];
				const uint8_t* segment_range_data[2];
				const uint8_t* animated_track_data[2];

				BitSetDescription bitset_desc;
				uint8_t num_rotation_components;

				float clip_duration;

				bool has_mixed_packing;

				// Read-write data
				alignas(k_cache_line_size) uint32_t constant_track_offset;
				uint32_t constant_track_data_offset;
				uint32_t default_track_offset;
				uint32_t clip_range_data_offset;

				uint32_t format_per_track_data_offset;
				uint32_t segment_range_data_offset;

				uint32_t key_frame_byte_offsets[2];
				int32_t key_frame_bit_offsets[2];

				float interpolation_alpha;
			};

			// We use adapters to wrap the DecompressionSettings
			// This allows us to re-use the code for skipping and decompressing Vector3 samples
			// Code generation will generate specialized code for each specialization
			template<class SettingsType>
			struct TranslationDecompressionSettingsAdapter
			{
				TranslationDecompressionSettingsAdapter(const SettingsType& settings_) : settings(settings_) {}

				constexpr RangeReductionFlags8 get_range_reduction_flag() const { return RangeReductionFlags8::Translations; }
				constexpr Vector4_32 get_default_value() const { return vector_zero_32(); }
				constexpr VectorFormat8 get_vector_format(const ClipHeader& header) const { return settings.get_translation_format(header.translation_format); }
				constexpr bool is_vector_format_supported(VectorFormat8 format) const { return settings.is_translation_format_supported(format); }

				// Just forward the calls
				constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 flags) const { return settings.get_clip_range_reduction(flags); }
				constexpr RangeReductionFlags8 get_segment_range_reduction(RangeReductionFlags8 flags) const { return settings.get_segment_range_reduction(flags); }
				constexpr bool supports_mixed_packing() const { return settings.supports_mixed_packing(); }

				SettingsType settings;
			};

			template<class SettingsType>
			struct ScaleDecompressionSettingsAdapter
			{
				ScaleDecompressionSettingsAdapter(const SettingsType& settings_, const ClipHeader& header)
					: settings(settings_)
					, default_scale(header.default_scale ? vector_set(1.0f) : vector_zero_32())
				{}

				constexpr RangeReductionFlags8 get_range_reduction_flag() const { return RangeReductionFlags8::Scales; }
				inline Vector4_32 get_default_value() const { return default_scale; }
				constexpr VectorFormat8 get_vector_format(const ClipHeader& header) const { return settings.get_scale_format(header.scale_format); }
				constexpr bool is_vector_format_supported(VectorFormat8 format) const { return settings.is_scale_format_supported(format); }

				// Just forward the calls
				constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 flags) const { return settings.get_clip_range_reduction(flags); }
				constexpr RangeReductionFlags8 get_segment_range_reduction(RangeReductionFlags8 flags) const { return settings.get_segment_range_reduction(flags); }
				constexpr bool supports_mixed_packing() const { return settings.supports_mixed_packing(); }

				SettingsType settings;
				Vector4_32 default_scale;
			};

			template<class SettingsType>
			inline void initialize_context(const SettingsType& settings, const ClipHeader& header, DecompressionContext& context)
			{
				const RotationFormat8 rotation_format = settings.get_rotation_format(header.rotation_format);
				const VectorFormat8 translation_format = settings.get_translation_format(header.translation_format);
				const VectorFormat8 scale_format = settings.get_translation_format(header.scale_format);

#if defined(ACL_HAS_ASSERT_CHECKS)
				const RangeReductionFlags8 clip_range_reduction = settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = settings.get_segment_range_reduction(header.segment_range_reduction);

				ACL_ASSERT(rotation_format == header.rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(header.rotation_format));
				ACL_ASSERT(settings.is_rotation_format_supported(rotation_format), "Rotation format (%s) isn't statically supported!", get_rotation_format_name(rotation_format));
				ACL_ASSERT(translation_format == header.translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(header.translation_format));
				ACL_ASSERT(settings.is_translation_format_supported(translation_format), "Translation format (%s) isn't statically supported!", get_vector_format_name(translation_format));
				ACL_ASSERT(scale_format == header.scale_format, "Statically compiled scale format (%s) differs from the compressed scale format (%s)!", get_vector_format_name(scale_format), get_vector_format_name(header.scale_format));
				ACL_ASSERT(settings.is_scale_format_supported(scale_format), "Scale format (%s) isn't statically supported!", get_vector_format_name(scale_format));
				ACL_ASSERT(clip_range_reduction == header.clip_range_reduction, "Statically compiled clip range reduction settings (%u) differs from the compressed settings (%u)!", clip_range_reduction, header.clip_range_reduction);
				ACL_ASSERT(settings.are_clip_range_reduction_flags_supported(clip_range_reduction), "Clip range reduction settings (%u) aren't statically supported!", clip_range_reduction);
				ACL_ASSERT(segment_range_reduction == header.segment_range_reduction, "Statically compiled segment range reduction settings (%u) differs from the compressed settings (%u)!", segment_range_reduction, header.segment_range_reduction);
				ACL_ASSERT(settings.are_segment_range_reduction_flags_supported(segment_range_reduction), "Segment range reduction settings (%u) aren't statically supported!", segment_range_reduction);
#endif

				context.clip_duration = float(header.num_samples - 1) / float(header.sample_rate);
				context.segment_headers = header.get_segment_headers();
				context.default_tracks_bitset = header.get_default_tracks_bitset();

				context.constant_tracks_bitset = header.get_constant_tracks_bitset();
				context.constant_track_data = header.get_constant_track_data();
				context.clip_range_data = header.get_clip_range_data();

				for (uint8_t key_frame_index = 0; key_frame_index < 2; ++key_frame_index)
				{
					context.format_per_track_data[key_frame_index] = nullptr;
					context.segment_range_data[key_frame_index] = nullptr;
					context.animated_track_data[key_frame_index] = nullptr;
				}

				const uint32_t num_tracks_per_bone = header.has_scale ? 3 : 2;
				context.bitset_desc = BitSetDescription::make_from_num_bits(header.num_bones * num_tracks_per_bone);
				context.num_rotation_components = rotation_format == RotationFormat8::Quat_128 ? 4 : 3;

				// If all tracks are variable, no need for any extra padding except at the very end of the data
				// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
				const bool is_every_format_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
				const bool is_any_format_variable = is_rotation_format_variable(rotation_format) || is_vector_format_variable(translation_format) || is_vector_format_variable(scale_format);
				context.has_mixed_packing = !is_every_format_variable && is_any_format_variable;

				context.constant_track_offset = 0;
				context.constant_track_data_offset = 0;
				context.default_track_offset = 0;
				context.clip_range_data_offset = 0;
				context.format_per_track_data_offset = 0;
				context.segment_range_data_offset = 0;
			}

			template<class SettingsType>
			inline void seek(const SettingsType& settings, const ClipHeader& header, float sample_time, DecompressionContext& context)
			{
				context.constant_track_offset = 0;
				context.constant_track_data_offset = 0;
				context.default_track_offset = 0;
				context.clip_range_data_offset = 0;
				context.format_per_track_data_offset = 0;
				context.segment_range_data_offset = 0;

				const SampleRoundingPolicy rounding_policy = settings.get_sample_rounding_policy();

				uint32_t key_frame0;
				uint32_t key_frame1;
				find_linear_interpolation_samples(header.num_samples, context.clip_duration, sample_time, rounding_policy, key_frame0, key_frame1, context.interpolation_alpha);

				uint32_t segment_key_frame0 = 0;
				uint32_t segment_key_frame1 = 0;

				// Find segments
				// TODO: Use binary search?
				uint32_t segment_key_frame = 0;
				const SegmentHeader* segment_header0 = nullptr;
				const SegmentHeader* segment_header1 = nullptr;
				for (uint16_t segment_index = 0; segment_index < header.num_segments; ++segment_index)
				{
					const SegmentHeader& segment_header = context.segment_headers[segment_index];

					if (key_frame0 >= segment_key_frame && key_frame0 < segment_key_frame + segment_header.num_samples)
					{
						segment_header0 = &segment_header;
						segment_key_frame0 = key_frame0 - segment_key_frame;

						if (key_frame1 >= segment_key_frame && key_frame1 < segment_key_frame + segment_header.num_samples)
						{
							segment_header1 = &segment_header;
							segment_key_frame1 = key_frame1 - segment_key_frame;
						}
						else
						{
							ACL_ASSERT(segment_index + 1 < header.num_segments, "Invalid segment index: %u", segment_index + 1);
							const SegmentHeader& next_segment_header = context.segment_headers[segment_index + 1];
							segment_header1 = &next_segment_header;
							segment_key_frame1 = key_frame1 - (segment_key_frame + segment_header.num_samples);
						}

						break;
					}

					segment_key_frame += segment_header.num_samples;
				}

				ACL_ASSERT(segment_header0 != nullptr, "Failed to find segment.");

				context.format_per_track_data[0] = header.get_format_per_track_data(*segment_header0);
				context.format_per_track_data[1] = header.get_format_per_track_data(*segment_header1);
				context.segment_range_data[0] = header.get_segment_range_data(*segment_header0);
				context.segment_range_data[1] = header.get_segment_range_data(*segment_header1);
				context.animated_track_data[0] = header.get_track_data(*segment_header0);
				context.animated_track_data[1] = header.get_track_data(*segment_header1);

				context.key_frame_byte_offsets[0] = (segment_key_frame0 * segment_header0->animated_pose_bit_size) / 8;
				context.key_frame_byte_offsets[1] = (segment_key_frame1 * segment_header1->animated_pose_bit_size) / 8;
				context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
				context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Deriving from this struct and overriding these constexpr functions
		// allow you to control which code is stripped for maximum performance.
		// With these, you can:
		//    - Support only a subset of the formats and statically strip the rest
		//    - Force a single format and statically strip the rest
		//    - Decide all of this at runtime by not making the overrides constexpr
		//
		// By default, all formats are supported.
		//////////////////////////////////////////////////////////////////////////
		struct DecompressionSettings
		{
			constexpr bool is_rotation_format_supported(RotationFormat8 format) const { return true; }
			constexpr bool is_translation_format_supported(VectorFormat8 format) const { return true; }
			constexpr bool is_scale_format_supported(VectorFormat8 format) const { return true; }
			constexpr RotationFormat8 get_rotation_format(RotationFormat8 format) const { return format; }
			constexpr VectorFormat8 get_translation_format(VectorFormat8 format) const { return format; }
			constexpr VectorFormat8 get_scale_format(VectorFormat8 format) const { return format; }

			constexpr bool are_clip_range_reduction_flags_supported(RangeReductionFlags8 flags) const { return true; }
			constexpr bool are_segment_range_reduction_flags_supported(RangeReductionFlags8 flags) const { return true; }
			constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 flags) const { return flags; }
			constexpr RangeReductionFlags8 get_segment_range_reduction(RangeReductionFlags8 flags) const { return flags; }

			// Whether tracks must all be variable or all fixed width, or if they can be mixed and require padding
			constexpr bool supports_mixed_packing() const { return true; }

			constexpr SampleRoundingPolicy get_sample_rounding_policy() const { return SampleRoundingPolicy::None; }
		};

		template<class SettingsType>
		inline void* allocate_decompression_context(IAllocator& allocator, const SettingsType& settings, const CompressedClip& clip)
		{
			using namespace impl;

			DecompressionContext* context = allocate_type<DecompressionContext>(allocator);

			ACL_ASSERT(is_aligned_to(&context->segment_headers, k_cache_line_size), "Read-only decompression context is misaligned");
			ACL_ASSERT(is_aligned_to(&context->constant_track_offset, k_cache_line_size), "Read-write decompression context is misaligned");

			initialize_context(settings, get_clip_header(clip), *context);

			return context;
		}

		inline void deallocate_decompression_context(IAllocator& allocator, void* opaque_context)
		{
			using namespace impl;

			DecompressionContext* context = safe_ptr_cast<DecompressionContext>(opaque_context);
			deallocate_type<DecompressionContext>(allocator, context);
		}

		template<class SettingsType, class OutputWriterType>
		inline void decompress_pose(const SettingsType& settings, const CompressedClip& clip, void* opaque_context, float sample_time, OutputWriterType& writer)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");
			static_assert(std::is_base_of<OutputWriter, OutputWriterType>::value, "OutputWriterType must derive from OutputWriter!");

			using namespace impl;

			ACL_ASSERT(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ASSERT(clip.is_valid(false).empty(), "Clip is invalid");

			const ClipHeader& header = get_clip_header(clip);

			DecompressionContext& context = *safe_ptr_cast<DecompressionContext>(opaque_context);

			seek(settings, header, sample_time, context);

			const TranslationDecompressionSettingsAdapter<SettingsType> translation_adapter(settings);
			const ScaleDecompressionSettingsAdapter<SettingsType> scale_adapter(settings, header);

			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				Quat_32 rotation = decompress_and_interpolate_rotation(settings, header, context);
				writer.write_bone_rotation(bone_index, rotation);

				Vector4_32 translation = decompress_and_interpolate_vector(translation_adapter, header, context);
				writer.write_bone_translation(bone_index, translation);

				Vector4_32 scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, context) : scale_adapter.get_default_value();
				writer.write_bone_scale(bone_index, scale);
			}
		}

		template<class SettingsType>
		inline void decompress_bone(const SettingsType& settings, const CompressedClip& clip, void* opaque_context, float sample_time, uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation, Vector4_32* out_scale)
		{
			static_assert(std::is_base_of<DecompressionSettings, SettingsType>::value, "SettingsType must derive from DecompressionSettings!");

			using namespace impl;

			ACL_ASSERT(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));
			ACL_ASSERT(clip.is_valid(false).empty(), "Clip is invalid");

			const ClipHeader& header = get_clip_header(clip);

			DecompressionContext& context = *safe_ptr_cast<DecompressionContext>(opaque_context);

			seek(settings, header, sample_time, context);

			const TranslationDecompressionSettingsAdapter<SettingsType> translation_adapter(settings);
			const ScaleDecompressionSettingsAdapter<SettingsType> scale_adapter(settings, header);

			// TODO: Optimize this by counting the number of bits set, we can use the pop-count instruction on
			// architectures that support it (e.g. xb1/ps4). This would entirely avoid looping here.

			for (uint32_t bone_index = 0; bone_index < header.num_bones; ++bone_index)
			{
				if (bone_index == sample_bone_index)
					break;

				skip_rotations_in_two_key_frames(settings, header, context);
				skip_vectors_in_two_key_frames(translation_adapter, header, context);

				if (header.has_scale)
					skip_vectors_in_two_key_frames(scale_adapter, header, context);
			}

			// TODO: Skip if not interested in return value
			Quat_32 rotation = decompress_and_interpolate_rotation(settings, header, context);
			if (out_rotation != nullptr)
				*out_rotation = rotation;

			Vector4_32 translation = decompress_and_interpolate_vector(translation_adapter, header, context);
			if (out_translation != nullptr)
				*out_translation = translation;

			Vector4_32 scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, context) : scale_adapter.get_default_value();
			if (out_scale != nullptr)
				*out_scale = scale;
		}
	}
}
