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

#if defined(SJSON_CPP_PARSER)

#include "acl/io/clip_reader_error.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/skeleton.h"
#include "acl/core/algorithm_types.h"
#include "acl/core/iallocator.h"
#include "acl/core/string.h"
#include "acl/core/unique_ptr.h"

#include <cstdint>

#if defined(__ANDROID__)
	#include <stdlib.h>
#else
	#include <cstdlib>
#endif

namespace acl
{
	namespace impl
	{
		inline unsigned long long int strtoull(const char* str, char** endptr, int base)
		{
#if defined(__ANDROID__)
			return ::strtoull(str, endptr, base);
#else
			return std::strtoull(str, endptr, base);
#endif
		}
	}

	class ClipReader
	{
	public:
		ClipReader(IAllocator& allocator, const char* sjson_input, size_t input_length)
			: m_allocator(allocator)
			, m_parser(sjson_input, input_length)
			, m_error()
			, m_version(0)
			, m_num_samples(0)
			, m_sample_rate(0)
			, m_is_binary_exact(false)
		{
		}

		bool read_settings(bool& out_has_settings, AlgorithmType8& out_algorithm_type, CompressionSettings& out_settings)
		{
			reset_state();

			return read_version() && read_clip_header() && read_settings(&out_has_settings, &out_algorithm_type, &out_settings);
		}

		bool read_skeleton(std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
		{
			reset_state();

			return read_version() && read_clip_header() && read_settings(nullptr, nullptr, nullptr) && create_skeleton(skeleton);
		}

		bool read_clip(std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip, const RigidSkeleton& skeleton)
		{
			reset_state();

			return read_version() && read_clip_header() && read_settings(nullptr, nullptr, nullptr) && read_skeleton() && create_clip(clip, skeleton) && read_tracks(*clip, skeleton) && nothing_follows();
		}

		ClipReaderError get_error() { return m_error; }

	private:
		IAllocator& m_allocator;
		sjson::Parser m_parser;
		ClipReaderError m_error;

		uint32_t m_version;
		uint32_t m_num_samples;
		uint32_t m_sample_rate;
		sjson::StringView m_clip_name;
		bool m_is_binary_exact;
		AdditiveClipFormat8 m_additive_format;
		sjson::StringView m_additive_base_name;
		uint32_t m_additive_base_num_samples;
		uint32_t m_additive_base_sample_rate;

		void reset_state()
		{
			m_parser.reset_state();
			set_error(ClipReaderError::None);
		}

		bool read_version()
		{
			if (!m_parser.read("version", m_version))
			{
				m_error = m_parser.get_error();
				return false;
			}

			if (m_version > 3)
			{
				set_error(ClipReaderError::UnsupportedVersion);
				return false;
			}

			return true;
		}

		bool read_clip_header()
		{
			sjson::StringView additive_format;

			if (!m_parser.object_begins("clip"))
				goto error;

			if (!m_parser.read("name", m_clip_name))
				goto error;

			double num_samples;
			if (!m_parser.read("num_samples", num_samples))
				goto error;

			m_num_samples = static_cast<uint32_t>(num_samples);
			if (static_cast<double>(m_num_samples) != num_samples || m_num_samples == 0)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}

			double sample_rate;
			if (!m_parser.read("sample_rate", sample_rate))
				goto error;

			m_sample_rate = static_cast<uint32_t>(sample_rate);
			if (static_cast<double>(m_sample_rate) != sample_rate || m_sample_rate == 0)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}

			// Version 1 had an error_threshold field, skip it
			double error_threshold;
			if (m_version == 1 && !m_parser.read("error_threshold", error_threshold))
				goto error;

			// Optional value
			m_parser.try_read("is_binary_exact", m_is_binary_exact, false);

			// Optional value
			m_parser.try_read("additive_format", additive_format, "None");
			if (!get_additive_clip_format(additive_format.c_str(), m_additive_format))
			{
				set_error(ClipReaderError::InvalidAdditiveClipFormat);
				return false;
			}

			m_parser.try_read("additive_base_name", m_additive_base_name, "");
			m_parser.try_read("additive_base_num_samples", num_samples, 1.0);
			m_additive_base_num_samples = static_cast<uint32_t>(num_samples);
			if (static_cast<double>(m_additive_base_num_samples) != num_samples || m_additive_base_num_samples == 0)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}
			m_parser.try_read("additive_base_sample_rate", sample_rate, 30.0);
			m_additive_base_sample_rate = static_cast<uint32_t>(sample_rate);
			if (static_cast<double>(m_additive_base_sample_rate) != sample_rate || m_additive_base_sample_rate == 0)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}

			if (!m_parser.object_ends())
				goto error;

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		bool read_settings(bool* out_has_settings, AlgorithmType8* out_algorithm_type, CompressionSettings* out_settings)
		{
			if (!m_parser.try_object_begins("settings"))
			{
				if (out_has_settings != nullptr)
					*out_has_settings = false;

				// Settings are optional, all good
				return true;
			}

			CompressionSettings default_settings;

			sjson::StringView algorithm_name;
			sjson::StringView rotation_format;
			sjson::StringView translation_format;
			sjson::StringView scale_format;
			bool rotation_range_reduction;
			bool translation_range_reduction;
			bool scale_range_reduction;
			double constant_rotation_threshold_angle;
			double constant_translation_threshold;
			double constant_scale_threshold;
			double error_threshold;

			bool segmenting_enabled = default_settings.segmenting.enabled;
			double segmenting_ideal_num_samples = double(default_settings.segmenting.ideal_num_samples);
			double segmenting_max_num_samples = double(default_settings.segmenting.max_num_samples);
			bool segmenting_rotation_range_reduction = are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Rotations);
			bool segmenting_translation_range_reduction = are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Translations);
			bool segmenting_scale_range_reduction = are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Scales);

			m_parser.try_read("algorithm_name", algorithm_name, get_algorithm_name(AlgorithmType8::UniformlySampled));
			m_parser.try_read("rotation_format", rotation_format, get_rotation_format_name(default_settings.rotation_format));
			m_parser.try_read("translation_format", translation_format, get_vector_format_name(default_settings.translation_format));
			m_parser.try_read("scale_format", scale_format, get_vector_format_name(default_settings.scale_format));
			m_parser.try_read("rotation_range_reduction", rotation_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Rotations));
			m_parser.try_read("translation_range_reduction", translation_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Translations));
			m_parser.try_read("scale_range_reduction", scale_range_reduction, are_any_enum_flags_set(default_settings.range_reduction, RangeReductionFlags8::Scales));

			if (m_parser.try_object_begins("segmenting"))
			{
				m_parser.try_read("enabled", segmenting_enabled, default_settings.segmenting.enabled);
				m_parser.try_read("ideal_num_samples", segmenting_ideal_num_samples, double(default_settings.segmenting.ideal_num_samples));
				m_parser.try_read("max_num_samples", segmenting_max_num_samples, double(default_settings.segmenting.max_num_samples));
				m_parser.try_read("rotation_range_reduction", segmenting_rotation_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Rotations));
				m_parser.try_read("translation_range_reduction", segmenting_translation_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Translations));
				m_parser.try_read("scale_range_reduction", segmenting_scale_range_reduction, are_any_enum_flags_set(default_settings.segmenting.range_reduction, RangeReductionFlags8::Scales));

				if (!m_parser.is_valid() || !m_parser.object_ends())
					goto parsing_error;
			}

			m_parser.try_read("constant_rotation_threshold_angle", constant_rotation_threshold_angle, double(default_settings.constant_rotation_threshold_angle));
			m_parser.try_read("constant_translation_threshold", constant_translation_threshold, double(default_settings.constant_translation_threshold));
			m_parser.try_read("constant_scale_threshold", constant_scale_threshold, double(default_settings.constant_scale_threshold));
			m_parser.try_read("error_threshold", error_threshold, double(default_settings.error_threshold));

			if (!m_parser.is_valid() || !m_parser.object_ends())
				goto parsing_error;

			if (out_has_settings != nullptr)
			{
				*out_has_settings = true;

				if (!get_algorithm_type(algorithm_name.c_str(), *out_algorithm_type))
					goto invalid_value_error;

				if (!get_rotation_format(rotation_format.c_str(), out_settings->rotation_format))
					goto invalid_value_error;

				if (!get_vector_format(translation_format.c_str(), out_settings->translation_format))
					goto invalid_value_error;

				if (!get_vector_format(scale_format.c_str(), out_settings->scale_format))
					goto invalid_value_error;

				RangeReductionFlags8 range_reduction = RangeReductionFlags8::None;
				if (rotation_range_reduction)
					range_reduction |= RangeReductionFlags8::Rotations;

				if (translation_range_reduction)
					range_reduction |= RangeReductionFlags8::Translations;

				if (scale_range_reduction)
					range_reduction |= RangeReductionFlags8::Scales;

				out_settings->range_reduction = range_reduction;

				out_settings->segmenting.enabled = segmenting_enabled;
				out_settings->segmenting.ideal_num_samples = uint16_t(segmenting_ideal_num_samples);
				out_settings->segmenting.max_num_samples = uint16_t(segmenting_max_num_samples);

				RangeReductionFlags8 segmenting_range_reduction = RangeReductionFlags8::None;
				if (rotation_range_reduction)
					segmenting_range_reduction |= RangeReductionFlags8::Rotations;

				if (translation_range_reduction)
					segmenting_range_reduction |= RangeReductionFlags8::Translations;

				if (scale_range_reduction)
					segmenting_range_reduction |= RangeReductionFlags8::Scales;

				out_settings->segmenting.range_reduction = segmenting_range_reduction;

				out_settings->constant_rotation_threshold_angle = float(constant_rotation_threshold_angle);
				out_settings->constant_translation_threshold = float(constant_translation_threshold);
				out_settings->constant_scale_threshold = float(constant_scale_threshold);
				out_settings->error_threshold = float(error_threshold);
			}

			return true;

		parsing_error:
			m_error = m_parser.get_error();
			return false;

		invalid_value_error:
			m_parser.get_position(m_error.line, m_error.column);
			m_error.error = ClipReaderError::InvalidCompressionSetting;
			return false;
		}

		bool create_skeleton(std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
		{
			sjson::ParserState before_bones = m_parser.save_state();

			uint16_t num_bones;
			if (!process_each_bone(nullptr, num_bones))
				return false;

			m_parser.restore_state(before_bones);

			RigidBone* bones = allocate_type_array<RigidBone>(m_allocator, num_bones);
			const uint16_t num_allocated_bones = num_bones;

			if (!process_each_bone(bones, num_bones))
			{
				deallocate_type_array(m_allocator, bones, num_allocated_bones);
				return false;
			}

			ACL_ASSERT(num_bones == num_allocated_bones, "Number of bones read mismatch");
			skeleton = make_unique<RigidSkeleton>(m_allocator, m_allocator, bones, num_bones);
			deallocate_type_array(m_allocator, bones, num_allocated_bones);

			return true;
		}

		bool read_skeleton()
		{
			uint16_t num_bones;
			return process_each_bone(nullptr, num_bones);
		}

		static double hex_to_double(const sjson::StringView& value)
		{
			union UInt64ToDouble
			{
				uint64_t u64;
				double dbl;

				constexpr explicit UInt64ToDouble(uint64_t u64_value) : u64(u64_value) {}
			};

			ACL_ASSERT(value.size() <= 16, "Invalid binary exact double value");
			uint64_t value_u64 = impl::strtoull(value.c_str(), nullptr, 16);
			return UInt64ToDouble(value_u64).dbl;
		}

		static Quat_64 hex_to_quat(const sjson::StringView values[4])
		{
			return quat_set(hex_to_double(values[0]), hex_to_double(values[1]), hex_to_double(values[2]), hex_to_double(values[3]));
		}

		static Vector4_64 hex_to_vector3(const sjson::StringView values[3])
		{
			return vector_set(hex_to_double(values[0]), hex_to_double(values[1]), hex_to_double(values[2]));
		}

		bool process_each_bone(RigidBone* bones, uint16_t& num_bones)
		{
			bool counting = bones == nullptr;
			num_bones = 0;

			if (!m_parser.array_begins("bones"))
				goto error;

			for (uint16_t i = 0; !m_parser.try_array_ends(); ++i)
			{
				RigidBone dummy;
				RigidBone& bone = counting ? dummy : bones[i];

				if (!m_parser.object_begins())
					goto error;

				sjson::StringView name;
				if (!m_parser.read("name", name))
					goto error;

				if (!counting)
					bone.name = String(m_allocator, name.c_str(), name.size());

				sjson::StringView parent;
				if (!m_parser.read("parent", parent))
					goto error;

				if (!counting)
				{
					if (parent.size() == 0)
					{
						// This is the root bone.
						bone.parent_index = k_invalid_bone_index;
					}
					else
					{
						bone.parent_index = find_bone(bones, num_bones, parent);
						if (bone.parent_index == k_invalid_bone_index)
						{
							set_error(ClipReaderError::NoParentBoneWithThatName);
							return false;
						}
					}
				}

				if (!m_parser.read("vertex_distance", bone.vertex_distance))
					goto error;

				if (m_is_binary_exact)
				{
					sjson::StringView rotation[4];
					if (m_parser.try_read("bind_rotation", rotation, 4, nullptr) && !counting)
						bone.bind_transform.rotation = hex_to_quat(rotation);

					sjson::StringView translation[3];
					if (m_parser.try_read("bind_translation", translation, 3, nullptr) && !counting)
						bone.bind_transform.translation = hex_to_vector3(translation);

					sjson::StringView scale[3];
					if (m_parser.try_read("bind_scale", scale, 3, nullptr) && !counting)
						bone.bind_transform.scale = hex_to_vector3(scale);
				}
				else
				{
					double rotation[4];
					if (m_parser.try_read("bind_rotation", rotation, 4, 0.0) && !counting)
						bone.bind_transform.rotation = quat_unaligned_load(&rotation[0]);

					double translation[3];
					if (m_parser.try_read("bind_translation", translation, 3, 0.0) && !counting)
						bone.bind_transform.translation = vector_unaligned_load3(&translation[0]);

					double scale[3];
					if (m_parser.try_read("bind_scale", scale, 3, 0.0) && !counting)
						bone.bind_transform.scale = vector_unaligned_load3(&scale[0]);
				}

				if (!m_parser.object_ends())
					goto error;

				++num_bones;
			}

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		uint16_t find_bone(const RigidBone* bones, uint16_t num_bones, const sjson::StringView& name) const
		{
			for (uint16_t i = 0; i < num_bones; ++i)
			{
				if (name == bones[i].name.c_str())
					return i;
			}

			return k_invalid_bone_index;
		}

		bool create_clip(std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip, const RigidSkeleton& skeleton)
		{
			clip = make_unique<AnimationClip>(m_allocator, m_allocator, skeleton, m_num_samples, m_sample_rate, String(m_allocator, m_clip_name.c_str(), m_clip_name.size()));
			return true;
		}

		bool read_tracks(AnimationClip& clip, const RigidSkeleton& skeleton)
		{
			std::unique_ptr<AnimationClip, Deleter<AnimationClip>> base_clip;

			if (m_parser.try_array_begins("base_tracks"))
			{
				base_clip = make_unique<AnimationClip>(m_allocator, m_allocator, skeleton, m_additive_base_num_samples, m_additive_base_sample_rate, String(m_allocator, m_additive_base_name.c_str(), m_additive_base_name.size()));

				while (!m_parser.try_array_ends())
				{
					if (!m_parser.object_begins())
						goto error;

					sjson::StringView name;
					if (!m_parser.read("name", name))
						goto error;

					const uint16_t bone_index = find_bone(skeleton.get_bones(), skeleton.get_num_bones(), name);
					if (bone_index == k_invalid_bone_index)
					{
						set_error(ClipReaderError::NoBoneWithThatName);
						return false;
					}

					AnimatedBone& bone = base_clip->get_animated_bone(bone_index);

					if (m_parser.try_array_begins("rotations"))
					{
						if (!read_track_rotations(bone, m_additive_base_num_samples) || !m_parser.array_ends())
							goto error;
					}
					else
					{
						for (uint32_t sample_index = 0; sample_index < m_additive_base_num_samples; ++sample_index)
							bone.rotation_track.set_sample(sample_index, quat_identity_64());
					}

					if (m_parser.try_array_begins("translations"))
					{
						if (!read_track_translations(bone, m_additive_base_num_samples) || !m_parser.array_ends())
							goto error;
					}
					else
					{
						for (uint32_t sample_index = 0; sample_index < m_additive_base_num_samples; ++sample_index)
							bone.translation_track.set_sample(sample_index, vector_zero_64());
					}

					if (m_parser.try_array_begins("scales"))
					{
						if (!read_track_scales(bone, m_additive_base_num_samples) || !m_parser.array_ends())
							goto error;
					}
					else
					{
						for (uint32_t sample_index = 0; sample_index < m_additive_base_num_samples; ++sample_index)
							bone.scale_track.set_sample(sample_index, vector_set(1.0));
					}

					if (!m_parser.object_ends())
						goto error;
				}
			}

			if (!m_parser.array_begins("tracks"))
				goto error;

			while (!m_parser.try_array_ends())
			{
				if (!m_parser.object_begins())
					goto error;

				sjson::StringView name;
				if (!m_parser.read("name", name))
					goto error;

				const uint16_t bone_index = find_bone(skeleton.get_bones(), skeleton.get_num_bones(), name);
				if (bone_index == k_invalid_bone_index)
				{
					set_error(ClipReaderError::NoBoneWithThatName);
					return false;
				}

				AnimatedBone& bone = clip.get_animated_bone(bone_index);

				if (m_parser.try_array_begins("rotations"))
				{
					if (!read_track_rotations(bone, m_num_samples) || !m_parser.array_ends())
						goto error;
				}
				else
				{
					for (uint32_t sample_index = 0; sample_index < m_num_samples; ++sample_index)
						bone.rotation_track.set_sample(sample_index, quat_identity_64());
				}

				if (m_parser.try_array_begins("translations"))
				{
					if (!read_track_translations(bone, m_num_samples) || !m_parser.array_ends())
						goto error;
				}
				else
				{
					for (uint32_t sample_index = 0; sample_index < m_num_samples; ++sample_index)
						bone.translation_track.set_sample(sample_index, vector_zero_64());
				}

				if (m_parser.try_array_begins("scales"))
				{
					if (!read_track_scales(bone, m_num_samples) || !m_parser.array_ends())
						goto error;
				}
				else
				{
					for (uint32_t sample_index = 0; sample_index < m_num_samples; ++sample_index)
						bone.scale_track.set_sample(sample_index, vector_set(1.0));
				}

				if (!m_parser.object_ends())
					goto error;
			}

			clip.set_additive_base(base_clip.release(), m_additive_format);

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		bool read_track_rotations(AnimatedBone& bone, uint32_t num_samples_expected)
		{
			for (uint32_t i = 0; i < num_samples_expected; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Quat_64 rotation;

				if (m_is_binary_exact)
				{
					sjson::StringView values[4];
					if (!m_parser.read(values, 4))
						return false;

					rotation = hex_to_quat(values);
				}
				else
				{
					double values[4];
					if (!m_parser.read(values, 4))
						return false;

					rotation = quat_unaligned_load(values);
				}

				if (!m_parser.array_ends())
					return false;

				bone.rotation_track.set_sample(i, rotation);
			}

			return true;
		}

		bool read_track_translations(AnimatedBone& bone, uint32_t num_samples_expected)
		{
			for (uint32_t i = 0; i < num_samples_expected; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Vector4_64 translation;

				if (m_is_binary_exact)
				{
					sjson::StringView values[3];
					if (!m_parser.read(values, 3))
						return false;

					translation = hex_to_vector3(values);
				}
				else
				{
					double values[3];
					if (!m_parser.read(values, 3))
						return false;

					translation = vector_unaligned_load3(values);
				}

				if (!m_parser.array_ends())
					return false;

				bone.translation_track.set_sample(i, translation);
			}

			return true;
		}

		bool read_track_scales(AnimatedBone& bone, uint32_t num_samples_expected)
		{
			for (uint32_t i = 0; i < num_samples_expected; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Vector4_64 scale;

				if (m_is_binary_exact)
				{
					sjson::StringView values[3];
					if (!m_parser.read(values, 3))
						return false;

					scale = hex_to_vector3(values);
				}
				else
				{
					double values[3];
					if (!m_parser.read(values, 3))
						return false;

					scale = vector_unaligned_load3(values);
				}

				if (!m_parser.array_ends())
					return false;

				bone.scale_track.set_sample(i, scale);
			}

			return true;
		}

		bool nothing_follows()
		{
			if (!m_parser.remainder_is_comments_and_whitespace())
			{
				m_error = m_parser.get_error();
				return false;
			}

			return true;
		}

		void set_error(uint32_t reason)
		{
			m_parser.get_position(m_error.line, m_error.column);
			m_error.error = reason;
		}
	};
}

#endif	// #if defined(SJSON_CPP_PARSER)
