#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

// Included only once from track_error.h

#include "acl/core/compressed_tracks.h"
#include "acl/core/error.h"
#include "acl/core/error_result.h"
#include "acl/core/iallocator.h"
#include "acl/core/impl/debug_track_writer.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/compression/impl/track_list_context.h"
#include "acl/decompression/decompress.h"

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <functional>

namespace acl
{
	namespace acl_impl
	{
		inline rtm::vector4f RTM_SIMD_CALL get_scalar_track_error(track_type8 track_type, uint32_t raw_track_index, uint32_t lossy_track_index, const debug_track_writer& raw_tracks_writer, const debug_track_writer& lossy_tracks_writer)
		{
			rtm::vector4f error;
			switch (track_type)
			{
			case track_type8::float1f:
			{
				const float raw_value = raw_tracks_writer.read_float1(raw_track_index);
				const float lossy_value = lossy_tracks_writer.read_float1(lossy_track_index);
				error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
				break;
			}
			case track_type8::float2f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float2(raw_track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float2(lossy_track_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, rtm::vector_zero());
				break;
			}
			case track_type8::float3f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float3(raw_track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float3(lossy_track_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, rtm::vector_zero());
				break;
			}
			case track_type8::float4f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_float4(raw_track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_float4(lossy_track_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				break;
			}
			case track_type8::vector4f:
			{
				const rtm::vector4f raw_value = raw_tracks_writer.read_vector4(raw_track_index);
				const rtm::vector4f lossy_value = lossy_tracks_writer.read_vector4(lossy_track_index);
				error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
				break;
			}
			default:
				ACL_ASSERT(false, "Unsupported track type");
				error = rtm::vector_zero();
				break;
			}

			return error;
		}

		struct calculate_track_error_args
		{
			// Scalar and transforms
			uint32_t num_samples = 0;
			uint32_t num_tracks = 0;
			float duration = 0.0F;
			float sample_rate = 0.0F;
			track_type8 track_type = track_type8::float1f;

			std::function<void(float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)> sample_tracks0;
			std::function<void(float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)> sample_tracks1;

			// Transforms only
			const itransform_error_metric* error_metric = nullptr;
			std::function<uint32_t(uint32_t track_index)> get_parent_index;
			std::function<float(uint32_t track_index)> get_shell_distance;

			// Optional
			uint32_t base_num_samples = 0;
			float base_duration = 0.0F;


			std::function<void(float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)> sample_tracks_base;
			std::function<uint32_t(uint32_t track_index)> get_output_index;

			std::function<void(debug_track_writer& track_writer0, debug_track_writer& track_writer1, debug_track_writer& track_writer_remapped)> remap_output;
		};

		inline track_error calculate_scalar_track_error(iallocator& allocator, const calculate_track_error_args& args)
		{
			const uint32_t num_samples = args.num_samples;
			if (args.num_samples == 0)
				return track_error();	// Cannot measure any error

			const uint32_t num_tracks = args.num_tracks;
			if (args.num_tracks == 0)
				return track_error();	// Cannot measure any error

			track_error result;
			result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

			const float duration = args.duration;
			const float sample_rate = args.sample_rate;
			const track_type8 track_type = args.track_type;

			// We use the nearest sample to accurately measure the loss that happened, if any
			const sample_rounding_policy rounding_policy = sample_rounding_policy::nearest;

			debug_track_writer tracks_writer0(allocator, track_type, num_tracks);
			debug_track_writer tracks_writer1(allocator, track_type, num_tracks);

			// Measure our error
			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

				args.sample_tracks0(sample_time, rounding_policy, tracks_writer0);
				args.sample_tracks1(sample_time, rounding_policy, tracks_writer1);

				// Validate decompress_tracks
				for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
				{
					const uint32_t output_index = args.get_output_index ? args.get_output_index(track_index) : track_index;
					if (output_index == k_invalid_track_index)
						continue;	// Track is being stripped, ignore it

					const rtm::vector4f error = get_scalar_track_error(track_type, track_index, output_index, tracks_writer0, tracks_writer1);

					const float max_error = rtm::vector_get_max_component(error);
					if (max_error > result.error)
					{
						result.error = max_error;
						result.index = track_index;
						result.sample_time = sample_time;
					}
				}
			}

			return result;
		}

		inline track_error calculate_transform_track_error(iallocator& allocator, const calculate_track_error_args& args)
		{
			ACL_ASSERT(args.error_metric != nullptr, "Must have an error metric");
			ACL_ASSERT(args.get_parent_index, "Must be able to query the parent track index");
			ACL_ASSERT(args.get_shell_distance, "Must be able to query the shell distance");

			const uint32_t num_samples = args.num_samples;
			if (num_samples == 0)
				return track_error();	// Cannot measure any error

			const uint32_t num_tracks = args.num_tracks;
			if (num_tracks == 0)
				return track_error();	// Cannot measure any error

			const float clip_duration = args.duration;
			const float sample_rate = args.sample_rate;
			const itransform_error_metric& error_metric = *args.error_metric;
			const uint32_t additive_num_samples = args.base_num_samples;
			const float additive_duration = args.base_duration;

			// Always calculate the error with scale, slower but we don't need to know if we have scale or not
			const bool has_scale = true;

			// We use the nearest sample to accurately measure the loss that happened, if any
			const sample_rounding_policy rounding_policy = sample_rounding_policy::nearest;

			debug_track_writer tracks_writer0(allocator, track_type8::qvvf, num_tracks);
			debug_track_writer tracks_writer1(allocator, track_type8::qvvf, num_tracks);
			debug_track_writer tracks_writer1_remapped(allocator, track_type8::qvvf, num_tracks);
			debug_track_writer tracks_writer_base(allocator, track_type8::qvvf, num_tracks);

			const size_t transform_size = error_metric.get_transform_size(has_scale);
			const bool needs_conversion = error_metric.needs_conversion(has_scale);
			uint8_t* raw_local_pose_converted = nullptr;
			uint8_t* base_local_pose_converted = nullptr;
			uint8_t* lossy_local_pose_converted = nullptr;
			if (needs_conversion)
			{
				raw_local_pose_converted = allocate_type_array_aligned<uint8_t>(allocator, num_tracks * transform_size, 64);
				base_local_pose_converted = allocate_type_array_aligned<uint8_t>(allocator, num_tracks * transform_size, 64);
				lossy_local_pose_converted = allocate_type_array_aligned<uint8_t>(allocator, num_tracks * transform_size, 64);
			}

			uint8_t* raw_object_pose = allocate_type_array_aligned<uint8_t>(allocator, num_tracks * transform_size, 64);
			uint8_t* lossy_object_pose = allocate_type_array_aligned<uint8_t>(allocator, num_tracks * transform_size, 64);

			uint32_t* parent_transform_indices = allocate_type_array<uint32_t>(allocator, num_tracks);
			uint32_t* self_transform_indices = allocate_type_array<uint32_t>(allocator, num_tracks);

			for (uint32_t transform_index = 0; transform_index < num_tracks; ++transform_index)
			{
				const uint32_t parent_index = args.get_parent_index(transform_index);
				parent_transform_indices[transform_index] = parent_index;
				self_transform_indices[transform_index] = transform_index;
			}

			void* raw_local_pose_ = needs_conversion ? (void*)raw_local_pose_converted : (void*)tracks_writer0.tracks_typed.qvvf;
			const void* base_local_pose_ = needs_conversion ? (void*)base_local_pose_converted : (void*)tracks_writer_base.tracks_typed.qvvf;
			void* lossy_local_pose_ = needs_conversion ? (void*)lossy_local_pose_converted : (void*)tracks_writer1_remapped.tracks_typed.qvvf;

			itransform_error_metric::convert_transforms_args convert_transforms_args_raw;
			convert_transforms_args_raw.dirty_transform_indices = self_transform_indices;
			convert_transforms_args_raw.num_dirty_transforms = num_tracks;
			convert_transforms_args_raw.transforms = tracks_writer0.tracks_typed.qvvf;
			convert_transforms_args_raw.num_transforms = num_tracks;

			itransform_error_metric::convert_transforms_args convert_transforms_args_base = convert_transforms_args_raw;
			convert_transforms_args_base.transforms = tracks_writer_base.tracks_typed.qvvf;

			itransform_error_metric::convert_transforms_args convert_transforms_args_lossy = convert_transforms_args_raw;
			convert_transforms_args_lossy.transforms = tracks_writer1_remapped.tracks_typed.qvvf;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_raw;
			apply_additive_to_base_args_raw.dirty_transform_indices = self_transform_indices;
			apply_additive_to_base_args_raw.num_dirty_transforms = num_tracks;
			apply_additive_to_base_args_raw.local_transforms = raw_local_pose_;
			apply_additive_to_base_args_raw.base_transforms = base_local_pose_;
			apply_additive_to_base_args_raw.num_transforms = num_tracks;

			itransform_error_metric::apply_additive_to_base_args apply_additive_to_base_args_lossy = apply_additive_to_base_args_raw;
			apply_additive_to_base_args_lossy.local_transforms = lossy_local_pose_;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_raw;
			local_to_object_space_args_raw.dirty_transform_indices = self_transform_indices;
			local_to_object_space_args_raw.num_dirty_transforms = num_tracks;
			local_to_object_space_args_raw.parent_transform_indices = parent_transform_indices;
			local_to_object_space_args_raw.local_transforms = raw_local_pose_;
			local_to_object_space_args_raw.num_transforms = num_tracks;

			itransform_error_metric::local_to_object_space_args local_to_object_space_args_lossy = local_to_object_space_args_raw;
			local_to_object_space_args_lossy.local_transforms = lossy_local_pose_;

			track_error result;
			result.error = -1.0F;		// Can never have a negative error, use -1 so the first sample is used

			for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
			{
				const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, clip_duration);

				// Sample our tracks
				args.sample_tracks0(sample_time, rounding_policy, tracks_writer0);
				args.sample_tracks1(sample_time, rounding_policy, tracks_writer1);

				// Maybe remap them
				if (args.remap_output)
					args.remap_output(tracks_writer0, tracks_writer1, tracks_writer1_remapped);
				else
					std::memcpy(tracks_writer1_remapped.tracks_typed.qvvf, tracks_writer1.tracks_typed.qvvf, sizeof(rtm::qvvf) * num_tracks);

				if (needs_conversion)
				{
					error_metric.convert_transforms(convert_transforms_args_raw, raw_local_pose_converted);
					error_metric.convert_transforms(convert_transforms_args_lossy, lossy_local_pose_converted);
				}

				if (args.sample_tracks_base)
				{
					const float normalized_sample_time = additive_num_samples > 1 ? (sample_time / clip_duration) : 0.0F;
					const float additive_sample_time = additive_num_samples > 1 ? (normalized_sample_time * additive_duration) : 0.0F;
					args.sample_tracks_base(additive_sample_time, rounding_policy, tracks_writer_base);

					if (needs_conversion)
						error_metric.convert_transforms(convert_transforms_args_base, base_local_pose_converted);

					error_metric.apply_additive_to_base(apply_additive_to_base_args_raw, raw_local_pose_);
					error_metric.apply_additive_to_base(apply_additive_to_base_args_lossy, lossy_local_pose_);
				}

				error_metric.local_to_object_space(local_to_object_space_args_raw, raw_object_pose);
				error_metric.local_to_object_space(local_to_object_space_args_lossy, lossy_object_pose);

				for (uint32_t bone_index = 0; bone_index < num_tracks; ++bone_index)
				{
					const float shell_distance = args.get_shell_distance(bone_index);

					itransform_error_metric::calculate_error_args calculate_error_args;
					calculate_error_args.transform0 = raw_object_pose + (bone_index * transform_size);
					calculate_error_args.transform1 = lossy_object_pose + (bone_index * transform_size);
					calculate_error_args.construct_sphere_shell(shell_distance);

					const float error = rtm::scalar_cast(error_metric.calculate_error(calculate_error_args));

					if (error > result.error)
					{
						result.error = error;
						result.index = bone_index;
						result.sample_time = sample_time;
					}
				}
			}

			deallocate_type_array(allocator, raw_local_pose_converted, num_tracks * transform_size);
			deallocate_type_array(allocator, base_local_pose_converted, num_tracks * transform_size);
			deallocate_type_array(allocator, lossy_local_pose_converted, num_tracks * transform_size);
			deallocate_type_array(allocator, raw_object_pose, num_tracks * transform_size);
			deallocate_type_array(allocator, lossy_object_pose, num_tracks * transform_size);
			deallocate_type_array(allocator, parent_transform_indices, num_tracks);
			deallocate_type_array(allocator, self_transform_indices, num_tracks);

			return result;
		}

		inline track_error invalid_track_error()
		{
			track_error result;
			result.index = ~0U;
			result.error = -1.0F;
			result.sample_time = -1.0F;
			return result;
		}
	}

	template<class decompression_context_type, acl_impl::is_decompression_context<decompression_context_type>>
	inline track_error calculate_compression_error(iallocator& allocator, const track_array& raw_tracks, decompression_context_type& context)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(context.is_initialized(), "Context isn't initialized");

		if (raw_tracks.get_track_type() == track_type8::qvvf)
			return invalid_track_error();	// Only supports scalar tracks

		calculate_track_error_args args;
		args.num_samples = raw_tracks.get_num_samples_per_track();
		args.num_tracks = raw_tracks.get_num_tracks();
		args.duration = raw_tracks.get_duration();
		args.sample_rate = raw_tracks.get_sample_rate();
		args.track_type = raw_tracks.get_track_type();

		args.sample_tracks0 = [&raw_tracks](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		args.sample_tracks1 = [&context](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			context.seek(sample_time, rounding_policy);
			context.decompress_tracks(track_writer);
		};

		args.get_output_index = [&raw_tracks](uint32_t track_index)
		{
			const track& track_ = raw_tracks[track_index];
			return track_.get_output_index();
		};

		return calculate_scalar_track_error(allocator, args);
	}

	template<class decompression_context_type, acl_impl::is_decompression_context<decompression_context_type>>
	inline track_error calculate_compression_error(iallocator& allocator, const track_array& raw_tracks, decompression_context_type& context, const itransform_error_metric& error_metric)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(context.is_initialized(), "Context isn't initialized");

		calculate_track_error_args args;
		args.num_samples = raw_tracks.get_num_samples_per_track();
		args.num_tracks = raw_tracks.get_num_tracks();
		args.duration = raw_tracks.get_duration();
		args.sample_rate = raw_tracks.get_sample_rate();
		args.track_type = raw_tracks.get_track_type();

		args.sample_tracks0 = [&raw_tracks](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		args.sample_tracks1 = [&context](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			context.seek(sample_time, rounding_policy);
			context.decompress_tracks(track_writer);
		};

		args.get_output_index = [&raw_tracks](uint32_t track_index)
		{
			const track& track_ = raw_tracks[track_index];
			return track_.get_output_index();
		};

		if (raw_tracks.get_track_type() != track_type8::qvvf)
			return calculate_scalar_track_error(allocator, args);

		uint32_t num_output_bones = 0;
		uint32_t* output_bone_mapping = create_output_track_mapping(allocator, raw_tracks, num_output_bones);

		args.error_metric = &error_metric;

		args.get_parent_index = [&raw_tracks](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks[track_index]);
			return track_.get_description().parent_index;
		};

		args.get_shell_distance = [&raw_tracks](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks[track_index]);
			return track_.get_description().shell_distance;
		};

		args.remap_output = [output_bone_mapping, num_output_bones](debug_track_writer& track_writer0, debug_track_writer& track_writer1, debug_track_writer& track_writer_remapped)
		{
			// Perform remapping by copying the raw pose first and we overwrite with the decompressed pose if
			// the data is available
			std::memcpy(track_writer_remapped.tracks_typed.qvvf, track_writer0.tracks_typed.qvvf, sizeof(rtm::qvvf) * track_writer_remapped.num_tracks);
			for (uint32_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint32_t bone_index = output_bone_mapping[output_index];
				track_writer_remapped.tracks_typed.qvvf[bone_index] = track_writer1.tracks_typed.qvvf[output_index];
			}
		};

		const track_error result = calculate_transform_track_error(allocator, args);

		deallocate_type_array(allocator, output_bone_mapping, num_output_bones);

		return result;
	}

	template<class decompression_context_type, acl_impl::is_decompression_context<decompression_context_type>>
	inline track_error calculate_compression_error(iallocator& allocator, const track_array_qvvf& raw_tracks, decompression_context_type& context, const itransform_error_metric& error_metric, const track_array_qvvf& additive_base_tracks)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(context.is_initialized(), "Context isn't initialized");

		calculate_track_error_args args;
		args.num_samples = raw_tracks.get_num_samples_per_track();
		args.num_tracks = raw_tracks.get_num_tracks();
		args.duration = raw_tracks.get_duration();
		args.sample_rate = raw_tracks.get_sample_rate();
		args.track_type = raw_tracks.get_track_type();

		args.sample_tracks0 = [&raw_tracks](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		args.sample_tracks1 = [&context](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			context.seek(sample_time, rounding_policy);
			context.decompress_tracks(track_writer);
		};

		args.get_output_index = [&raw_tracks](uint32_t track_index)
		{
			const track& track_ = raw_tracks[track_index];
			return track_.get_output_index();
		};

		uint32_t num_output_bones = 0;
		uint32_t* output_bone_mapping = create_output_track_mapping(allocator, raw_tracks, num_output_bones);

		args.error_metric = &error_metric;

		args.get_parent_index = [&raw_tracks](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks[track_index]);
			return track_.get_description().parent_index;
		};

		args.get_shell_distance = [&raw_tracks](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks[track_index]);
			return track_.get_description().shell_distance;
		};

		args.remap_output = [output_bone_mapping, num_output_bones](debug_track_writer& track_writer0, debug_track_writer& track_writer1, debug_track_writer& track_writer_remapped)
		{
			// Perform remapping by copying the raw pose first and we overwrite with the decompressed pose if
			// the data is available
			std::memcpy(track_writer_remapped.tracks_typed.qvvf, track_writer0.tracks_typed.qvvf, sizeof(rtm::qvvf) * track_writer_remapped.num_tracks);
			for (uint32_t output_index = 0; output_index < num_output_bones; ++output_index)
			{
				const uint32_t bone_index = output_bone_mapping[output_index];
				track_writer_remapped.tracks_typed.qvvf[bone_index] = track_writer1.tracks_typed.qvvf[output_index];
			}
		};

		if (!additive_base_tracks.is_empty())
		{
			args.base_num_samples = additive_base_tracks.get_num_samples_per_track();
			args.base_duration = additive_base_tracks.get_duration();

			args.sample_tracks_base = [&additive_base_tracks](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
			{
				additive_base_tracks.sample_tracks(sample_time, rounding_policy, track_writer);
			};
		}

		const track_error result = calculate_transform_track_error(allocator, args);

		deallocate_type_array(allocator, output_bone_mapping, num_output_bones);

		return result;
	}

	template<class decompression_context_type0, class decompression_context_type1, acl_impl::is_decompression_context<decompression_context_type0>, acl_impl::is_decompression_context<decompression_context_type1>>
	inline track_error calculate_compression_error(iallocator& allocator, decompression_context_type0& context0, decompression_context_type1& context1)
	{
		using namespace acl_impl;

		ACL_ASSERT(context0.is_initialized(), "Context isn't initialized");
		ACL_ASSERT(context1.is_initialized(), "Context isn't initialized");

		const compressed_tracks* tracks0 = context0.get_compressed_tracks();

		if (tracks0->get_track_type() == track_type8::qvvf)
			return invalid_track_error();	// Only supports scalar tracks

		calculate_track_error_args args;
		args.num_samples = tracks0->get_num_samples_per_track();
		args.num_tracks = tracks0->get_num_tracks();
		args.duration = tracks0->get_duration();
		args.sample_rate = tracks0->get_sample_rate();
		args.track_type = tracks0->get_track_type();

		args.sample_tracks0 = [&context0](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			context0.seek(sample_time, rounding_policy);
			context0.decompress_tracks(track_writer);
		};

		args.sample_tracks1 = [&context1](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			context1.seek(sample_time, rounding_policy);
			context1.decompress_tracks(track_writer);
		};

		return calculate_scalar_track_error(allocator, args);
	}

	inline track_error calculate_compression_error(iallocator& allocator, const track_array& raw_tracks0, const track_array& raw_tracks1)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks0.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(raw_tracks1.is_valid().empty(), "Raw tracks are invalid");

		if (raw_tracks0.get_track_type() == track_type8::qvvf)
			return invalid_track_error();	// Only supports scalar tracks

		calculate_track_error_args args;
		args.num_samples = raw_tracks0.get_num_samples_per_track();
		args.num_tracks = raw_tracks0.get_num_tracks();
		args.duration = raw_tracks0.get_duration();
		args.sample_rate = raw_tracks0.get_sample_rate();
		args.track_type = raw_tracks0.get_track_type();

		args.sample_tracks0 = [&raw_tracks0](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks0.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		args.sample_tracks1 = [&raw_tracks1](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks1.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		return calculate_scalar_track_error(allocator, args);
	}

	inline track_error calculate_compression_error(iallocator& allocator, const track_array& raw_tracks0, const track_array& raw_tracks1, const itransform_error_metric& error_metric)
	{
		using namespace acl_impl;

		ACL_ASSERT(raw_tracks0.is_valid().empty(), "Raw tracks are invalid");
		ACL_ASSERT(raw_tracks1.is_valid().empty(), "Raw tracks are invalid");

		calculate_track_error_args args;
		args.num_samples = raw_tracks0.get_num_samples_per_track();
		args.num_tracks = raw_tracks0.get_num_tracks();
		args.duration = raw_tracks0.get_duration();
		args.sample_rate = raw_tracks0.get_sample_rate();
		args.track_type = raw_tracks0.get_track_type();

		args.sample_tracks0 = [&raw_tracks0](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks0.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		args.sample_tracks1 = [&raw_tracks1](float sample_time, sample_rounding_policy rounding_policy, debug_track_writer& track_writer)
		{
			raw_tracks1.sample_tracks(sample_time, rounding_policy, track_writer);
		};

		if (raw_tracks0.get_track_type() != track_type8::qvvf)
			return calculate_scalar_track_error(allocator, args);

		args.error_metric = &error_metric;

		args.get_parent_index = [&raw_tracks0](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks0[track_index]);
			return track_.get_description().parent_index;
		};

		args.get_shell_distance = [&raw_tracks0](uint32_t track_index)
		{
			const track_qvvf& track_ = track_cast<track_qvvf>(raw_tracks0[track_index]);
			return track_.get_description().shell_distance;
		};

		return calculate_transform_track_error(allocator, args);
	}
}
