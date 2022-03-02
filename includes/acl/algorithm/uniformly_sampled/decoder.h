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
#include "acl/core/bit_manip_utils.h"
#include "acl/core/compiler_utils.h"
#include "acl/core/compressed_clip.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_mask.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/utils.h"
#include "acl/math/quat_32.h"
#include "acl/math/vector4_32.h"
#include "acl/math/quat_packing.h"
#include "acl/decompression/decompress_data.h"
#include "acl/decompression/output_writer.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

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
				// Clip related data							//   offsets
				const CompressedClip* clip;						//   0 |   0

				const uint32_t* constant_tracks_bitset;			//   4 |   8
				const uint8_t* constant_track_data;				//   8 |  16
				const uint32_t* default_tracks_bitset;			//  12 |  24

				const uint8_t* clip_range_data;					//  16 |  32

				float clip_duration;							//  20 |  40

				BitSetDescription bitset_desc;					//  24 |  44

				uint32_t clip_hash;								//  28 |  48

				uint8_t num_rotation_components;				//  32 |  52
				uint8_t has_mixed_packing;						//  33 |  53

				uint8_t padding0[2];							//  34 |  54

				// Seeking related data
				const uint8_t* format_per_track_data[2];		//  36 |  56
				const uint8_t* segment_range_data[2];			//  44 |  72
				const uint8_t* animated_track_data[2];			//  52 |  88

				uint32_t key_frame_byte_offsets[2];				//  60 | 104	// Fixed quantization
				uint32_t key_frame_bit_offsets[2];				//  68 | 112	// Variable quantization

				float interpolation_alpha;						//  76 | 120
				float sample_time;								//  80 | 124

				const interpolation_mask* interp_mask;			//  84 | 128

				uint8_t padding1[sizeof(void*) == 4 ? 40 : 56];	//  88 | 136

				//									Total size:	   128 | 192
			};

			static_assert(sizeof(DecompressionContext) == (sizeof(void*) == 4 ? 128 : 192), "Unexpected size");

			struct alignas(k_cache_line_size) SamplingContext
			{
				static constexpr size_t k_num_samples_to_interpolate = 2;

				inline static Quat_32 ACL_SIMD_CALL interpolate_rotation(Quat_32Arg0 rotation0, Quat_32Arg1 rotation1, float interpolation_alpha)
				{
					return quat_lerp(rotation0, rotation1, interpolation_alpha);
				}

				inline static Quat_32 ACL_SIMD_CALL interpolate_rotation(Quat_32Arg0 rotation0, Quat_32Arg1 rotation1, Quat_32Arg2 rotation2, Quat_32Arg3 rotation3, float interpolation_alpha)
				{
					(void)rotation1;
					(void)rotation2;
					(void)rotation3;
					(void)interpolation_alpha;
					return rotation0;	// Not implemented, we use linear interpolation
				}

				inline static Vector4_32 ACL_SIMD_CALL interpolate_vector4(Vector4_32Arg0 vector0, Vector4_32Arg1 vector1, float interpolation_alpha)
				{
					return vector_lerp(vector0, vector1, interpolation_alpha);
				}

				inline static Vector4_32 ACL_SIMD_CALL interpolate_vector4(Vector4_32Arg0 vector0, Vector4_32Arg1 vector1, Vector4_32Arg2 vector2, Vector4_32Arg3 vector3, float interpolation_alpha)
				{
					(void)vector1;
					(void)vector2;
					(void)vector3;
					(void)interpolation_alpha;
					return vector0;		// Not implemented, we use linear interpolation
				}

				//													//   offsets
				uint32_t track_index;								//   0 |   0
				uint32_t constant_track_data_offset;				//   4 |   4
				uint32_t clip_range_data_offset;					//   8 |   8

				uint32_t format_per_track_data_offset;				//  12 |  12
				uint32_t segment_range_data_offset;					//  16 |  16

				uint32_t key_frame_byte_offsets[2];					//  20 |  20	// Fixed quantization
				uint32_t key_frame_bit_offsets[2];					//  28 |  28	// Variable quantization

				uint8_t padding[28];								//  36 |  36

				Vector4_32 vectors[k_num_samples_to_interpolate];	//  64 |  64
				Vector4_32 padding0[2];								//  96 |  96

				//										Total size:	   128 | 128
			};

			static_assert(sizeof(SamplingContext) == 128, "Unexpected size");

			// We use adapters to wrap the DecompressionSettings
			// This allows us to re-use the code for skipping and decompressing Vector3 samples
			// Code generation will generate specialized code for each specialization
			template<class SettingsType>
			struct TranslationDecompressionSettingsAdapter
			{
				explicit TranslationDecompressionSettingsAdapter(const SettingsType& settings_) : settings(settings_) {}

				constexpr RangeReductionFlags8 get_range_reduction_flag() const { return RangeReductionFlags8::Translations; }
				inline Vector4_32 ACL_SIMD_CALL get_default_value() const { return vector_zero_32(); }
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
				explicit ScaleDecompressionSettingsAdapter(const SettingsType& settings_, const ClipHeader& header)
					: settings(settings_)
					, default_scale(header.default_scale ? vector_set(1.0F) : vector_zero_32())
				{}

				constexpr RangeReductionFlags8 get_range_reduction_flag() const { return RangeReductionFlags8::Scales; }
				inline Vector4_32 ACL_SIMD_CALL get_default_value() const { return default_scale; }
				constexpr VectorFormat8 get_vector_format(const ClipHeader& header) const { return settings.get_scale_format(header.scale_format); }
				constexpr bool is_vector_format_supported(VectorFormat8 format) const { return settings.is_scale_format_supported(format); }

				// Just forward the calls
				constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 flags) const { return settings.get_clip_range_reduction(flags); }
				constexpr RangeReductionFlags8 get_segment_range_reduction(RangeReductionFlags8 flags) const { return settings.get_segment_range_reduction(flags); }
				constexpr bool supports_mixed_packing() const { return settings.supports_mixed_packing(); }

				SettingsType settings;
				uint8_t padding[get_required_padding<SettingsType, Vector4_32>()];
				Vector4_32 default_scale;
			};
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
			constexpr bool is_rotation_format_supported(RotationFormat8 /*format*/) const { return true; }
			constexpr bool is_translation_format_supported(VectorFormat8 /*format*/) const { return true; }
			constexpr bool is_scale_format_supported(VectorFormat8 /*format*/) const { return true; }
			constexpr RotationFormat8 get_rotation_format(RotationFormat8 format) const { return format; }
			constexpr VectorFormat8 get_translation_format(VectorFormat8 format) const { return format; }
			constexpr VectorFormat8 get_scale_format(VectorFormat8 format) const { return format; }

			constexpr bool are_clip_range_reduction_flags_supported(RangeReductionFlags8 /*flags*/) const { return true; }
			constexpr bool are_segment_range_reduction_flags_supported(RangeReductionFlags8 /*flags*/) const { return true; }
			constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 flags) const { return flags; }
			constexpr RangeReductionFlags8 get_segment_range_reduction(RangeReductionFlags8 flags) const { return flags; }

			// Whether tracks must all be variable or all fixed width, or if they can be mixed and require padding.
			constexpr bool supports_mixed_packing() const { return true; }

			// Whether to explicitly disable floating point exceptions during decompression.
			// This has a cost, exceptions are usually disabled globally and do not need to be
			// explicitly disabled during decompression.
			// We assume that floating point exceptions are already disabled by the caller.
			constexpr bool disable_fp_exeptions() const { return false; }
		};

		//////////////////////////////////////////////////////////////////////////
		// These are debug settings, everything is enabled and nothing is stripped.
		// It will have the worst performance but allows every feature.
		//////////////////////////////////////////////////////////////////////////
		struct DebugDecompressionSettings : DecompressionSettings {};

		//////////////////////////////////////////////////////////////////////////
		// These are the default settings. Only the generally optimal settings
		// are enabled and will offer the overall best performance.
		//
		// Note: Segment range reduction supports AllTracks or None because it can
		// be disabled if there is a single segment.
		//////////////////////////////////////////////////////////////////////////
		struct DefaultDecompressionSettings : DecompressionSettings
		{
			constexpr bool is_rotation_format_supported(RotationFormat8 format) const { return format == RotationFormat8::QuatDropW_Variable; }
			constexpr bool is_translation_format_supported(VectorFormat8 format) const { return format == VectorFormat8::Vector3_Variable; }
			constexpr bool is_scale_format_supported(VectorFormat8 format) const { return format == VectorFormat8::Vector3_Variable; }
			constexpr RotationFormat8 get_rotation_format(RotationFormat8 /*format*/) const { return RotationFormat8::QuatDropW_Variable; }
			constexpr VectorFormat8 get_translation_format(VectorFormat8 /*format*/) const { return VectorFormat8::Vector3_Variable; }
			constexpr VectorFormat8 get_scale_format(VectorFormat8 /*format*/) const { return VectorFormat8::Vector3_Variable; }

			constexpr RangeReductionFlags8 get_clip_range_reduction(RangeReductionFlags8 /*flags*/) const { return RangeReductionFlags8::AllTracks; }

			constexpr bool supports_mixed_packing() const { return false; }
		};

		//////////////////////////////////////////////////////////////////////////
		// Decompression context for the uniformly sampled algorithm. The context
		// allows various decompression actions to be performed in a clip.
		//
		// Both the constructor and destructor are public because it is safe to place
		// instances of this context on the stack or as member variables.
		//
		// This compression algorithm is the simplest by far and as such it offers
		// the fastest compression and decompression. Every sample is retained and
		// every track has the same number of samples playing back at the same
		// sample rate. This means that when we sample at a particular time within
		// the clip, we can trivially calculate the offsets required to read the
		// desired data. All the data is sorted in order to ensure all reads are
		// as contiguous as possible for optimal cache locality during decompression.
		//////////////////////////////////////////////////////////////////////////
		template<class DecompressionSettingsType>
		class DecompressionContext
		{
			static_assert(std::is_base_of<DecompressionSettings, DecompressionSettingsType>::value, "DecompressionSettingsType must derive from DecompressionSettings!");

		public:
			//////////////////////////////////////////////////////////////////////////
			// Constructs a context instance with an optional allocator instance.
			// The default constructor for the DecompressionSettingsType will be used.
			// If an allocator is provided, it will be used in `release()` to free the context
			explicit DecompressionContext(IAllocator* allocator = nullptr);

			//////////////////////////////////////////////////////////////////////////
			// Constructs a context instance from a set of static settings and an optional allocator instance.
			// If an allocator is provided, it will be used in `release()` to free the context
			DecompressionContext(const DecompressionSettingsType& settings, IAllocator* allocator = nullptr);

			//////////////////////////////////////////////////////////////////////////
			// Destructs a context instance
			~DecompressionContext();

			//////////////////////////////////////////////////////////////////////////
			// Initializes the context instance to a particular compressed clip
			void initialize(const CompressedClip& clip, const interpolation_mask* interpolationMask = nullptr);

			bool is_dirty(const CompressedClip& clip);

			//////////////////////////////////////////////////////////////////////////
			// Seeks within the compressed clip to a particular point in time
			void seek(float sample_time, SampleRoundingPolicy rounding_policy);

			//////////////////////////////////////////////////////////////////////////
			// Decompress a full pose at the current sample time.
			// The OutputWriterType allows complete control over how the pose is written out
			template<class OutputWriterType>
			void decompress_pose(OutputWriterType& writer);

			//////////////////////////////////////////////////////////////////////////
			// Decompress a single bone at the current sample time.
			// Each track entry is optional
			void decompress_bone(uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation, Vector4_32* out_scale);

			//////////////////////////////////////////////////////////////////////////
			// Releases the context instance if it contains an allocator reference
			void release();

		private:
			DecompressionContext(const DecompressionContext& other) = delete;
			DecompressionContext& operator=(const DecompressionContext& other) = delete;

			// Internal context data
			impl::DecompressionContext m_context;

			// The static settings used to strip out code at runtime
			DecompressionSettingsType m_settings;

			// The optional allocator instance used to allocate this instance
			IAllocator* m_allocator;
		};

		//////////////////////////////////////////////////////////////////////////
		// Allocates and constructs an instance of the decompression context
		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>* make_decompression_context(IAllocator& allocator)
		{
			return allocate_type<DecompressionContext<DecompressionSettingsType>>(allocator, &allocator);
		}

		//////////////////////////////////////////////////////////////////////////
		// Allocates and constructs an instance of the decompression context
		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>* make_decompression_context(IAllocator& allocator, const DecompressionSettingsType& settings)
		{
			return allocate_type<DecompressionContext<DecompressionSettingsType>>(allocator, settings, &allocator);
		}

		//////////////////////////////////////////////////////////////////////////

		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>::DecompressionContext(IAllocator* allocator)
			: m_context()
			, m_settings()
			, m_allocator(allocator)
		{
			m_context.clip = nullptr;		// Only member used to detect if we are initialized
		}

		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>::DecompressionContext(const DecompressionSettingsType& settings, IAllocator* allocator)
			: m_context()
			, m_settings(settings)
			, m_allocator(allocator)
		{
			m_context.clip = nullptr;		// Only member used to detect if we are initialized
		}

		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>::~DecompressionContext()
		{
			release();
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::initialize(const CompressedClip& clip, const interpolation_mask* interpolationMask)
		{
			ACL_ASSERT(clip.is_valid(false).empty(), "CompressedClip is not valid");
			ACL_ASSERT(clip.get_algorithm_type() == AlgorithmType8::UniformlySampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(AlgorithmType8::UniformlySampled));

			const ClipHeader& header = get_clip_header(clip);

			const RotationFormat8 rotation_format = m_settings.get_rotation_format(header.rotation_format);
			const VectorFormat8 translation_format = m_settings.get_translation_format(header.translation_format);
			const VectorFormat8 scale_format = m_settings.get_scale_format(header.scale_format);

#if defined(ACL_HAS_ASSERT_CHECKS)
			const RangeReductionFlags8 clip_range_reduction = m_settings.get_clip_range_reduction(header.clip_range_reduction);
			const RangeReductionFlags8 segment_range_reduction = m_settings.get_segment_range_reduction(header.segment_range_reduction);

			ACL_ASSERT(rotation_format == header.rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(header.rotation_format));
			ACL_ASSERT(m_settings.is_rotation_format_supported(rotation_format), "Rotation format (%s) isn't statically supported!", get_rotation_format_name(rotation_format));
			ACL_ASSERT(translation_format == header.translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(header.translation_format));
			ACL_ASSERT(m_settings.is_translation_format_supported(translation_format), "Translation format (%s) isn't statically supported!", get_vector_format_name(translation_format));
			ACL_ASSERT(scale_format == header.scale_format, "Statically compiled scale format (%s) differs from the compressed scale format (%s)!", get_vector_format_name(scale_format), get_vector_format_name(header.scale_format));
			ACL_ASSERT(m_settings.is_scale_format_supported(scale_format), "Scale format (%s) isn't statically supported!", get_vector_format_name(scale_format));
			ACL_ASSERT((clip_range_reduction & header.clip_range_reduction) == header.clip_range_reduction, "Statically compiled clip range reduction settings (%u) differs from the compressed settings (%u)!", clip_range_reduction, header.clip_range_reduction);
			ACL_ASSERT(m_settings.are_clip_range_reduction_flags_supported(clip_range_reduction), "Clip range reduction settings (%u) aren't statically supported!", clip_range_reduction);
			ACL_ASSERT((segment_range_reduction & header.segment_range_reduction) == header.segment_range_reduction, "Statically compiled segment range reduction settings (%u) differs from the compressed settings (%u)!", segment_range_reduction, header.segment_range_reduction);
			ACL_ASSERT(m_settings.are_segment_range_reduction_flags_supported(segment_range_reduction), "Segment range reduction settings (%u) aren't statically supported!", segment_range_reduction);
#endif

			m_context.clip = &clip;
			m_context.clip_hash = clip.get_hash();
			m_context.clip_duration = calculate_duration(header.num_samples, header.sample_rate);
			m_context.sample_time = -1.0F;
			m_context.default_tracks_bitset = header.get_default_tracks_bitset();

			m_context.constant_tracks_bitset = header.get_constant_tracks_bitset();
			m_context.constant_track_data = header.get_constant_track_data();
			m_context.clip_range_data = header.get_clip_range_data();

			for (uint8_t key_frame_index = 0; key_frame_index < 2; ++key_frame_index)
			{
				m_context.format_per_track_data[key_frame_index] = nullptr;
				m_context.segment_range_data[key_frame_index] = nullptr;
				m_context.animated_track_data[key_frame_index] = nullptr;
			}

			const uint32_t num_tracks_per_bone = header.has_scale ? 3 : 2;
			m_context.bitset_desc = BitSetDescription::make_from_num_bits(header.num_bones * num_tracks_per_bone);
			m_context.num_rotation_components = rotation_format == RotationFormat8::Quat_128 ? 4 : 3;

			// If all tracks are variable, no need for any extra padding except at the very end of the data
			// If our tracks are mixed variable/not variable, we need to add some padding to ensure alignment
			const bool is_every_format_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
			const bool is_any_format_variable = is_rotation_format_variable(rotation_format) || is_vector_format_variable(translation_format) || is_vector_format_variable(scale_format);
			m_context.has_mixed_packing = !is_every_format_variable && is_any_format_variable;

			m_context.interp_mask = interpolationMask;
		}

		template<class DecompressionSettingsType>
		inline bool DecompressionContext<DecompressionSettingsType>::is_dirty(const CompressedClip& clip)
		{
			if (m_context.clip != &clip)
				return true;

			if (m_context.clip_hash != clip.get_hash())
				return true;

			return false;
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::seek(float sample_time, SampleRoundingPolicy rounding_policy)
		{
			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			// TODO: Make it optional via DecompressionSettingsType?
			sample_time = clamp(sample_time, 0.0F, m_context.clip_duration);

			if (m_context.sample_time == sample_time)
				return;

			m_context.sample_time = sample_time;

			const ClipHeader& header = get_clip_header(*m_context.clip);

			uint32_t key_frame0;
			uint32_t key_frame1;
			find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, key_frame0, key_frame1, m_context.interpolation_alpha);

			uint32_t segment_key_frame0;
			uint32_t segment_key_frame1;

			const SegmentHeader* segment_header0;
			const SegmentHeader* segment_header1;

			const SegmentHeader* segment_headers = header.get_segment_headers();
			const uint32_t num_segments = header.num_segments;

			if (num_segments == 1)
			{
				// Key frame 0 and 1 are in the only segment present
				// This is a really common case and when it happens, we don't store the segment start index (zero)
				segment_header0 = segment_headers;
				segment_key_frame0 = key_frame0;

				segment_header1 = segment_headers;
				segment_key_frame1 = key_frame1;
			}
			else
			{
				const uint32_t* segment_start_indices = header.get_segment_start_indices();

				// See segment_streams(..) for implementation details. This implementation is directly tied to it.
				const uint32_t approx_num_samples_per_segment = header.num_samples / num_segments;	// TODO: Store in header?
				const uint32_t approx_segment_index = key_frame0 / approx_num_samples_per_segment;

				uint32_t segment_index0 = 0;
				uint32_t segment_index1 = 0;

				// Our approximate segment guess is just that, a guess. The actual segments we need could be just before or after.
				// We start looking one segment earlier and up to 2 after. If we have too few segments after, we will hit the
				// sentinel value of 0xFFFFFFFF and exit the loop.
				// TODO: Can we do this with SIMD? Load all 4 values, set key_frame0, compare, move mask, count leading zeroes
				const uint32_t start_segment_index = approx_segment_index > 0 ? (approx_segment_index - 1) : 0;
				const uint32_t end_segment_index = start_segment_index + 4;

				for (uint32_t segment_index = start_segment_index; segment_index < end_segment_index; ++segment_index)
				{
					if (key_frame0 < segment_start_indices[segment_index])
					{
						// We went too far, use previous segment
						ACL_ASSERT(segment_index > 0, "Invalid segment index: %u", segment_index);
						segment_index0 = segment_index - 1;
						segment_index1 = key_frame1 < segment_start_indices[segment_index] ? segment_index0 : segment_index;
						break;
					}
				}

				segment_header0 = segment_headers + segment_index0;
				segment_header1 = segment_headers + segment_index1;

				segment_key_frame0 = key_frame0 - segment_start_indices[segment_index0];
				segment_key_frame1 = key_frame1 - segment_start_indices[segment_index1];
			}

			m_context.format_per_track_data[0] = header.get_format_per_track_data(*segment_header0);
			m_context.format_per_track_data[1] = header.get_format_per_track_data(*segment_header1);
			m_context.segment_range_data[0] = header.get_segment_range_data(*segment_header0);
			m_context.segment_range_data[1] = header.get_segment_range_data(*segment_header1);
			m_context.animated_track_data[0] = header.get_track_data(*segment_header0);
			m_context.animated_track_data[1] = header.get_track_data(*segment_header1);

			m_context.key_frame_byte_offsets[0] = (segment_key_frame0 * segment_header0->animated_pose_bit_size) / 8;
			m_context.key_frame_byte_offsets[1] = (segment_key_frame1 * segment_header1->animated_pose_bit_size) / 8;
			m_context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			m_context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;
		}

		template<class DecompressionSettingsType>
		template<class OutputWriterType>
		inline void DecompressionContext<DecompressionSettingsType>::decompress_pose(OutputWriterType& writer)
		{
			static_assert(std::is_base_of<OutputWriter, OutputWriterType>::value, "OutputWriterType must derive from OutputWriter");

			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");
			ACL_ASSERT(m_context.sample_time >= 0.0f, "Context not set to a valid sample time");

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (m_settings.disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const ClipHeader& header = get_clip_header(*m_context.clip);

			const impl::TranslationDecompressionSettingsAdapter<DecompressionSettingsType> translation_adapter(m_settings);
			const impl::ScaleDecompressionSettingsAdapter<DecompressionSettingsType> scale_adapter(m_settings, header);

			impl::SamplingContext sampling_context;
			sampling_context.track_index = 0;
			sampling_context.constant_track_data_offset = 0;
			sampling_context.clip_range_data_offset = 0;
			sampling_context.format_per_track_data_offset = 0;
			sampling_context.segment_range_data_offset = 0;
			sampling_context.key_frame_byte_offsets[0] = m_context.key_frame_byte_offsets[0];
			sampling_context.key_frame_byte_offsets[1] = m_context.key_frame_byte_offsets[1];
			sampling_context.key_frame_bit_offsets[0] = m_context.key_frame_bit_offsets[0];
			sampling_context.key_frame_bit_offsets[1] = m_context.key_frame_bit_offsets[1];

			const uint16_t num_bones = header.num_bones;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				float interpolation_alpha = m_context.interpolation_alpha;
				if (m_context.interp_mask)
				{
					SampleRoundingPolicy const rounding_policy = m_context.interp_mask->get(bone_index);
					interpolation_alpha = apply_rounding_policy(interpolation_alpha, rounding_policy);
				}

				if (writer.skip_all_bone_rotations() || writer.skip_bone_rotation(bone_index))
					skip_over_rotation(m_settings, header, m_context, sampling_context);
				else
				{
					const Quat_32 rotation = decompress_and_interpolate_rotation(m_settings, header, m_context, interpolation_alpha, sampling_context);
					writer.write_bone_rotation(bone_index, rotation);
				}

				if (writer.skip_all_bone_translations() || writer.skip_bone_translation(bone_index))
					skip_over_vector(translation_adapter, header, m_context, sampling_context);
				else
				{
					const Vector4_32 translation = decompress_and_interpolate_vector(translation_adapter, header, m_context, interpolation_alpha, sampling_context);
					writer.write_bone_translation(bone_index, translation);
				}

				if (writer.skip_all_bone_scales() || writer.skip_bone_scale(bone_index))
				{
					if (header.has_scale)
						skip_over_vector(scale_adapter, header, m_context, sampling_context);
				}
				else
				{
					const Vector4_32 scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, m_context, interpolation_alpha, sampling_context) : scale_adapter.get_default_value();
					writer.write_bone_scale(bone_index, scale);
				}
			}

			if (m_settings.disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::decompress_bone(uint16_t sample_bone_index, Quat_32* out_rotation, Vector4_32* out_translation, Vector4_32* out_scale)
		{
			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");
			ACL_ASSERT(m_context.sample_time >= 0.0f, "Context not set to a valid sample time");

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (m_settings.disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			float interpolation_alpha = m_context.interpolation_alpha;
			if (m_context.interp_mask)
			{
				SampleRoundingPolicy const rounding_policy = m_context.interp_mask->get(sample_bone_index);
				interpolation_alpha = apply_rounding_policy(interpolation_alpha, rounding_policy);
			}

			const ClipHeader& header = get_clip_header(*m_context.clip);

			const impl::TranslationDecompressionSettingsAdapter<DecompressionSettingsType> translation_adapter(m_settings);
			const impl::ScaleDecompressionSettingsAdapter<DecompressionSettingsType> scale_adapter(m_settings, header);

			impl::SamplingContext sampling_context;
			sampling_context.key_frame_byte_offsets[0] = m_context.key_frame_byte_offsets[0];
			sampling_context.key_frame_byte_offsets[1] = m_context.key_frame_byte_offsets[1];
			sampling_context.key_frame_bit_offsets[0] = m_context.key_frame_bit_offsets[0];
			sampling_context.key_frame_bit_offsets[1] = m_context.key_frame_bit_offsets[1];

			const RotationFormat8 rotation_format = m_settings.get_rotation_format(header.rotation_format);
			const VectorFormat8 translation_format = m_settings.get_translation_format(header.translation_format);
			const VectorFormat8 scale_format = m_settings.get_scale_format(header.scale_format);

			const bool are_all_tracks_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
			const bool has_mixed_padding_or_fixed_quantization = (m_settings.supports_mixed_packing() && m_context.has_mixed_packing) || !are_all_tracks_variable;
			if (has_mixed_padding_or_fixed_quantization)
			{
				// Slow path, not optimized yet because it's more complex and shouldn't be used in production anyway
				sampling_context.track_index = 0;
				sampling_context.constant_track_data_offset = 0;
				sampling_context.clip_range_data_offset = 0;
				sampling_context.format_per_track_data_offset = 0;
				sampling_context.segment_range_data_offset = 0;

				for (uint16_t bone_index = 0; bone_index < sample_bone_index; ++bone_index)
				{
					skip_over_rotation(m_settings, header, m_context, sampling_context);
					skip_over_vector(translation_adapter, header, m_context, sampling_context);

					if (header.has_scale)
						skip_over_vector(scale_adapter, header, m_context, sampling_context);
				}
			}
			else
			{
				const uint32_t num_tracks_per_bone = header.has_scale ? 3 : 2;
				const uint32_t track_index = sample_bone_index * num_tracks_per_bone;
				uint32_t num_default_rotations = 0;
				uint32_t num_default_translations = 0;
				uint32_t num_default_scales = 0;
				uint32_t num_constant_rotations = 0;
				uint32_t num_constant_translations = 0;
				uint32_t num_constant_scales = 0;

				if (header.has_scale)
				{
					uint32_t rotation_track_bit_mask = 0x92492492;		// b100100100..
					uint32_t translation_track_bit_mask = 0x49249249;	// b010010010..
					uint32_t scale_track_bit_mask = 0x24924924;			// b001001001..

					const uint32_t last_offset = track_index / 32;
					uint32_t offset = 0;
					for (; offset < last_offset; ++offset)
					{
						const uint32_t default_value = m_context.default_tracks_bitset[offset];
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
						num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

						const uint32_t constant_value = m_context.constant_tracks_bitset[offset];
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
						num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);

						// Because the number of tracks in a 32 bit value isn't a multiple of the number of tracks we have (3),
						// we have to cycle the masks. There are 3 possible masks, just swap them.
						const uint32_t old_rotation_track_bit_mask = rotation_track_bit_mask;
						rotation_track_bit_mask = translation_track_bit_mask;
						translation_track_bit_mask = scale_track_bit_mask;
						scale_track_bit_mask = old_rotation_track_bit_mask;
					}

					const uint32_t remaining_tracks = track_index % 32;
					if (remaining_tracks != 0)
					{
						const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
						const uint32_t default_value = and_not(not_up_to_track_mask, m_context.default_tracks_bitset[offset]);
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);
						num_default_scales += count_set_bits(default_value & scale_track_bit_mask);

						const uint32_t constant_value = and_not(not_up_to_track_mask, m_context.constant_tracks_bitset[offset]);
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
						num_constant_scales += count_set_bits(constant_value & scale_track_bit_mask);
					}
				}
				else
				{
					const uint32_t rotation_track_bit_mask = 0xAAAAAAAA;		// b10101010..
					const uint32_t translation_track_bit_mask = 0x55555555;		// b01010101..

					const uint32_t last_offset = track_index / 32;
					uint32_t offset = 0;
					for (; offset < last_offset; ++offset)
					{
						const uint32_t default_value = m_context.default_tracks_bitset[offset];
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

						const uint32_t constant_value = m_context.constant_tracks_bitset[offset];
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					}

					const uint32_t remaining_tracks = track_index % 32;
					if (remaining_tracks != 0)
					{
						const uint32_t not_up_to_track_mask = ((1 << (32 - remaining_tracks)) - 1);
						const uint32_t default_value = and_not(not_up_to_track_mask, m_context.default_tracks_bitset[offset]);
						num_default_rotations += count_set_bits(default_value & rotation_track_bit_mask);
						num_default_translations += count_set_bits(default_value & translation_track_bit_mask);

						const uint32_t constant_value = and_not(not_up_to_track_mask, m_context.constant_tracks_bitset[offset]);
						num_constant_rotations += count_set_bits(constant_value & rotation_track_bit_mask);
						num_constant_translations += count_set_bits(constant_value & translation_track_bit_mask);
					}
				}

				// Tracks that are default are also constant
				const uint32_t num_animated_rotations = sample_bone_index - num_constant_rotations;
				const uint32_t num_animated_translations = sample_bone_index - num_constant_translations;

				const RotationFormat8 packed_rotation_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				const uint32_t packed_rotation_size = get_packed_rotation_size(packed_rotation_format);

				uint32_t constant_track_data_offset = (num_constant_rotations - num_default_rotations) * packed_rotation_size;
				constant_track_data_offset += (num_constant_translations - num_default_translations) * get_packed_vector_size(VectorFormat8::Vector3_96);

				uint32_t clip_range_data_offset = 0;
				uint32_t segment_range_data_offset = 0;

				const RangeReductionFlags8 clip_range_reduction = m_settings.get_clip_range_reduction(header.clip_range_reduction);
				const RangeReductionFlags8 segment_range_reduction = m_settings.get_segment_range_reduction(header.segment_range_reduction);
				if (are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Rotations))
					clip_range_data_offset += m_context.num_rotation_components * sizeof(float) * 2 * num_animated_rotations;

				if (are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Rotations))
					segment_range_data_offset += m_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_rotations;

				if (are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Translations))
					clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_translations;

				if (are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Translations))
					segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_translations;

				uint32_t num_animated_tracks = num_animated_rotations + num_animated_translations;
				if (header.has_scale)
				{
					const uint32_t num_animated_scales = sample_bone_index - num_constant_scales;
					num_animated_tracks += num_animated_scales;

					constant_track_data_offset += (num_constant_scales - num_default_scales) * get_packed_vector_size(VectorFormat8::Vector3_96);

					if (are_any_enum_flags_set(clip_range_reduction, RangeReductionFlags8::Scales))
						clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_scales;

					if (are_any_enum_flags_set(segment_range_reduction, RangeReductionFlags8::Scales))
						segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_scales;
				}

				sampling_context.track_index = track_index;
				sampling_context.constant_track_data_offset = constant_track_data_offset;
				sampling_context.clip_range_data_offset = clip_range_data_offset;
				sampling_context.segment_range_data_offset = segment_range_data_offset;
				sampling_context.format_per_track_data_offset = num_animated_tracks;

				for (uint32_t animated_track_index = 0; animated_track_index < num_animated_tracks; ++animated_track_index)
				{
					for (size_t i = 0; i < 2; ++i)
					{
						const uint8_t bit_rate = m_context.format_per_track_data[i][animated_track_index];
						const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate) * 3;	// 3 components

						sampling_context.key_frame_bit_offsets[i] += num_bits_at_bit_rate;
					}
				}
			}

			if (out_rotation != nullptr)
				*out_rotation = decompress_and_interpolate_rotation(m_settings, header, m_context, interpolation_alpha, sampling_context);
			else
				skip_over_rotation(m_settings, header, m_context, sampling_context);

			if (out_translation != nullptr)
				*out_translation = decompress_and_interpolate_vector(translation_adapter, header, m_context, interpolation_alpha, sampling_context);
			else if (out_scale != nullptr && header.has_scale)
			{
				// We'll need to read the scale value that follows, skip the translation we don't need
				skip_over_vector(translation_adapter, header, m_context, sampling_context);
			}

			if (out_scale != nullptr)
				*out_scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, m_context, interpolation_alpha, sampling_context) : scale_adapter.get_default_value();
			// No need to skip our last scale, we don't care anymore

			if (m_settings.disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::release()
		{
			IAllocator* allocator = m_allocator;
			if (allocator != nullptr)
			{
				m_allocator = nullptr;
				deallocate_type<DecompressionContext>(*allocator, this);
			}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
