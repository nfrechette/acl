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

#include "acl/io/clip_reader_error.h"
#include "acl/compression/animation_clip.h"
#include "acl/compression/skeleton.h"
#include "acl/core/iallocator.h"
#include "acl/core/string.h"
#include "acl/core/string_view.h"
#include "acl/core/unique_ptr.h"
#include "acl/sjson/sjson_parser.h"

#include <cstdint>

namespace acl
{
	class ClipReader
	{
	public:
		ClipReader(IAllocator& allocator, const char* sjson_input, size_t input_length)
			: m_allocator(allocator)
			, m_parser(sjson_input, input_length)
			, m_error()
			, m_version(0.0)
			, m_num_samples(0)
			, m_sample_rate(0)
			, m_error_threshold(0.0)
			, m_is_binary_exact(false)
		{
		}

		bool read(std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
		{
			reset_state();

			return read_version() && read_clip_header() && create_skeleton(skeleton);
		}

		bool read(std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip, const RigidSkeleton& skeleton)
		{
			reset_state();

			return read_version() && read_clip_header() && read_skeleton() && create_clip(clip, skeleton) && read_tracks(*clip, skeleton) && nothing_follows();
		}

		ClipReaderError get_error() { return m_error; }

	private:
		IAllocator& m_allocator;
		SJSONParser m_parser;
		ClipReaderError m_error;

		double m_version;
		uint32_t m_num_samples;
		uint32_t m_sample_rate;
		StringView m_clip_name;
		double m_error_threshold;
		bool m_is_binary_exact;

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

			if (m_version != 1.0)
			{
				set_error(ClipReaderError::UnsupportedVersion);
				return false;
			}

			return true;
		}

		bool read_clip_header()
		{
			if (!m_parser.object_begins("clip"))
				goto error;

			if (!m_parser.read("name", m_clip_name))
				goto error;

			double num_samples;
			if (!m_parser.read("num_samples", num_samples))
				goto error;

			m_num_samples = static_cast<uint32_t>(num_samples);
			if (static_cast<double>(m_num_samples) != num_samples)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}

			double sample_rate;
			if (!m_parser.read("sample_rate", sample_rate))
				goto error;

			m_sample_rate = static_cast<uint32_t>(sample_rate);
			if (static_cast<double>(m_sample_rate) != sample_rate)
			{
				set_error(ClipReaderError::UnsignedIntegerExpected);
				return false;
			}

			if (!m_parser.read("error_threshold", m_error_threshold))
				goto error;

			// Optional value
			if (!m_parser.read("is_binary_exact", m_is_binary_exact))
				m_is_binary_exact = false;

			if (!m_parser.object_ends())
				goto error;

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		bool create_skeleton(std::unique_ptr<RigidSkeleton, Deleter<RigidSkeleton>>& skeleton)
		{
			SJSONParser::State before_bones = m_parser.save_state();

			uint16_t num_bones;
			if (!process_each_bone(nullptr, num_bones))
				return false;

			m_parser.restore_state(before_bones);

			std::unique_ptr<RigidBone, Deleter<RigidBone>> bones = make_unique_array<RigidBone>(m_allocator, num_bones);
			if (!process_each_bone(bones.get(), num_bones))
				return false;

			skeleton = make_unique<RigidSkeleton>(m_allocator, m_allocator, bones.get(), num_bones);

			return true;
		}

		bool read_skeleton()
		{
			uint16_t num_bones;
			return process_each_bone(nullptr, num_bones);
		}

		static double hex_to_double(const StringView& value)
		{
			union UInt64ToDouble
			{
				uint64_t u64;
				double dbl;

				constexpr explicit UInt64ToDouble(uint64_t value) : u64(value) {}
			};

			uint64_t value_u64 = std::strtoull(value.get_chars(), nullptr, 16);
			return UInt64ToDouble(value_u64).dbl;
		}

		static Quat_64 hex_to_quat(const StringView values[4])
		{
			return quat_set(hex_to_double(values[0]), hex_to_double(values[1]), hex_to_double(values[2]), hex_to_double(values[3]));
		}

		static Vector4_64 hex_to_vector3(const StringView values[3])
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

				StringView name;
				if (!m_parser.read("name", name))
					goto error;

				if (!counting)
					bone.name = String(m_allocator, name);

				StringView parent;
				if (!m_parser.read("parent", parent))
					goto error;

				if (!counting)
				{
					if (parent.get_length() == 0)
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
					StringView rotation[4];
					if (m_parser.try_read("bind_rotation", rotation, 4) && !counting)
						bone.bind_transform.rotation = hex_to_quat(rotation);

					StringView translation[3];
					if (m_parser.try_read("bind_translation", translation, 3) && !counting)
						bone.bind_transform.translation = hex_to_vector3(translation);

					StringView scale[3];
					if (m_parser.try_read("bind_scale", scale, 3) && !counting)
						bone.bind_transform.scale = hex_to_vector3(scale);
				}
				else
				{
					double rotation[4];
					if (m_parser.try_read("bind_rotation", rotation, 4) && !counting)
						bone.bind_transform.rotation = quat_unaligned_load(&rotation[0]);

					double translation[3];
					if (m_parser.try_read("bind_translation", translation, 3) && !counting)
						bone.bind_transform.translation = vector_unaligned_load3(&translation[0]);

					double scale[3];
					if (m_parser.try_read("bind_scale", scale, 3) && !counting)
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

		uint16_t find_bone(const RigidBone* bones, uint16_t num_bones, StringView name) const
		{
			for (uint16_t i = 0; i < num_bones; ++i)
			{
				if (bones[i].name == name)
					return i;
			}

			return k_invalid_bone_index;
		}

		bool create_clip(std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip, const RigidSkeleton& skeleton)
		{
			clip = make_unique<AnimationClip>(m_allocator, m_allocator, skeleton, m_num_samples, m_sample_rate, String(m_allocator, m_clip_name), (float)m_error_threshold);
			return true;
		}

		bool read_tracks(AnimationClip& clip, const RigidSkeleton& skeleton)
		{
			if (!m_parser.array_begins("tracks"))
				goto error;

			while (!m_parser.try_array_ends())
			{
				if (!m_parser.object_begins())
					goto error;

				StringView name;
				if (!m_parser.read("name", name))
					goto error;

				uint16_t bone_index = find_bone(skeleton.get_bones(), skeleton.get_num_bones(), name);
				if (bone_index == k_invalid_bone_index)
				{
					set_error(ClipReaderError::NoBoneWithThatName);
					return false;
				}

				AnimatedBone& bone = clip.get_bones()[bone_index];

				if (m_parser.try_array_begins("rotations"))
				{
					if (!read_track_rotations(bone) || !m_parser.array_ends())
						goto error;
				}
				else
				{
					for (uint32_t sample_index = 0; sample_index < m_num_samples; ++sample_index)
						bone.rotation_track.set_sample(sample_index, quat_identity_64());
				}

				if (m_parser.try_array_begins("translations"))
				{
					if (!read_track_translations(bone) || !m_parser.array_ends())
						goto error;
				}
				else
				{
					for (uint32_t sample_index = 0; sample_index < m_num_samples; ++sample_index)
						bone.translation_track.set_sample(sample_index, vector_zero_64());
				}

				if (m_parser.try_array_begins("scales"))
				{
					if (!read_track_scales(bone) || !m_parser.array_ends())
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

			return true;

		error:
			m_error = m_parser.get_error();
			return false;
		}

		bool read_track_rotations(AnimatedBone& bone)
		{
			for (uint32_t i = 0; i < m_num_samples; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Quat_64 rotation;

				if (m_is_binary_exact)
				{
					StringView values[4];
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

		bool read_track_translations(AnimatedBone& bone)
		{
			for (uint32_t i = 0; i < m_num_samples; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Vector4_64 translation;

				if (m_is_binary_exact)
				{
					StringView values[3];
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

		bool read_track_scales(AnimatedBone& bone)
		{
			for (uint32_t i = 0; i < m_num_samples; ++i)
			{
				if (!m_parser.array_begins())
					return false;

				Vector4_64 scale;

				if (m_is_binary_exact)
				{
					StringView values[3];
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
