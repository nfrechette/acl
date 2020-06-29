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
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/compressed_clip.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/utils.h"
#include "acl/math/quat_packing.h"
#include "acl/decompression/impl/decompress_data.h"
#include "acl/decompression/output_writer.h"

#include <rtm/quatf.h>
#include <rtm/vector4f.h>

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

		namespace acl_impl
		{
			RTM_DISABLE_SECURITY_COOKIE_CHECK inline rtm::quatf RTM_SIMD_CALL quat_lerp_no_normalization(rtm::quatf_arg0 start, rtm::quatf_arg1 end, float alpha) RTM_NO_EXCEPT
			{
				using namespace rtm;

#if defined(RTM_SSE2_INTRINSICS)
				// Calculate the vector4 dot product: dot(start, end)
				__m128 dot;
#if defined(RTM_SSE4_INTRINSICS)
				// The dpps instruction isn't as accurate but we don't care here, we only need the sign of the
				// dot product. If both rotations are on opposite ends of the hypersphere, the result will be
				// very negative. If we are on the edge, the rotations are nearly opposite but not quite which
				// means that the linear interpolation here will have terrible accuracy to begin with. It is designed
				// for interpolating rotations that are reasonably close together. The bias check is mainly necessary
				// because the W component is often kept positive which flips the sign.
				// Using the dpps instruction reduces the number of registers that we need and helps the function get
				// inlined.
				dot = _mm_dp_ps(start, end, 0xFF);
#else
				{
					__m128 x2_y2_z2_w2 = _mm_mul_ps(start, end);
					__m128 z2_w2_0_0 = _mm_shuffle_ps(x2_y2_z2_w2, x2_y2_z2_w2, _MM_SHUFFLE(0, 0, 3, 2));
					__m128 x2z2_y2w2_0_0 = _mm_add_ps(x2_y2_z2_w2, z2_w2_0_0);
					__m128 y2w2_0_0_0 = _mm_shuffle_ps(x2z2_y2w2_0_0, x2z2_y2w2_0_0, _MM_SHUFFLE(0, 0, 0, 1));
					__m128 x2y2z2w2_0_0_0 = _mm_add_ps(x2z2_y2w2_0_0, y2w2_0_0_0);
					// Shuffle the dot product to all SIMD lanes, there is no _mm_and_ss and loading
					// the constant from memory with the 'and' instruction is faster, it uses fewer registers
					// and fewer instructions
					dot = _mm_shuffle_ps(x2y2z2w2_0_0_0, x2y2z2w2_0_0_0, _MM_SHUFFLE(0, 0, 0, 0));
				}
#endif

				// Calculate the bias, if the dot product is positive or zero, there is no bias
				// but if it is negative, we want to flip the 'end' rotation XYZW components
				__m128 bias = _mm_and_ps(dot, _mm_set_ps1(-0.0F));

				// Lerp the rotation after applying the bias
				// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
				__m128 alpha_ = _mm_set_ps1(alpha);
				__m128 interpolated_rotation = _mm_add_ps(_mm_sub_ps(start, _mm_mul_ps(alpha_, start)), _mm_mul_ps(alpha_, _mm_xor_ps(end, bias)));

				// Due to the interpolation, the result might not be anywhere near normalized!
				// Make sure to normalize afterwards before using
				return interpolated_rotation;
#elif defined (RTM_NEON64_INTRINSICS)
				// On ARM64 with NEON, we load 1.0 once and use it twice which is faster than
				// using a AND/XOR with the bias (same number of instructions)
				float dot = vector_dot(start, end);
				float bias = dot >= 0.0F ? 1.0F : -1.0F;

				// ((1.0 - alpha) * start) + (alpha * (end * bias)) == (start - alpha * start) + (alpha * (end * bias))
				vector4f interpolated_rotation = vector_mul_add(vector_mul(end, bias), alpha, vector_neg_mul_sub(start, alpha, start));

				// Due to the interpolation, the result might not be anywhere near normalized!
				// Make sure to normalize afterwards before using
				return interpolated_rotation;
#elif defined(RTM_NEON_INTRINSICS)
				// Calculate the vector4 dot product: dot(start, end)
				float32x4_t x2_y2_z2_w2 = vmulq_f32(start, end);
				float32x2_t x2_y2 = vget_low_f32(x2_y2_z2_w2);
				float32x2_t z2_w2 = vget_high_f32(x2_y2_z2_w2);
				float32x2_t x2z2_y2w2 = vadd_f32(x2_y2, z2_w2);
				float32x2_t x2y2z2w2 = vpadd_f32(x2z2_y2w2, x2z2_y2w2);

				// Calculate the bias, if the dot product is positive or zero, there is no bias
				// but if it is negative, we want to flip the 'end' rotation XYZW components
				// On ARM-v7-A, the AND/XOR trick is faster than the cmp/fsel
				uint32x2_t bias = vand_u32(vreinterpret_u32_f32(x2y2z2w2), vdup_n_u32(0x80000000));

				// Lerp the rotation after applying the bias
				// ((1.0 - alpha) * start) + (alpha * (end ^ bias)) == (start - alpha * start) + (alpha * (end ^ bias))
				float32x4_t end_biased = vreinterpretq_f32_u32(veorq_u32(vreinterpretq_u32_f32(end), vcombine_u32(bias, bias)));
				float32x4_t interpolated_rotation = vmlaq_n_f32(vmlsq_n_f32(start, start, alpha), end_biased, alpha);

				// Due to the interpolation, the result might not be anywhere near normalized!
				// Make sure to normalize afterwards before using
				return interpolated_rotation;
#else
				// To ensure we take the shortest path, we apply a bias if the dot product is negative
				vector4f start_vector = quat_to_vector(start);
				vector4f end_vector = quat_to_vector(end);
				float dot = vector_dot(start_vector, end_vector);
				float bias = dot >= 0.0F ? 1.0F : -1.0F;
				// ((1.0 - alpha) * start) + (alpha * (end * bias)) == (start - alpha * start) + (alpha * (end * bias))
				vector4f interpolated_rotation = vector_mul_add(vector_mul(end_vector, bias), alpha, vector_neg_mul_sub(start_vector, alpha, start_vector));

				// Due to the interpolation, the result might not be anywhere near normalized!
				// Make sure to normalize afterwards before using
				return vector_to_quat(interpolated_rotation);
#endif
			}

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

				range_reduction_flags8 range_reduction;			//  32 |  52
				uint8_t num_rotation_components;				//  33 |  53

				uint8_t padding0[2];							//  34 |  54

				// Seeking related data
				const uint8_t* format_per_track_data[2];		//  36 |  56
				const uint8_t* segment_range_data[2];			//  44 |  72
				const uint8_t* animated_track_data[2];			//  52 |  88

				uint32_t key_frame_bit_offsets[2];				//  60 | 104

				float interpolation_alpha;						//  68 | 112
				float sample_time;								//  76 | 120

				uint8_t padding1[sizeof(void*) == 4 ? 52 : 4];	//  80 | 124

				//									Total size:	   128 | 128
			};

			static_assert(sizeof(DecompressionContext) == 128, "Unexpected size");

			struct alignas(k_cache_line_size) SamplingContext
			{
				static constexpr size_t k_num_samples_to_interpolate = 2;

				inline static rtm::quatf RTM_SIMD_CALL interpolate_rotation(rtm::quatf_arg0 rotation0, rtm::quatf_arg1 rotation1, float interpolation_alpha)
				{
					return rtm::quat_lerp(rotation0, rotation1, interpolation_alpha);
				}

				inline static rtm::quatf RTM_SIMD_CALL interpolate_rotation_no_normalization(rtm::quatf_arg0 rotation0, rtm::quatf_arg1 rotation1, float interpolation_alpha)
				{
					return quat_lerp_no_normalization(rotation0, rotation1, interpolation_alpha);
				}

				inline static rtm::quatf RTM_SIMD_CALL interpolate_rotation(rtm::quatf_arg0 rotation0, rtm::quatf_arg1 rotation1, rtm::quatf_arg2 rotation2, rtm::quatf_arg3 rotation3, float interpolation_alpha)
				{
					(void)rotation1;
					(void)rotation2;
					(void)rotation3;
					(void)interpolation_alpha;
					return rotation0;	// Not implemented, we use linear interpolation
				}

				inline static rtm::vector4f RTM_SIMD_CALL interpolate_vector4(rtm::vector4f_arg0 vector0, rtm::vector4f_arg1 vector1, float interpolation_alpha)
				{
					return rtm::vector_lerp(vector0, vector1, interpolation_alpha);
				}

				inline static rtm::vector4f RTM_SIMD_CALL interpolate_vector4(rtm::vector4f_arg0 vector0, rtm::vector4f_arg1 vector1, rtm::vector4f_arg2 vector2, rtm::vector4f_arg3 vector3, float interpolation_alpha)
				{
					(void)vector1;
					(void)vector2;
					(void)vector3;
					(void)interpolation_alpha;
					return vector0;		// Not implemented, we use linear interpolation
				}

				//														//   offsets
				uint32_t track_index;									//   0 |   0
				uint32_t constant_track_data_offset;					//   4 |   4
				uint32_t clip_range_data_offset;						//   8 |   8

				uint32_t format_per_track_data_offset;					//  12 |  12
				uint32_t segment_range_data_offset;						//  16 |  16

				uint32_t key_frame_bit_offsets[2];						//  20 |  20

				uint8_t padding[4];										//  28 |  28

				rtm::vector4f vectors[k_num_samples_to_interpolate];	//  32 |  32

				//											Total size:	    64 |  64
			};

			static_assert(sizeof(SamplingContext) == 64, "Unexpected size");

			// We use adapters to wrap the DecompressionSettings
			// This allows us to re-use the code for skipping and decompressing Vector3 samples
			// Code generation will generate specialized code for each specialization
			template<class SettingsType>
			struct TranslationDecompressionSettingsAdapter
			{
				explicit TranslationDecompressionSettingsAdapter(const SettingsType& settings_) : settings(settings_) {}

				constexpr range_reduction_flags8 get_range_reduction_flag() const { return range_reduction_flags8::translations; }
				rtm::vector4f RTM_SIMD_CALL get_default_value() const { return rtm::vector_zero(); }
				vector_format8 get_vector_format(const ClipHeader& header) const { return settings.get_translation_format(header.translation_format); }
				bool is_vector_format_supported(vector_format8 format) const { return settings.is_translation_format_supported(format); }
				bool are_range_reduction_flags_supported(range_reduction_flags8 flags) const { return settings.are_range_reduction_flags_supported(flags); }

				SettingsType settings;
			};

			template<class SettingsType>
			struct ScaleDecompressionSettingsAdapter
			{
				explicit ScaleDecompressionSettingsAdapter(const SettingsType& settings_, const ClipHeader& header)
					: settings(settings_)
					, default_scale(header.default_scale ? rtm::vector_set(1.0F) : rtm::vector_zero())
				{}

				constexpr range_reduction_flags8 get_range_reduction_flag() const { return range_reduction_flags8::scales; }
				rtm::vector4f RTM_SIMD_CALL get_default_value() const { return default_scale; }
				vector_format8 get_vector_format(const ClipHeader& header) const { return settings.get_scale_format(header.scale_format); }
				bool is_vector_format_supported(vector_format8 format) const { return settings.is_scale_format_supported(format); }
				bool are_range_reduction_flags_supported(range_reduction_flags8 flags) const { return settings.are_range_reduction_flags_supported(flags); }

				SettingsType settings;
				uint8_t padding[get_required_padding<SettingsType, rtm::vector4f>()];
				rtm::vector4f default_scale;
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
			constexpr bool is_rotation_format_supported(rotation_format8 /*format*/) const { return true; }
			constexpr bool is_translation_format_supported(vector_format8 /*format*/) const { return true; }
			constexpr bool is_scale_format_supported(vector_format8 /*format*/) const { return true; }
			constexpr rotation_format8 get_rotation_format(rotation_format8 format) const { return format; }
			constexpr vector_format8 get_translation_format(vector_format8 format) const { return format; }
			constexpr vector_format8 get_scale_format(vector_format8 format) const { return format; }

			constexpr bool are_range_reduction_flags_supported(range_reduction_flags8 /*flags*/) const { return true; }
			constexpr range_reduction_flags8 get_range_reduction(range_reduction_flags8 flags) const { return flags; }

			// Whether to explicitly disable floating point exceptions during decompression.
			// This has a cost, exceptions are usually disabled globally and do not need to be
			// explicitly disabled during decompression.
			// We assume that floating point exceptions are already disabled by the caller.
			constexpr bool disable_fp_exeptions() const { return false; }

			// Whether rotations should be normalized before being output or not. Some animation
			// runtimes will normalize in a separate step and do not need the explicit normalization.
			// Enabled by default for safety.
			constexpr bool normalize_rotations() const { return true; }
		};

		//////////////////////////////////////////////////////////////////////////
		// These are debug settings, everything is enabled and nothing is stripped.
		// It will have the worst performance but allows every feature.
		//////////////////////////////////////////////////////////////////////////
		struct DebugDecompressionSettings : public DecompressionSettings {};

		//////////////////////////////////////////////////////////////////////////
		// These are the default settings. Only the generally optimal settings
		// are enabled and will offer the overall best performance.
		//
		// Note: Segment range reduction supports all_tracks or none because it can
		// be disabled if there is a single segment.
		//////////////////////////////////////////////////////////////////////////
		struct DefaultDecompressionSettings : public DecompressionSettings
		{
			constexpr bool is_rotation_format_supported(rotation_format8 format) const { return format == rotation_format8::quatf_drop_w_variable; }
			constexpr bool is_translation_format_supported(vector_format8 format) const { return format == vector_format8::vector3f_variable; }
			constexpr bool is_scale_format_supported(vector_format8 format) const { return format == vector_format8::vector3f_variable; }
			constexpr rotation_format8 get_rotation_format(rotation_format8 /*format*/) const { return rotation_format8::quatf_drop_w_variable; }
			constexpr vector_format8 get_translation_format(vector_format8 /*format*/) const { return vector_format8::vector3f_variable; }
			constexpr vector_format8 get_scale_format(vector_format8 /*format*/) const { return vector_format8::vector3f_variable; }

			constexpr range_reduction_flags8 get_range_reduction(range_reduction_flags8 /*flags*/) const { return range_reduction_flags8::all_tracks; }
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
		public:
			static_assert(std::is_base_of<DecompressionSettings, DecompressionSettingsType>::value, "DecompressionSettingsType must derive from DecompressionSettings!");

			//////////////////////////////////////////////////////////////////////////
			// An alias to the decompression settings type.
			using SettingsType = DecompressionSettingsType;

			//////////////////////////////////////////////////////////////////////////
			// Constructs a context instance.
			// The default constructor for the DecompressionSettingsType will be used.
			DecompressionContext();

			//////////////////////////////////////////////////////////////////////////
			// Constructs a context instance from a settings instance.
			DecompressionContext(const DecompressionSettingsType& settings);

			//////////////////////////////////////////////////////////////////////////
			// Returns the compressed clip bound to this context instance.
			const CompressedClip* get_compressed_clip() const { return m_context.clip; }

			//////////////////////////////////////////////////////////////////////////
			// Initializes the context instance to a particular compressed clip
			void initialize(const CompressedClip& clip);

			//////////////////////////////////////////////////////////////////////////
			// Returns true if this context instance is bound to a compressed clip, false otherwise.
			bool is_initialized() const { return m_context.clip != nullptr; }

			//////////////////////////////////////////////////////////////////////////
			// Returns true if this context instance is bound to the specified compressed clip, false otherwise.
			bool is_dirty(const CompressedClip& clip);

			//////////////////////////////////////////////////////////////////////////
			// Seeks within the compressed clip to a particular point in time
			void seek(float sample_time, sample_rounding_policy rounding_policy);

			//////////////////////////////////////////////////////////////////////////
			// Decompress a full pose at the current sample time.
			// The OutputWriterType allows complete control over how the pose is written out
			template<class OutputWriterType>
			void decompress_pose(OutputWriterType& writer);

			//////////////////////////////////////////////////////////////////////////
			// Decompress a single bone at the current sample time.
			// Each track entry is optional
			void decompress_bone(uint16_t sample_bone_index, rtm::quatf* out_rotation, rtm::vector4f* out_translation, rtm::vector4f* out_scale);

		private:
			DecompressionContext(const DecompressionContext& other) = delete;
			DecompressionContext& operator=(const DecompressionContext& other) = delete;

			// Internal context data
			acl_impl::DecompressionContext m_context;

			// The static settings used to strip out code at runtime
			DecompressionSettingsType m_settings;

			// Ensure we have non-zero padding to avoid compiler warnings
			static constexpr size_t k_padding_size = alignof(acl_impl::DecompressionContext) - sizeof(DecompressionSettingsType);
			uint8_t m_padding[k_padding_size != 0 ? k_padding_size : alignof(acl_impl::DecompressionContext)];
		};

		//////////////////////////////////////////////////////////////////////////
		// Allocates and constructs an instance of the decompression context
		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>* make_decompression_context(IAllocator& allocator)
		{
			return allocate_type<DecompressionContext<DecompressionSettingsType>>(allocator);
		}

		//////////////////////////////////////////////////////////////////////////
		// Allocates and constructs an instance of the decompression context
		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>* make_decompression_context(IAllocator& allocator, const DecompressionSettingsType& settings)
		{
			return allocate_type<DecompressionContext<DecompressionSettingsType>>(allocator, settings);
		}

		//////////////////////////////////////////////////////////////////////////

		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>::DecompressionContext()
			: m_context()
			, m_settings()
		{
			m_context.clip = nullptr;		// Only member used to detect if we are initialized
		}

		template<class DecompressionSettingsType>
		inline DecompressionContext<DecompressionSettingsType>::DecompressionContext(const DecompressionSettingsType& settings)
			: m_context()
			, m_settings(settings)
		{
			m_context.clip = nullptr;		// Only member used to detect if we are initialized
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::initialize(const CompressedClip& clip)
		{
			ACL_ASSERT(clip.is_valid(false).empty(), "CompressedClip is not valid");
			ACL_ASSERT(clip.get_algorithm_type() == algorithm_type8::uniformly_sampled, "Invalid algorithm type [%s], expected [%s]", get_algorithm_name(clip.get_algorithm_type()), get_algorithm_name(algorithm_type8::uniformly_sampled));

			const ClipHeader& header = get_clip_header(clip);

			const rotation_format8 rotation_format = m_settings.get_rotation_format(header.rotation_format);
			const vector_format8 translation_format = m_settings.get_translation_format(header.translation_format);
			const vector_format8 scale_format = m_settings.get_scale_format(header.scale_format);

			ACL_ASSERT(rotation_format == header.rotation_format, "Statically compiled rotation format (%s) differs from the compressed rotation format (%s)!", get_rotation_format_name(rotation_format), get_rotation_format_name(header.rotation_format));
			ACL_ASSERT(m_settings.is_rotation_format_supported(rotation_format), "Rotation format (%s) isn't statically supported!", get_rotation_format_name(rotation_format));
			ACL_ASSERT(translation_format == header.translation_format, "Statically compiled translation format (%s) differs from the compressed translation format (%s)!", get_vector_format_name(translation_format), get_vector_format_name(header.translation_format));
			ACL_ASSERT(m_settings.is_translation_format_supported(translation_format), "Translation format (%s) isn't statically supported!", get_vector_format_name(translation_format));
			ACL_ASSERT(scale_format == header.scale_format, "Statically compiled scale format (%s) differs from the compressed scale format (%s)!", get_vector_format_name(scale_format), get_vector_format_name(header.scale_format));
			ACL_ASSERT(m_settings.is_scale_format_supported(scale_format), "Scale format (%s) isn't statically supported!", get_vector_format_name(scale_format));

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

			range_reduction_flags8 range_reduction = range_reduction_flags8::none;
			if (is_rotation_format_variable(rotation_format))
				range_reduction |= range_reduction_flags8::rotations;
			if (is_vector_format_variable(translation_format))
				range_reduction |= range_reduction_flags8::translations;
			if (is_vector_format_variable(scale_format))
				range_reduction |= range_reduction_flags8::scales;

			m_context.range_reduction = m_settings.get_range_reduction(range_reduction);

			ACL_ASSERT((m_context.range_reduction & range_reduction) == range_reduction, "Statically compiled range reduction flags (%u) differ from the compressed flags (%u)!", static_cast< uint32_t >( m_context.range_reduction ), static_cast< uint32_t >( range_reduction ) );
			ACL_ASSERT(m_settings.are_range_reduction_flags_supported(m_context.range_reduction), "Range reduction flags (%u) aren't statically supported!", static_cast< uint32_t >( m_context.range_reduction ) );

			m_context.num_rotation_components = rotation_format == rotation_format8::quatf_full ? 4 : 3;
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
		inline void DecompressionContext<DecompressionSettingsType>::seek(float sample_time, sample_rounding_policy rounding_policy)
		{
			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			// TODO: Make it optional via DecompressionSettingsType?
			sample_time = rtm::scalar_clamp(sample_time, 0.0F, m_context.clip_duration);

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

			m_context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			m_context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;
		}

		template<class DecompressionSettingsType>
		template<class OutputWriterType>
		inline void DecompressionContext<DecompressionSettingsType>::decompress_pose(OutputWriterType& writer)
		{
			static_assert(std::is_base_of<OutputWriter, OutputWriterType>::value, "OutputWriterType must derive from OutputWriter");

			using namespace acl::acl_impl;

			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");
			ACL_ASSERT(m_context.sample_time >= 0.0f, "Context not set to a valid sample time");

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (m_settings.disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const ClipHeader& header = get_clip_header(*m_context.clip);

			const acl_impl::TranslationDecompressionSettingsAdapter<DecompressionSettingsType> translation_adapter(m_settings);
			const acl_impl::ScaleDecompressionSettingsAdapter<DecompressionSettingsType> scale_adapter(m_settings, header);

			const rtm::vector4f default_scale = scale_adapter.get_default_value();

			acl_impl::SamplingContext sampling_context;
			sampling_context.track_index = 0;
			sampling_context.constant_track_data_offset = 0;
			sampling_context.clip_range_data_offset = 0;
			sampling_context.format_per_track_data_offset = 0;
			sampling_context.segment_range_data_offset = 0;
			sampling_context.key_frame_bit_offsets[0] = m_context.key_frame_bit_offsets[0];
			sampling_context.key_frame_bit_offsets[1] = m_context.key_frame_bit_offsets[1];

			sampling_context.vectors[0] = default_scale;	// Init with something to avoid GCC warning
			sampling_context.vectors[1] = default_scale;	// Init with something to avoid GCC warning

			const uint16_t num_bones = header.num_bones;
			for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
			{
				if (writer.skip_all_bone_rotations() || writer.skip_bone_rotation(bone_index))
					skip_over_rotation(m_settings, header, m_context, sampling_context);
				else
				{
					const rtm::quatf rotation = decompress_and_interpolate_rotation(m_settings, header, m_context, sampling_context);
					writer.write_bone_rotation(bone_index, rotation);
				}

				if (writer.skip_all_bone_translations() || writer.skip_bone_translation(bone_index))
					skip_over_vector(translation_adapter, header, m_context, sampling_context);
				else
				{
					const rtm::vector4f translation = decompress_and_interpolate_vector(translation_adapter, header, m_context, sampling_context);
					writer.write_bone_translation(bone_index, translation);
				}

				if (writer.skip_all_bone_scales() || writer.skip_bone_scale(bone_index))
				{
					if (header.has_scale)
						skip_over_vector(scale_adapter, header, m_context, sampling_context);
				}
				else
				{
					const rtm::vector4f scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, m_context, sampling_context) : default_scale;
					writer.write_bone_scale(bone_index, scale);
				}
			}

			if (m_settings.disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		template<class DecompressionSettingsType>
		inline void DecompressionContext<DecompressionSettingsType>::decompress_bone(uint16_t sample_bone_index, rtm::quatf* out_rotation, rtm::vector4f* out_translation, rtm::vector4f* out_scale)
		{
			using namespace acl::acl_impl;

			ACL_ASSERT(m_context.clip != nullptr, "Context is not initialized");
			ACL_ASSERT(m_context.sample_time >= 0.0f, "Context not set to a valid sample time");

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (m_settings.disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			const ClipHeader& header = get_clip_header(*m_context.clip);

			const acl_impl::TranslationDecompressionSettingsAdapter<DecompressionSettingsType> translation_adapter(m_settings);
			const acl_impl::ScaleDecompressionSettingsAdapter<DecompressionSettingsType> scale_adapter(m_settings, header);

			acl_impl::SamplingContext sampling_context;
			sampling_context.key_frame_bit_offsets[0] = m_context.key_frame_bit_offsets[0];
			sampling_context.key_frame_bit_offsets[1] = m_context.key_frame_bit_offsets[1];

			const rotation_format8 rotation_format = m_settings.get_rotation_format(header.rotation_format);
			const vector_format8 translation_format = m_settings.get_translation_format(header.translation_format);
			const vector_format8 scale_format = m_settings.get_scale_format(header.scale_format);

			const bool are_all_tracks_variable = is_rotation_format_variable(rotation_format) && is_vector_format_variable(translation_format) && is_vector_format_variable(scale_format);
			if (!are_all_tracks_variable)
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

						// Because the number of tracks in a 32bit word isn't a multiple of the number of tracks we have (3),
						// we have to rotate the masks left
						rotation_track_bit_mask = rotate_bits_left(rotation_track_bit_mask, 2);
						translation_track_bit_mask = rotate_bits_left(translation_track_bit_mask, 2);
						scale_track_bit_mask = rotate_bits_left(scale_track_bit_mask, 2);
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

				const rotation_format8 packed_rotation_format = is_rotation_format_variable(rotation_format) ? get_highest_variant_precision(get_rotation_variant(rotation_format)) : rotation_format;
				const uint32_t packed_rotation_size = get_packed_rotation_size(packed_rotation_format);

				uint32_t constant_track_data_offset = (num_constant_rotations - num_default_rotations) * packed_rotation_size;
				constant_track_data_offset += (num_constant_translations - num_default_translations) * get_packed_vector_size(vector_format8::vector3f_full);

				uint32_t clip_range_data_offset = 0;
				uint32_t segment_range_data_offset = 0;

				const range_reduction_flags8 range_reduction = m_context.range_reduction;
				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::rotations) && m_settings.are_range_reduction_flags_supported(range_reduction_flags8::rotations))
				{
					clip_range_data_offset += m_context.num_rotation_components * sizeof(float) * 2 * num_animated_rotations;

					if (header.num_segments > 1)
						segment_range_data_offset += m_context.num_rotation_components * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_rotations;
				}

				if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::translations) && m_settings.are_range_reduction_flags_supported(range_reduction_flags8::translations))
				{
					clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_translations;

					if (header.num_segments > 1)
						segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_translations;
				}

				uint32_t num_animated_tracks = num_animated_rotations + num_animated_translations;
				if (header.has_scale)
				{
					const uint32_t num_animated_scales = sample_bone_index - num_constant_scales;
					num_animated_tracks += num_animated_scales;

					constant_track_data_offset += (num_constant_scales - num_default_scales) * get_packed_vector_size(vector_format8::vector3f_full);

					if (are_any_enum_flags_set(range_reduction, range_reduction_flags8::scales) && m_settings.are_range_reduction_flags_supported(range_reduction_flags8::scales))
					{
						clip_range_data_offset += k_clip_range_reduction_vector3_range_size * num_animated_scales;

						if (header.num_segments > 1)
							segment_range_data_offset += 3 * k_segment_range_reduction_num_bytes_per_component * 2 * num_animated_scales;
					}
				}

				sampling_context.track_index = track_index;
				sampling_context.constant_track_data_offset = constant_track_data_offset;
				sampling_context.clip_range_data_offset = clip_range_data_offset;
				sampling_context.segment_range_data_offset = segment_range_data_offset;
				sampling_context.format_per_track_data_offset = num_animated_tracks;

				for (uint32_t animated_track_index = 0; animated_track_index < num_animated_tracks; ++animated_track_index)
				{
					const uint8_t bit_rate0 = m_context.format_per_track_data[0][animated_track_index];
					const uint32_t num_bits_at_bit_rate0 = get_num_bits_at_bit_rate(bit_rate0) * 3;	// 3 components

					sampling_context.key_frame_bit_offsets[0] += num_bits_at_bit_rate0;

					const uint8_t bit_rate1 = m_context.format_per_track_data[1][animated_track_index];
					const uint32_t num_bits_at_bit_rate1 = get_num_bits_at_bit_rate(bit_rate1) * 3;	// 3 components

					sampling_context.key_frame_bit_offsets[1] += num_bits_at_bit_rate1;
				}
			}

			const rtm::vector4f default_scale = scale_adapter.get_default_value();

			sampling_context.vectors[0] = default_scale;	// Init with something to avoid GCC warning
			sampling_context.vectors[1] = default_scale;	// Init with something to avoid GCC warning

			if (out_rotation != nullptr)
				*out_rotation = decompress_and_interpolate_rotation(m_settings, header, m_context, sampling_context);
			else
				skip_over_rotation(m_settings, header, m_context, sampling_context);

			if (out_translation != nullptr)
				*out_translation = decompress_and_interpolate_vector(translation_adapter, header, m_context, sampling_context);
			else if (out_scale != nullptr && header.has_scale)
			{
				// We'll need to read the scale value that follows, skip the translation we don't need
				skip_over_vector(translation_adapter, header, m_context, sampling_context);
			}

			if (out_scale != nullptr)
				*out_scale = header.has_scale ? decompress_and_interpolate_vector(scale_adapter, header, m_context, sampling_context) : default_scale;
			// No need to skip our last scale, we don't care anymore

			if (m_settings.disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP
