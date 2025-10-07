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

#if defined(ACL_USE_SJSON)

#include "acl/version.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/track_array.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/iallocator.h"
#include "acl/core/error.h"
#include "acl/core/track_desc.h"

#include <rtm/quatd.h>
#include <rtm/vector4d.h>

#include <sjson/writer.h>

#include <cstdint>
#include <cinttypes>
#include <cstdio>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

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

		inline void write_sjson_settings(const compression_settings& settings, sjson::Writer& writer)
		{
			writer["settings"] = [&](sjson::ObjectWriter& settings_writer)
			{
				settings_writer["level"] = get_compression_level_name(settings.level);
				settings_writer["rotation_format"] = get_rotation_format_name(settings.rotation_format);
				settings_writer["translation_format"] = get_vector_format_name(settings.translation_format);
				settings_writer["scale_format"] = get_vector_format_name(settings.scale_format);
			};
			writer.insert_newline();
		}

		inline void write_sjson_header(const track_array& track_list, sjson::Writer& writer)
		{
			writer["version"] = 6;
			writer.insert_newline();

			writer["track_list"] = [&](sjson::ObjectWriter& header_writer)
			{
				header_writer["name"] = track_list.get_name().c_str();
				header_writer["num_samples"] = track_list.get_num_samples_per_track();
				header_writer["sample_rate"] = track_list.get_sample_rate();
				header_writer["is_binary_exact"] = true;

			#if 0	// TODO
				clip_writer["additive_format"] = get_additive_clip_format_name(clip.get_additive_format());

				const AnimationClip* base_clip = clip.get_additive_base();
				if (base_clip != nullptr)
				{
					clip_writer["additive_base_name"] = base_clip->get_name().c_str();
					clip_writer["additive_base_num_samples"] = base_clip->get_num_samples();
					clip_writer["additive_base_sample_rate"] = base_clip->get_sample_rate();
				}
			#endif
			};
			writer.insert_newline();
		}

		inline void write_sjson_track_desc(const track_desc_scalarf& desc, sjson::ObjectWriter& writer)
		{
			char buffer[32] = { 0 };

			writer["precision"] = format_hex_float(desc.precision, buffer, sizeof(buffer));
			writer["output_index"] = desc.output_index;
		}

		inline void write_sjson_track_desc(const track_desc_transformf& desc, sjson::ObjectWriter& writer)
		{
			char buffer[32] = { 0 };

			writer["precision"] = format_hex_float(desc.precision, buffer, sizeof(buffer));
			writer["output_index"] = desc.output_index;
			writer["parent_index"] = desc.parent_index;
			writer["shell_distance"] = format_hex_float(desc.shell_distance, buffer, sizeof(buffer));
			writer["bind_rotation"] = [&](sjson::ArrayWriter& rotation_writer)
			{
				rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_x(desc.default_value.rotation), buffer, sizeof(buffer)));
				rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_y(desc.default_value.rotation), buffer, sizeof(buffer)));
				rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_z(desc.default_value.rotation), buffer, sizeof(buffer)));
				rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_w(desc.default_value.rotation), buffer, sizeof(buffer)));
			};
			writer["bind_translation"] = [&](sjson::ArrayWriter& translation_writer)
			{
				translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(desc.default_value.translation), buffer, sizeof(buffer)));
				translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(desc.default_value.translation), buffer, sizeof(buffer)));
				translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(desc.default_value.translation), buffer, sizeof(buffer)));
			};
			writer["bind_scale"] = [&](sjson::ArrayWriter& scale_writer)
			{
				scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(desc.default_value.scale), buffer, sizeof(buffer)));
				scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(desc.default_value.scale), buffer, sizeof(buffer)));
				scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(desc.default_value.scale), buffer, sizeof(buffer)));
			};
		}

		inline void write_sjson_scalar_track_sample_value(float sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push(acl_impl::format_hex_float(sample, buffer, sizeof(buffer)));
		}

		inline void write_sjson_scalar_track_sample_value(const rtm::float2f& sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
		}

		inline void write_sjson_scalar_track_sample_value(const rtm::float3f& sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.z, buffer, sizeof(buffer)));
		}

		inline void write_sjson_scalar_track_sample_value(const rtm::float4f& sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push(acl_impl::format_hex_float(sample.x, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.y, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.z, buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(sample.w, buffer, sizeof(buffer)));
		}

		inline void write_sjson_scalar_track_sample_value(const rtm::vector4f& sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(sample), buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(sample), buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(sample), buffer, sizeof(buffer)));
			sample_writer.push(acl_impl::format_hex_float(rtm::vector_get_w(sample), buffer, sizeof(buffer)));
		}

		inline void write_sjson_scalar_track_sample_value(const rtm::qvvf& sample, char (&buffer)[32], sjson::ArrayWriter& sample_writer)
		{
			sample_writer.push([&](sjson::ArrayWriter& rotation_writer)
				{
					rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_x(sample.rotation), buffer, sizeof(buffer)));
					rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_y(sample.rotation), buffer, sizeof(buffer)));
					rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_z(sample.rotation), buffer, sizeof(buffer)));
					rotation_writer.push(acl_impl::format_hex_float(rtm::quat_get_w(sample.rotation), buffer, sizeof(buffer)));
				});
			sample_writer.push([&](sjson::ArrayWriter& translation_writer)
				{
					translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(sample.translation), buffer, sizeof(buffer)));
					translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(sample.translation), buffer, sizeof(buffer)));
					translation_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(sample.translation), buffer, sizeof(buffer)));
				});
			sample_writer.push([&](sjson::ArrayWriter& scale_writer)
				{
					scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_x(sample.scale), buffer, sizeof(buffer)));
					scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_y(sample.scale), buffer, sizeof(buffer)));
					scale_writer.push(acl_impl::format_hex_float(rtm::vector_get_z(sample.scale), buffer, sizeof(buffer)));
				});
		}

		template<typename sample_type>
		inline void write_sjson_scalar_track_sample_ex(const sample_constant_data_t<sample_type>& sample, char (&buffer)[32], sjson::ArrayWriter& value_writer)
		{
			write_sjson_scalar_track_sample_value(sample.value, buffer, value_writer);
		}

		template<typename sample_type>
		inline void write_sjson_scalar_track_sample_ex(const sample_linear_data_t<sample_type>& sample, char (&buffer)[32], sjson::ArrayWriter& value_writer)
		{
			write_sjson_scalar_track_sample_value(sample.value, buffer, value_writer);
		}

		// Scalar sample
		template<typename sample_type>
		inline void write_sjson_scalar_track_sample(const sample_type& sample, char (&buffer)[32], sjson::ArrayWriter& data_writer)
		{
			data_writer.push([&](sjson::ArrayWriter& sample_writer)
				{
					write_sjson_scalar_track_sample_value(sample, buffer, sample_writer);
				});
		}

		// Extended sample
		template<typename sample_type>
		inline void write_sjson_scalar_track_sample(const track_extended_sample_t<sample_type>& sample, char (&buffer)[32], sjson::ArrayWriter& data_writer)
		{
			data_writer.push([&](sjson::ObjectWriter& sample_writer)
				{
					sample_writer["interpolator"] = get_sample_interpolator_name(sample.interpolator);

					sample_writer["value"] = [&](sjson::ArrayWriter& value_writer)
						{
							switch (sample.interpolator)
							{
							case sample_interpolator_t::constant:
								write_sjson_scalar_track_sample_ex(sample.data.constant, buffer, value_writer);
								break;
							case sample_interpolator_t::linear:
								write_sjson_scalar_track_sample_ex(sample.data.linear, buffer, value_writer);
								break;
							default:
								ACL_ASSERT(false, "Unknown sample interpolator type");
								break;
							}
						};
				});
		}

		template<class scalar_track_t>
		inline void write_sjson_scalar_track(const track& track_, char (&buffer)[32], sjson::ObjectWriter& track_writer)
		{
			const scalar_track_t& track__ = track_cast<scalar_track_t>(track_);
			write_sjson_track_desc(track__.get_description(), track_writer);

			track_writer["data"] = [&](sjson::ArrayWriter& data_writer)
			{
				const uint32_t num_samples = track__.get_num_samples();
				if (num_samples > 0)
					data_writer.push_newline();

				for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
				{
					const auto& sample = track__[sample_index];
					write_sjson_scalar_track_sample(sample, buffer, data_writer);

					data_writer.push_newline();
				}
			};
		}

		inline void write_sjson_tracks(const track_array& track_list, sjson::Writer& writer)
		{
			char buffer[32] = { 0 };

			writer["tracks"] = [&](sjson::ArrayWriter& tracks_writer)
			{
				const uint32_t num_tracks = track_list.get_num_tracks();
				if (num_tracks > 0)
					tracks_writer.push_newline();

				for (const track& track_ : track_list)
				{
					tracks_writer.push([&](sjson::ObjectWriter& track_writer)
						{
							track_writer["name"] = track_.get_name().c_str();
							track_writer["type"] = get_track_type_name(track_.get_type());
							track_writer["interpolator"] = get_track_interpolator_name(track_.get_interpolator());

							switch (track_.get_type())
							{
							case track_type8::float1f:
							{
								if (track_.get_interpolator() == track_interpolator_t::linear)
									write_sjson_scalar_track<track_float1f>(track_, buffer, track_writer);
								else
									write_sjson_scalar_track<track_float1f_ex>(track_, buffer, track_writer);
								break;
							}
							case track_type8::float2f:
							{
								if (track_.get_interpolator() == track_interpolator_t::linear)
									write_sjson_scalar_track<track_float2f>(track_, buffer, track_writer);
								else
									write_sjson_scalar_track<track_float2f_ex>(track_, buffer, track_writer);
								break;
							}
							case track_type8::float3f:
							{
								if (track_.get_interpolator() == track_interpolator_t::linear)
									write_sjson_scalar_track<track_float3f>(track_, buffer, track_writer);
								else
									write_sjson_scalar_track<track_float3f_ex>(track_, buffer, track_writer);
								break;
							}
							case track_type8::float4f:
							{
								if (track_.get_interpolator() == track_interpolator_t::linear)
									write_sjson_scalar_track<track_float4f>(track_, buffer, track_writer);
								else
									write_sjson_scalar_track<track_float4f_ex>(track_, buffer, track_writer);
								break;
							}
							case track_type8::vector4f:
							{
								if (track_.get_interpolator() == track_interpolator_t::linear)
									write_sjson_scalar_track<track_vector4f>(track_, buffer, track_writer);
								else
									write_sjson_scalar_track<track_vector4f_ex>(track_, buffer, track_writer);
								break;
							}
							case track_type8::qvvf:
							{
								ACL_ASSERT(track_.get_interpolator() == track_interpolator_t::linear, "Unsupported track interpolator for qvvf track type");
								write_sjson_scalar_track<track_qvvf>(track_, buffer, track_writer);
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
		}

		inline error_result write_track_list(const track_array& track_list, const compression_settings* settings, const char* acl_filename)
		{
			if (acl_filename == nullptr)
				return error_result("'acl_filename' cannot be NULL!");

			const size_t filename_len = std::strlen(acl_filename);
			if (filename_len < 10 || strncmp(acl_filename + filename_len - 10, ".acl.sjson", 10) != 0)
				return error_result("'acl_filename' file must be an ACL SJSON file of the form: *.acl.sjson");

			std::FILE* file = nullptr;

#ifdef _WIN32
			char path[1 * 1024] = { 0 };
			snprintf(path, get_array_size(path), "\\\\?\\%s", acl_filename);
			fopen_s(&file, path, "w");
#else
			file = fopen(acl_filename, "w");
#endif

			if (file == nullptr)
				return error_result("Failed to open ACL file for writing");

			sjson::FileStreamWriter stream_writer(file);
			sjson::Writer writer(stream_writer);

			write_sjson_header(track_list, writer);

			if (settings != nullptr)
				write_sjson_settings(*settings, writer);

			write_sjson_tracks(track_list, writer);

			std::fclose(file);
			return error_result();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL track list file.
	//////////////////////////////////////////////////////////////////////////
	inline error_result write_track_list(const track_array& track_list, const char* acl_filename)
	{
		return acl_impl::write_track_list(track_list, nullptr, acl_filename);
	}

	//////////////////////////////////////////////////////////////////////////
	// Write out an SJSON ACL track list file.
	//////////////////////////////////////////////////////////////////////////
	inline error_result write_track_list(const track_array& track_list, const compression_settings& settings, const char* acl_filename)
	{
		return acl_impl::write_track_list(track_list, &settings, acl_filename);
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

ACL_IMPL_FILE_PRAGMA_POP

#endif	// #if defined(ACL_USE_SJSON)
