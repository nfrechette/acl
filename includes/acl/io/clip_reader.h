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
#include "acl/core/memory.h"
#include "acl/core/string.h"
#include "acl/core/string_view.h"
#include "acl/sjson/sjson_parser.h"

#include <stdint.h>

namespace acl
{
	class ClipReader
	{
	public:
		ClipReader(Allocator& allocator, const char* sjson_input, size_t input_length)
			: m_allocator(allocator)
			, m_parser(sjson_input, input_length)
			, m_error()
			, m_version()
			, m_num_samples()
			, m_sample_rate()
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
		Allocator& m_allocator;
		SJSONParser m_parser;
		ClipReaderError m_error;

		double m_version;
		uint32_t m_num_samples;
		uint32_t m_sample_rate;
		StringView m_clip_name;
		double m_error_threshold;

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
						bone.parent_index = INVALID_BONE_INDEX;
					}
					else
					{
						bone.parent_index = find_bone(bones, num_bones, parent);
						if (bone.parent_index == INVALID_BONE_INDEX)
						{
							set_error(ClipReaderError::NoParentBoneWithThatName);
							return false;
						}
					}
				}

				if (!m_parser.read("vertex_distance", bone.vertex_distance))
					goto error;

				double rotation[4];
				if (m_parser.try_read("bind_rotation", rotation, 4) && !counting)
					bone.bind_rotation = quat_unaligned_load(rotation);

				double translation[3];
				if (m_parser.try_read("bind_translation", translation, 3) && !counting)
					bone.bind_translation = vector_unaligned_load3(translation);

				double scale[3];
				if (m_parser.try_read("bind_scale", scale, 3) && !counting)
				{
					// TODO: do something with bind_scale.
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

			return INVALID_BONE_INDEX;
		}

		bool create_clip(std::unique_ptr<AnimationClip, Deleter<AnimationClip>>& clip, const RigidSkeleton& skeleton)
		{
			clip = make_unique<AnimationClip>(m_allocator, m_allocator, skeleton, m_num_samples, m_sample_rate, String(m_allocator, m_clip_name), m_error_threshold);
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
				if (bone_index == INVALID_BONE_INDEX)
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
					// TODO: Set default scale
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
				double quaternion[4];

				if (!m_parser.array_begins() || !m_parser.read(quaternion, 4) || !m_parser.array_ends())
					return false;

				bone.rotation_track.set_sample(i, quat_unaligned_load(quaternion));
			}

			return true;
		}

		bool read_track_translations(AnimatedBone& bone)
		{
			for (uint32_t i = 0; i < m_num_samples; ++i)
			{
				double translation[3];

				if (!m_parser.array_begins() || !m_parser.read(translation, 3) || !m_parser.array_ends())
					return false;

				bone.translation_track.set_sample(i, vector_unaligned_load3(translation));
			}

			return true;
		}

		bool read_track_scales(AnimatedBone& bone)
		{
			for (uint32_t i = 0; i < m_num_samples; ++i)
			{
				double scale[3];

				if (!m_parser.array_begins() || !m_parser.read(scale, 3) || !m_parser.array_ends())
					return false;

				// TODO: do something with scale.
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
