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

#if defined(SJSON_CPP_WRITER)

#include "acl/compression/animation_clip.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/skeleton.h"
#include "acl/compression/track_array.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"

#include <rtm/quatd.h>
#include <rtm/vector4d.h>

#include <cstdint>
#include <cinttypes>
#include <cstdio>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
	{
		inline const char* format_hex_float(float value, char* buffer, size_t buffer_size)
		{
			union FloatToUInt32
			{
				uint32_t u32;
				float flt;

				constexpr explicit FloatToUInt32(float flt_value) : flt(flt_value) {}
			};

			snprintf(buffer, buffer_size, "%" PRIX32, FloatToUInt32(value).u32);

			return buffer;
		};

		inline const char* format_hex_double(double value, char* buffer, size_t buffer_size)
		{
			union DoubleToUInt64
			{
				uint64_t u64;
				double dbl;

				constexpr explicit DoubleToUInt64(double dbl_value) : dbl(dbl_value) {}
			};

			snprintf(buffer, buffer_size, "%" PRIX64, DoubleToUInt64(value).u64);

			return buffer;
		};

		inline void write_sjson_clip(const AnimationClip& clip, sjson::Writer& writer)
		{
			writer["clip"] = [&](sjson::ObjectWriter& clip_writer)
			{
				clip_writer["name"] = clip.get_name().c_str();
				clip_writer["num_samples"] = clip.get_num_samples();
				clip_writer["sample_rate"] = clip.get_sample_rate();
				clip_writer["is_binary_exact"] = true;
				clip_writer["additive_format"] = get_additive_clip_format_name(clip.get_additive_format());

				const AnimationClip* base_clip = clip.get_additive_base();
				if (base_clip != nullptr)
				{
					clip_writer["additive_base_name"] = base_clip->get_name().c_str();
					clip_writer["additive_base_num_samples"] = base_clip->get_num_samples();
					clip_writer["additive_base_sample_rate"] = base_clip->get_sample_rate();
				}
			};
			writer.insert_newline();
		}

		inline void write_sjson_settings(algorithm_type8 algorithm, const CompressionSettings& settings, sjson::Writer& writer)
		{
			writer["settings"] = [&](sjson::ObjectWriter& settings_writer)
			{
				settings_writer["algorithm_name"] = get_algorithm_name(algorithm);
				settings_writer["level"] = get_compression_level_name(settings.level);
				settings_writer["rotation_format"] = get_rotation_format_name(settings.rotation_format);
				settings_writer["translation_format"] = get_vector_format_name(settings.translation_format);
				settings_writer["scale_format"] = get_vector_format_name(settings.scale_format);

				settings_writer["segmenting"] = [&](sjson::ObjectWriter& segmenting_writer)
				{
					segmenting_writer["ideal_num_samples"] = settings.segmenting.ideal_num_samples;
					segmenting_writer["max_num_samples"] = settings.segmenting.max_num_samples;
				};

				settings_writer["constant_rotation_threshold_angle"] = settings.constant_rotation_threshold_angle;
				settings_writer["constant_translation_threshold"] = settings.constant_translation_threshold;
				settings_writer["constant_scale_threshold"] = settings.constant_scale_threshold;
				settings_writer["error_threshold"] = settings.error_threshold;
			};
			writer.insert_newline();
		}

		inline void write_sjson_bones(const RigidSkeleton& skeleton, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer["bones"] = [&](sjson::ArrayWriter& bones_writer)
			{
				const uint16_t num_bones = skeleton.get_num_bones();
				if (num_bones > 0)
					bones_writer.push_newline();

				for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					const RigidBone& bone = skeleton.get_bone(bone_index);
					const RigidBone& parent_bone = bone.is_root() ? bone : skeleton.get_bone(bone.parent_index);

					bones_writer.push([&](sjson::ObjectWriter& bone_writer)
					{
						bone_writer["name"] = bone.name.c_str();
						bone_writer["parent"] = bone.is_root() ? "" : parent_bone.name.c_str();
						bone_writer["vertex_distance"] = bone.vertex_distance;

						if (!rtm::quat_near_identity(bone.bind_transform.rotation))
						{
							bone_writer["bind_rotation"] = [&](sjson::ArrayWriter& rot_writer)
							{
								rot_writer.push(format_hex_double(rtm::quat_get_x(bone.bind_transform.rotation), buffer, sizeof(buffer)));
								rot_writer.push(format_hex_double(rtm::quat_get_y(bone.bind_transform.rotation), buffer, sizeof(buffer)));
								rot_writer.push(format_hex_double(rtm::quat_get_z(bone.bind_transform.rotation), buffer, sizeof(buffer)));
								rot_writer.push(format_hex_double(rtm::quat_get_w(bone.bind_transform.rotation), buffer, sizeof(buffer)));
							};
						}

						if (!rtm::vector_all_near_equal3(bone.bind_transform.translation, rtm::vector_zero()))
						{
							bone_writer["bind_translation"] = [&](sjson::ArrayWriter& trans_writer)
							{
								trans_writer.push(format_hex_double(rtm::vector_get_x(bone.bind_transform.translation), buffer, sizeof(buffer)));
								trans_writer.push(format_hex_double(rtm::vector_get_y(bone.bind_transform.translation), buffer, sizeof(buffer)));
								trans_writer.push(format_hex_double(rtm::vector_get_z(bone.bind_transform.translation), buffer, sizeof(buffer)));
							};
						}

						if (!rtm::vector_all_near_equal3(bone.bind_transform.scale, rtm::vector_set(1.0)))
						{
							bone_writer["bind_scale"] = [&](sjson::ArrayWriter& scale_writer)
							{
								scale_writer.push(format_hex_double(rtm::vector_get_x(bone.bind_transform.scale), buffer, sizeof(buffer)));
								scale_writer.push(format_hex_double(rtm::vector_get_y(bone.bind_transform.scale), buffer, sizeof(buffer)));
								scale_writer.push(format_hex_double(rtm::vector_get_z(bone.bind_transform.scale), buffer, sizeof(buffer)));
							};
						}
					});
				}
			};
			writer.insert_newline();
		}

		inline void write_sjson_tracks(const RigidSkeleton& skeleton, const AnimationClip& clip, bool is_base_clip, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer[is_base_clip ? "base_tracks" : "tracks"] = [&](sjson::ArrayWriter& tracks_writer)
			{
				const uint16_t num_bones = skeleton.get_num_bones();
				if (num_bones > 0)
					tracks_writer.push_newline();

				for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
				{
					const RigidBone& rigid_bone = skeleton.get_bone(bone_index);
					const AnimatedBone& bone = clip.get_animated_bone(bone_index);

					tracks_writer.push([&](sjson::ObjectWriter& track_writer)
					{
						track_writer["name"] = rigid_bone.name.c_str();
						track_writer["rotations"] = [&](sjson::ArrayWriter& rotations_writer)
						{
							const uint32_t num_rotation_samples = bone.rotation_track.get_num_samples();
							if (num_rotation_samples > 0)
								rotations_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_rotation_samples; ++sample_index)
							{
								const rtm::quatd rotation = bone.rotation_track.get_sample(sample_index);
								rotations_writer.push([&](sjson::ArrayWriter& rot_writer)
								{
									rot_writer.push(format_hex_double(rtm::quat_get_x(rotation), buffer, sizeof(buffer)));
									rot_writer.push(format_hex_double(rtm::quat_get_y(rotation), buffer, sizeof(buffer)));
									rot_writer.push(format_hex_double(rtm::quat_get_z(rotation), buffer, sizeof(buffer)));
									rot_writer.push(format_hex_double(rtm::quat_get_w(rotation), buffer, sizeof(buffer)));
								});
								rotations_writer.push_newline();
							}
						};

						track_writer["translations"] = [&](sjson::ArrayWriter& translations_writer)
						{
							const uint32_t num_translation_samples = bone.translation_track.get_num_samples();
							if (num_translation_samples > 0)
								translations_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_translation_samples; ++sample_index)
							{
								const rtm::vector4d translation = bone.translation_track.get_sample(sample_index);
								translations_writer.push([&](sjson::ArrayWriter& trans_writer)
								{
									trans_writer.push(format_hex_double(rtm::vector_get_x(translation), buffer, sizeof(buffer)));
									trans_writer.push(format_hex_double(rtm::vector_get_y(translation), buffer, sizeof(buffer)));
									trans_writer.push(format_hex_double(rtm::vector_get_z(translation), buffer, sizeof(buffer)));
								});
								translations_writer.push_newline();
							}
						};

						track_writer["scales"] = [&](sjson::ArrayWriter& scales_writer)
						{
							const uint32_t num_scale_samples = bone.scale_track.get_num_samples();
							if (num_scale_samples > 0)
								scales_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_scale_samples; ++sample_index)
							{
								const rtm::vector4d scale = bone.scale_track.get_sample(sample_index);
								scales_writer.push([&](sjson::ArrayWriter& scale_writer)
								{
									scale_writer.push(format_hex_double(rtm::vector_get_x(scale), buffer, sizeof(buffer)));
									scale_writer.push(format_hex_double(rtm::vector_get_y(scale), buffer, sizeof(buffer)));
									scale_writer.push(format_hex_double(rtm::vector_get_z(scale), buffer, sizeof(buffer)));
								});
								scales_writer.push_newline();
							}
						};
					});
				}
			};
		}

		inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, algorithm_type8 algorithm, const CompressionSettings* settings, const char* acl_filename)
		{
			if (acl_filename == nullptr)
				return "'acl_filename' cannot be NULL!";

			const size_t filename_len = std::strlen(acl_filename);
			if (filename_len < 10 || strncmp(acl_filename + filename_len - 10, ".acl.sjson", 10) != 0)
				return "'acl_filename' file must be an ACL SJSON file of the form: *.acl.sjson";

			std::FILE* file = nullptr;

#ifdef _WIN32
			char path[64 * 1024] = { 0 };
			snprintf(path, get_array_size(path), "\\\\?\\%s", acl_filename);
			fopen_s(&file, path, "w");
#else
			file = fopen(acl_filename, "w");
#endif

			if (file == nullptr)
				return "Failed to open ACL file for writing";

			sjson::FileStreamWriter stream_writer(file);
			sjson::Writer writer(stream_writer);

			writer["version"] = 5;
			writer.insert_newline();

			write_sjson_clip(clip, writer);
			if (settings != nullptr)
				write_sjson_settings(algorithm, *settings, writer);
			write_sjson_bones(skeleton, writer);

			const AnimationClip* base_clip = clip.get_additive_base();
			if (base_clip != nullptr)
				write_sjson_tracks(skeleton, *base_clip, true, writer);

			write_sjson_tracks(skeleton, clip, false, writer);

			std::fclose(file);
			return nullptr;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and no specific compression settings.
	// Returns an error string on failure, null on success.
	//////////////////////////////////////////////////////////////////////////
	inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, const char* acl_filename)
	{
		return acl_impl::write_acl_clip(skeleton, clip, algorithm_type8::uniformly_sampled, nullptr, acl_filename);
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL clip file from a skeleton, a clip,
	// and compression settings.
	// Returns an error string on failure, null on success.
	//////////////////////////////////////////////////////////////////////////
	inline const char* write_acl_clip(const RigidSkeleton& skeleton, const AnimationClip& clip, algorithm_type8 algorithm, const CompressionSettings& settings, const char* acl_filename)
	{
		return acl_impl::write_acl_clip(skeleton, clip, algorithm, &settings, acl_filename);
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL track list file.
	// Returns an error string on failure, null on success.
	//////////////////////////////////////////////////////////////////////////
	inline const char* write_track_list(const track_array& track_list, const char* acl_filename)
	{
		if (acl_filename == nullptr)
			return "'acl_filename' cannot be NULL!";

		const size_t filename_len = std::strlen(acl_filename);
		if (filename_len < 10 || strncmp(acl_filename + filename_len - 10, ".acl.sjson", 10) != 0)
			return "'acl_filename' file must be an ACL SJSON file of the form: *.acl.sjson";

		std::FILE* file = nullptr;

#ifdef _WIN32
		char path[64 * 1024] = { 0 };
		snprintf(path, get_array_size(path), "\\\\?\\%s", acl_filename);
		fopen_s(&file, path, "w");
#else
		file = fopen(acl_filename, "w");
#endif

		if (file == nullptr)
			return "Failed to open ACL file for writing";

		char buffer[32] = { 0 };

		sjson::FileStreamWriter stream_writer(file);
		sjson::Writer writer(stream_writer);

		writer["version"] = 5;
		writer.insert_newline();

		writer["track_list"] = [&](sjson::ObjectWriter& header_writer)
		{
			//header_writer["name"] = track_list.get_name().c_str();
			header_writer["num_samples"] = track_list.get_num_samples_per_track();
			header_writer["sample_rate"] = track_list.get_sample_rate();
			header_writer["is_binary_exact"] = true;
		};
		writer.insert_newline();

		writer["tracks"] = [&](sjson::ArrayWriter& tracks_writer)
		{
			const uint32_t num_tracks = track_list.get_num_tracks();
			if (num_tracks > 0)
				tracks_writer.push_newline();

			for (const track& track_ : track_list)
			{
				tracks_writer.push([&](sjson::ObjectWriter& track_writer)
				{
					//track_writer["name"] = track_.get_name().c_str();
					track_writer["type"] = get_track_type_name(track_.get_type());

					switch (track_.get_type())
					{
					case track_type8::float1f:
					{
						const track_float1f& track__ = track_cast<track_float1f>(track_);
						track_writer["precision"] = track__.get_description().precision;
						track_writer["output_index"] = track__.get_description().output_index;

						track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
						{
							const uint32_t num_samples = track__.get_num_samples();
							if (num_samples > 0)
								data_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
							{
								data_writer.push([&](sjson::ArrayWriter& sample_writer)
								{
									const float sample = track__[sample_index];
									sample_writer.push(acl_impl::format_hex_float(sample, buffer, sizeof(buffer)));
								});
								data_writer.push_newline();
							}
						};
						break;
					}
					case track_type8::float2f:
					{
						const track_float2f& track__ = track_cast<track_float2f>(track_);
						track_writer["precision"] = track__.get_description().precision;
						track_writer["output_index"] = track__.get_description().output_index;

						track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
						{
							const uint32_t num_samples = track__.get_num_samples();
							if (num_samples > 0)
								data_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
							{
								data_writer.push([&](sjson::ArrayWriter& sample_writer)
								{
									const rtm::float2f& sample = track__[sample_index];
									sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
								});
								data_writer.push_newline();
							}
						};
						break;
					}
					case track_type8::float3f:
					{
						const track_float3f& track__ = track_cast<track_float3f>(track_);
						track_writer["precision"] = track__.get_description().precision;
						track_writer["output_index"] = track__.get_description().output_index;

						track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
						{
							const uint32_t num_samples = track__.get_num_samples();
							if (num_samples > 0)
								data_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
							{
								data_writer.push([&](sjson::ArrayWriter& sample_writer)
								{
									const rtm::float3f& sample = track__[sample_index];
									sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.z, buffer, sizeof(buffer)));
								});
								data_writer.push_newline();
							}
						};
						break;
					}
					case track_type8::float4f:
					{
						const track_float4f& track__ = track_cast<track_float4f>(track_);
						track_writer["precision"] = track__.get_description().precision;
						track_writer["output_index"] = track__.get_description().output_index;

						track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
						{
							const uint32_t num_samples = track__.get_num_samples();
							if (num_samples > 0)
								data_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
							{
								data_writer.push([&](sjson::ArrayWriter& sample_writer)
								{
									const rtm::float4f& sample = track__[sample_index];
									sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.z, buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(sample.w, buffer, sizeof(buffer)));
								});
								data_writer.push_newline();
							}
						};
						break;
					}
					case track_type8::vector4f:
					{
						const track_vector4f& track__ = track_cast<track_vector4f>(track_);
						track_writer["precision"] = track__.get_description().precision;
						track_writer["output_index"] = track__.get_description().output_index;

						track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
						{
							const uint32_t num_samples = track__.get_num_samples();
							if (num_samples > 0)
								data_writer.push_newline();

							for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
							{
								data_writer.push([&](sjson::ArrayWriter& sample_writer)
								{
									const rtm::vector4f& sample = track__[sample_index];
									sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(sample), buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(sample), buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(sample), buffer, sizeof(buffer)));
									sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_w(sample), buffer, sizeof(buffer)));
								});
								data_writer.push_newline();
							}
						};
						break;
					}
					default:
						ACL_ASSERT(false, "Unknown track type");
						break;
					}
				});
			}
		};
		writer.insert_newline();

		std::fclose(file);
		return nullptr;
	}
}

ACL_IMPL_FILE_PRAGMA_POP

#endif	// #if defined(SJSON_CPP_WRITER)
