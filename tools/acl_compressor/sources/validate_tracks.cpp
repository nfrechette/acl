////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2021 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl_compressor.h"

#include "acl/core/compressed_tracks.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/compression/compress.h"
#include "acl/compression/convert.h"
#include "acl/compression/track_array.h"
#include "acl/compression/track_error.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/decompression/decompress.h"

using namespace acl;

#if defined(ACL_USE_SJSON) && defined(ACL_HAS_ASSERT_CHECKS)
static bool RTM_SIMD_CALL are_rotations_equal(rtm::quatf_arg0 lhs, rtm::quatf_arg1 rhs, float threshold) RTM_NO_EXCEPT
{
	// Rotations are equal regardless of which half of the hyper-sphere they live on
	const rtm::quatf neg_lhs = rtm::quat_neg(lhs);
	return rtm::quat_near_equal(lhs, rhs, threshold) || rtm::quat_near_equal(neg_lhs, rhs, threshold);
}

template<class debug_track_writer_t>
void validate_transform_tracks(const acl::acl_impl::debug_track_writer& reference, const debug_track_writer_t& tracks, float sample_time, float quat_error_threshold, float vec3_error_threshold)
{
	const uint32_t num_tracks = reference.num_tracks;
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const rtm::qvvf ref_transform = reference.read_qvv(track_index);
		const rtm::qvvf transform = tracks.read_qvv(track_index);

		// If our default sub-track mode is skipped or variable, our lossy transform should equal the ref transform
		// But if the constant mode is used, we must check that as well

		bool is_rotation_matching = are_rotations_equal(ref_transform.rotation, transform.rotation, quat_error_threshold);
		if (!is_rotation_matching && debug_track_writer_t::get_default_rotation_mode() == default_sub_track_mode::constant)
			is_rotation_matching = rtm::quat_near_equal(tracks.get_constant_default_rotation(), transform.rotation, quat_error_threshold);

		bool is_translation_matching = rtm::vector_all_near_equal3(ref_transform.translation, transform.translation, vec3_error_threshold);
		if (!is_translation_matching && debug_track_writer_t::get_default_translation_mode() == default_sub_track_mode::constant)
			is_translation_matching = rtm::vector_all_near_equal3(tracks.get_constant_default_translation(), transform.translation, vec3_error_threshold);

		bool is_scale_matching = rtm::vector_all_near_equal3(ref_transform.scale, transform.scale, vec3_error_threshold);
		if (!is_scale_matching && debug_track_writer_t::get_default_scale_mode() == default_sub_track_mode::constant)
			is_scale_matching = rtm::vector_all_near_equal3(tracks.get_constant_default_scale(), transform.scale, vec3_error_threshold);

		ACL_ASSERT(is_rotation_matching,
			"Failed to sample rotation with decompress_tracks for bone index %u at sample time %.2f. Expected [%.5f, %.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f, %.5f].",
			track_index, sample_time,
			(float)rtm::quat_get_x(ref_transform.rotation), (float)rtm::quat_get_y(ref_transform.rotation), (float)rtm::quat_get_z(ref_transform.rotation), (float)rtm::quat_get_w(ref_transform.rotation),
			(float)rtm::quat_get_x(transform.rotation), (float)rtm::quat_get_y(transform.rotation), (float)rtm::quat_get_z(transform.rotation), (float)rtm::quat_get_w(transform.rotation));

		ACL_ASSERT(is_translation_matching,
			"Failed to sample translation with decompress_tracks for bone index %u at sample time %.2f. Expected [%.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f].",
			track_index, sample_time,
			(float)rtm::vector_get_x(ref_transform.translation), (float)rtm::vector_get_y(ref_transform.translation), (float)rtm::vector_get_z(ref_transform.translation),
			(float)rtm::vector_get_x(transform.translation), (float)rtm::vector_get_y(transform.translation), (float)rtm::vector_get_z(transform.translation));

		ACL_ASSERT(is_scale_matching,
			"Failed to sample scale with decompress_tracks for bone index %u at sample time %.2f. Expected [%.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f].",
			track_index, sample_time,
			(float)rtm::vector_get_x(ref_transform.scale), (float)rtm::vector_get_y(ref_transform.scale), (float)rtm::vector_get_z(ref_transform.scale),
			(float)rtm::vector_get_x(transform.scale), (float)rtm::vector_get_y(transform.scale), (float)rtm::vector_get_z(transform.scale));
	}
}

void validate_accuracy(
	iallocator& allocator,
	const track_array_qvvf& raw_tracks,
	const track_array_qvvf& additive_base_tracks,
	const itransform_error_metric& error_metric,
	const compressed_tracks& compressed_tracks_,
	double regression_error_threshold
	)
{
	using namespace acl_impl;

	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	// When intrinsics aren't used with x86, the floating point arithmetic falls back to
	// using x87 instructions. When this happens, depending on how code is generated some
	// small inaccuracies can pop up because rounding happens when we store to memory.
	// The full pose decompression stores samples into the stack while working with them
	// while the single track decompression does not which causes the issue.
	// With SSE2 and NEON, there are no such rounding issues.
#if !defined(RTM_SSE2_INTRINSICS) && defined(RTM_ARCH_X86)
	const float quat_error_threshold = 0.001F;
	const float vec3_error_threshold = 0.0001F;
#else
	const float quat_error_threshold = 0.0001F;
	const float vec3_error_threshold = 0.0F;
#endif

	// We use the nearest sample to accurately measure the loss that happened, if any
	const sample_rounding_policy rounding_policy = sample_rounding_policy::nearest;

	acl::decompression_context<debug_transform_decompression_settings> context;

	const bool initialized = context.initialize(compressed_tracks_);
	ACL_ASSERT(initialized, "Failed to initialize decompression context"); (void)initialized;

	const track_error error = calculate_compression_error(allocator, raw_tracks, context, error_metric, additive_base_tracks);
	ACL_ASSERT(rtm::scalar_is_finite(error.error), "Returned error is not a finite value"); (void)error;
	ACL_ASSERT(error.error < regression_error_threshold, "Error too high for bone %u: %f at time %f", error.index, error.error, error.sample_time);

	const uint32_t num_tracks = compressed_tracks_.get_num_tracks();
	const float duration = compressed_tracks_.get_finite_duration();
	const float sample_rate = compressed_tracks_.get_sample_rate();

	// Calculate the number of samples from the duration to account for the repeating
	// first frame at the end when wrapping is used
	const uint32_t num_samples = calculate_num_samples(duration, sample_rate);

	debug_track_writer track_writer(allocator, track_type8::qvvf, num_tracks);
	track_writer.initialize_with_defaults(raw_tracks);

	debug_track_writer_constant_defaults track_writer_constant(allocator, track_type8::qvvf, num_tracks);
	// todo handle additive1

	// Use the normal debug_track_writer since it skips default sub-tracks, we can use them
	debug_track_writer_variable_defaults track_writer_variable(allocator, track_type8::qvvf, num_tracks);
	track_writer_variable.default_sub_tracks = track_writer.tracks_typed.qvvf;

	debug_track_writer_per_track_rounding track_writer_per_track_rounding(allocator, track_type8::qvvf, num_tracks);
	track_writer_per_track_rounding.initialize_with_defaults(raw_tracks);

	{
		// Try to decompress something at 0.0, if we have no tracks or samples, it should be handled
		context.seek(0.0F, rounding_policy);
		context.decompress_tracks(track_writer);
		context.decompress_tracks(track_writer_constant);
		context.decompress_tracks(track_writer_variable);
	}

	// Basic sanity checks
	if (num_samples != 0)
	{
		debug_track_writer track_writer_clamped(allocator, track_type8::qvvf, num_tracks);
		track_writer_clamped.initialize_with_defaults(raw_tracks);

		// Make sure clamping works properly at the start of the clip
		context.seek(-0.2F, rounding_policy);
		context.decompress_tracks(track_writer_clamped);

		validate_transform_tracks(track_writer, track_writer_clamped, 0.0F, quat_error_threshold, vec3_error_threshold);

		// Make sure clamping works properly at the end of the clip
		const float last_sample_time = rtm::scalar_min(float(num_samples - 1) / sample_rate, duration);

		// When using the wrap looping policy, the last sample time will yield the first sample since we completely wrap
		context.seek(last_sample_time, rounding_policy);

		context.decompress_tracks(track_writer);

		context.seek(last_sample_time + 1.0F, rounding_policy);
		context.decompress_tracks(track_writer_clamped);

		validate_transform_tracks(track_writer, track_writer_clamped, last_sample_time, quat_error_threshold, vec3_error_threshold);

		// Test a few samples with all rounding modes per track
		const float sample_times[] = { 0.0F, duration * 0.2F, duration * 0.5F, duration * 0.75F, duration };
		const sample_rounding_policy rounding_policies[] = { sample_rounding_policy::none, sample_rounding_policy::floor, sample_rounding_policy::ceil, sample_rounding_policy::nearest };

		for (float sample_time : make_iterator(sample_times))
		{
			for (sample_rounding_policy policy : make_iterator(rounding_policies))
			{
				context.seek(sample_time, policy);
				context.decompress_tracks(track_writer);

				track_writer_per_track_rounding.rounding_policy = policy;
				context.seek(sample_time, sample_rounding_policy::per_track);
				context.decompress_tracks(track_writer_per_track_rounding);

				validate_transform_tracks(track_writer, track_writer_per_track_rounding, sample_time, quat_error_threshold, vec3_error_threshold);
			}
		}
	}

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

		// We use the nearest sample to accurately measure the loss that happened, if any
		context.seek(sample_time, rounding_policy);
		context.decompress_tracks(track_writer);
		context.decompress_tracks(track_writer_constant);
		context.decompress_tracks(track_writer_variable);

		// Make sure constant default sub-tracks match the skipped sub-tracks
		validate_transform_tracks(track_writer, track_writer_constant, sample_time, quat_error_threshold, vec3_error_threshold);

		// Make sure variable default sub-tracks match the skipped sub-tracks
		validate_transform_tracks(track_writer, track_writer_variable, sample_time, quat_error_threshold, vec3_error_threshold);

		// Validate decompress_track against decompress_tracks
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const rtm::qvvf transform0 = track_writer.read_qvv(track_index);

			// Make sure single track decompression matches
			context.decompress_track(track_index, track_writer);
			const rtm::qvvf transform1 = track_writer.read_qvv(track_index);

			// Rotations can differ a bit due to how we normalize during interpolation
			ACL_ASSERT(are_rotations_equal(transform0.rotation, transform1.rotation, quat_error_threshold),
				"Failed to sample rotation with decompress_track for bone index %u at sample index %u. Expected [%.5f, %.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f, %.5f].",
				track_index, sample_index,
				(float)rtm::quat_get_x(transform0.rotation), (float)rtm::quat_get_y(transform0.rotation), (float)rtm::quat_get_z(transform0.rotation), (float)rtm::quat_get_w(transform0.rotation),
				(float)rtm::quat_get_x(transform1.rotation), (float)rtm::quat_get_y(transform1.rotation), (float)rtm::quat_get_z(transform1.rotation), (float)rtm::quat_get_w(transform1.rotation));

			ACL_ASSERT(rtm::vector_all_near_equal3(transform0.translation, transform1.translation, vec3_error_threshold),
				"Failed to sample translation with decompress_track for bone index %u at sample index %u. Expected [%.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f].",
				track_index, sample_index,
				(float)rtm::vector_get_x(transform0.translation), (float)rtm::vector_get_y(transform0.translation), (float)rtm::vector_get_z(transform0.translation),
				(float)rtm::vector_get_x(transform1.translation), (float)rtm::vector_get_y(transform1.translation), (float)rtm::vector_get_z(transform1.translation));

			ACL_ASSERT(rtm::vector_all_near_equal3(transform0.scale, transform1.scale, vec3_error_threshold),
				"Failed to sample scale with decompress_track for bone index %u at sample index %u. Expected [%.5f, %.5f, %.5f], got [%.5f, %.5f, %.5f].",
				track_index, sample_index,
				(float)rtm::vector_get_x(transform0.scale), (float)rtm::vector_get_y(transform0.scale), (float)rtm::vector_get_z(transform0.scale),
				(float)rtm::vector_get_x(transform1.scale), (float)rtm::vector_get_y(transform1.scale), (float)rtm::vector_get_z(transform1.scale));
		}
	}
}

void RTM_SIMD_CALL validate_scalar_tracks(const track_array& raw_tracks, const acl::acl_impl::debug_track_writer& reference, const acl::acl_impl::debug_track_writer& tracks, rtm::vector4f_arg0 regression_error_thresholdv, float sample_time)
{
	const rtm::vector4f zero = rtm::vector_zero();

	const uint32_t num_tracks = reference.num_tracks;
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track& track_ = raw_tracks[track_index];
		const uint32_t output_index = track_.get_output_index();
		if (output_index == k_invalid_track_index)
			continue;	// Track is being stripped, ignore it

		rtm::vector4f error = zero;

		//switch (track_type)
		switch (reference.type)
		{
		case track_type8::float1f:
		{
			const float raw_value = reference.read_float1(track_index);
			const float lossy_value = tracks.read_float1(output_index);
			error = rtm::vector_set(rtm::scalar_abs(raw_value - lossy_value));
			break;
		}
		case track_type8::float2f:
		{
			const rtm::vector4f raw_value = reference.read_float2(track_index);
			const rtm::vector4f lossy_value = tracks.read_float2(output_index);
			error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
			error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::c, rtm::mix4::d>(error, zero);
			break;
		}
		case track_type8::float3f:
		{
			const rtm::vector4f raw_value = reference.read_float3(track_index);
			const rtm::vector4f lossy_value = tracks.read_float3(output_index);
			error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
			error = rtm::vector_mix<rtm::mix4::x, rtm::mix4::y, rtm::mix4::z, rtm::mix4::d>(error, zero);
			break;
		}
		case track_type8::float4f:
		{
			const rtm::vector4f raw_value = reference.read_float4(track_index);
			const rtm::vector4f lossy_value = tracks.read_float4(output_index);
			error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
			break;
		}
		case track_type8::vector4f:
		{
			const rtm::vector4f raw_value = reference.read_vector4(track_index);
			const rtm::vector4f lossy_value = tracks.read_vector4(output_index);
			error = rtm::vector_abs(rtm::vector_sub(raw_value, lossy_value));
			break;
		}
		default:
			ACL_ASSERT(false, "Unsupported track type");
			break;
		}

		(void)error;
		ACL_ASSERT(rtm::vector_is_finite(error), "Returned error is not a finite value");
		ACL_ASSERT(rtm::vector_all_less_than(error, regression_error_thresholdv), "Error too high for track %u at time %f", track_index, sample_time);
	}
}

void validate_accuracy(
	iallocator& allocator,
	const track_array& raw_tracks,
	const compressed_tracks& tracks,
	double regression_error_threshold
	)
{
	using namespace acl_impl;

	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	const float regression_error_thresholdf = static_cast<float>(regression_error_threshold);
	const rtm::vector4f regression_error_thresholdv = rtm::vector_set(regression_error_thresholdf);

	// We use the nearest sample to accurately measure the loss that happened, if any
	const sample_rounding_policy rounding_policy = sample_rounding_policy::nearest;

	const float duration = tracks.get_finite_duration();
	const float sample_rate = tracks.get_sample_rate();
	const uint32_t num_tracks = tracks.get_num_tracks();
	const track_type8 track_type = raw_tracks.get_track_type();

	// Calculate the number of samples from the duration to account for the repeating
	// first frame at the end when wrapping is used
	const uint32_t num_samples = calculate_num_samples(duration, sample_rate);

	ACL_ASSERT(rtm::scalar_near_equal(duration, raw_tracks.get_finite_duration(), 1.0E-7F), "Duration mismatch");
	ACL_ASSERT(sample_rate == raw_tracks.get_sample_rate(), "Sample rate mismatch");
	ACL_ASSERT(num_tracks <= raw_tracks.get_num_tracks(), "Num tracks mismatch");

	decompression_context<debug_scalar_decompression_settings> context;
	context.initialize(tracks);

	debug_track_writer raw_tracks_writer(allocator, track_type, num_tracks);
	debug_track_writer raw_track_writer(allocator, track_type, num_tracks);
	debug_track_writer lossy_tracks_writer(allocator, track_type, num_tracks);
	debug_track_writer lossy_track_writer(allocator, track_type, num_tracks);

	debug_track_writer_per_track_rounding track_writer_per_track_rounding(allocator, track_type, num_tracks);

	{
		// Try to decompress something at 0.0, if we have no tracks or samples, it should be handled
		context.seek(0.0F, rounding_policy);
		context.decompress_tracks(lossy_tracks_writer);
	}

	// Basic sanity checks
	if (num_samples != 0)
	{
		debug_track_writer track_writer_clamped(allocator, track_type, num_tracks);

		// Make sure clamping works properly at the start of the clip
		raw_tracks.sample_tracks(0.0F, rounding_policy, raw_tracks_writer);

		context.seek(-0.2F, rounding_policy);
		context.decompress_tracks(track_writer_clamped);

		validate_scalar_tracks(raw_tracks, raw_tracks_writer, track_writer_clamped, regression_error_thresholdv, -0.2F);

		// Make sure clamping works properly at the end of the clip when we clamp
		const float last_sample_time_clamp = rtm::scalar_min(float(num_samples - 1) / sample_rate, duration);

		raw_tracks.sample_tracks(last_sample_time_clamp, rounding_policy, raw_tracks_writer);

		context.seek(last_sample_time_clamp + 1.0F, rounding_policy);
		context.decompress_tracks(track_writer_clamped);

		validate_scalar_tracks(raw_tracks, raw_tracks_writer, track_writer_clamped, regression_error_thresholdv, last_sample_time_clamp + 1.0F);

		// Test a few samples with all rounding modes per track
		const float sample_times[] = { 0.0F, duration * 0.2F, duration * 0.5F, duration * 0.75F, duration };
		const sample_rounding_policy rounding_policies[] = { sample_rounding_policy::none, sample_rounding_policy::floor, sample_rounding_policy::ceil, sample_rounding_policy::nearest };

		for (float sample_time : make_iterator(sample_times))
		{
			for (sample_rounding_policy policy : make_iterator(rounding_policies))
			{
				context.seek(sample_time, policy);
				context.decompress_tracks(lossy_tracks_writer);

				track_writer_per_track_rounding.rounding_policy = policy;
				context.seek(sample_time, sample_rounding_policy::per_track);
				context.decompress_tracks(track_writer_per_track_rounding);

				validate_scalar_tracks(raw_tracks, lossy_tracks_writer, track_writer_per_track_rounding, regression_error_thresholdv, sample_time);
			}
		}
	}

	// Regression test
	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

		raw_tracks.sample_tracks(sample_time, rounding_policy, raw_tracks_writer);

		context.seek(sample_time, rounding_policy);
		context.decompress_tracks(lossy_tracks_writer);

		// Validate decompress_tracks
		validate_scalar_tracks(raw_tracks, raw_tracks_writer, lossy_tracks_writer, regression_error_thresholdv, sample_time);

		// Validate decompress_track
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& track_ = raw_tracks[track_index];
			const uint32_t output_index = track_.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Track is being stripped, ignore it

			raw_tracks.sample_track(track_index, sample_time, rounding_policy, raw_track_writer);
			context.decompress_track(output_index, lossy_track_writer);

			switch (track_type)
			{
			case track_type8::float1f:
			{
				const float raw_value_ = raw_tracks_writer.read_float1(track_index);
				const float lossy_value_ = lossy_tracks_writer.read_float1(output_index);
				const float raw_value = raw_track_writer.read_float1(track_index);
				const float lossy_value = lossy_track_writer.read_float1(output_index);
				ACL_ASSERT(rtm::scalar_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::scalar_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::scalar_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float2f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float2(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float2(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float2(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float2(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal2(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal2(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal2(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float3f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float3(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float3(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float3(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float3(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal3(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::float4f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_float4(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_float4(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_float4(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_float4(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			case track_type8::vector4f:
			{
				const rtm::vector4f raw_value_ = raw_tracks_writer.read_vector4(track_index);
				const rtm::vector4f lossy_value_ = lossy_tracks_writer.read_vector4(output_index);
				const rtm::vector4f raw_value = raw_track_writer.read_vector4(track_index);
				const rtm::vector4f lossy_value = lossy_track_writer.read_vector4(output_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value, lossy_value, regression_error_thresholdf), "Error too high for track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_value_, raw_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				ACL_ASSERT(rtm::vector_all_near_equal(lossy_value_, lossy_value, 0.00001F), "Failed to sample track %u at time %f", track_index, sample_time);
				break;
			}
			default:
				ACL_ASSERT(false, "Unsupported track type");
				break;
			}
		}
	}
}

void validate_metadata(const track_array& raw_tracks, const compressed_tracks& tracks)
{
	const uint32_t num_tracks = raw_tracks.get_num_tracks();

	// Validate track list name
	const string& raw_list_name = raw_tracks.get_name();
	const char* compressed_list_name = tracks.get_name();
	ACL_ASSERT(raw_list_name == compressed_list_name, "Unexpected track list name");

	// Validate track names
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track& raw_track = raw_tracks[track_index];
		const uint32_t output_index = raw_track.get_output_index();
		if (output_index == k_invalid_track_index)
			continue;	// Stripped

		const string& raw_name = raw_track.get_name();
		const char* compressed_name = tracks.get_track_name(output_index);
		ACL_ASSERT(raw_name == compressed_name, "Unexpected track name");
	}

	if (raw_tracks.get_track_type() == track_type8::qvvf)
	{
		// Specific to transform tracks
		const track_array_qvvf& transform_tracks = track_array_cast<track_array_qvvf>(raw_tracks);

		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track_qvvf& raw_track = transform_tracks[track_index];
			const uint32_t output_index = raw_track.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Stripped

			const track_desc_transformf& raw_desc = raw_track.get_description();
			const uint32_t parent_track_index = raw_desc.parent_index;
			const uint32_t parent_track_output_index = parent_track_index != k_invalid_track_index ? transform_tracks[parent_track_index].get_output_index() : k_invalid_track_index;

			const uint32_t compressed_parent_track_index = tracks.get_parent_track_index(output_index);
			ACL_ASSERT(parent_track_output_index == compressed_parent_track_index, "Unexpected parent track index");

			track_desc_transformf compressed_desc;
			const bool compressed_track_desc_found = tracks.get_track_description(output_index, compressed_desc);
			ACL_ASSERT(compressed_track_desc_found, "Expected track description");
			ACL_ASSERT(output_index == compressed_desc.output_index, "Unexpected output index");
			ACL_ASSERT(parent_track_output_index == compressed_desc.parent_index, "Unexpected parent track index");
			ACL_ASSERT(raw_desc.precision == compressed_desc.precision, "Unexpected precision");
			ACL_ASSERT(raw_desc.shell_distance == compressed_desc.shell_distance, "Unexpected shell_distance");
			ACL_ASSERT(raw_desc.constant_rotation_threshold_angle == compressed_desc.constant_rotation_threshold_angle, "Unexpected constant_rotation_threshold_angle");
			ACL_ASSERT(raw_desc.constant_translation_threshold == compressed_desc.constant_translation_threshold, "Unexpected constant_translation_threshold");
			ACL_ASSERT(raw_desc.constant_scale_threshold == compressed_desc.constant_scale_threshold, "Unexpected constant_scale_threshold");

			ACL_ASSERT(rtm::quat_near_equal(raw_desc.default_value.rotation, compressed_desc.default_value.rotation, 0.0F), "Unexpected default_value.rotation");
			ACL_ASSERT(rtm::vector_all_near_equal3(raw_desc.default_value.translation, compressed_desc.default_value.translation, 0.0F), "Unexpected default_value.translation");
			ACL_ASSERT(rtm::vector_all_near_equal3(raw_desc.default_value.scale, compressed_desc.default_value.scale, 0.0F), "Unexpected default_value.scale");
		}
	}
	else
	{
		// Specific to scalar tracks
		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& raw_track = raw_tracks[track_index];
			const uint32_t output_index = raw_track.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Stripped

			const track_desc_scalarf& raw_desc = raw_track.get_description<track_desc_scalarf>();

			track_desc_scalarf compressed_desc;
			const bool compressed_track_desc_found = tracks.get_track_description(output_index, compressed_desc);
			ACL_ASSERT(compressed_track_desc_found, "Expected track description");
			ACL_ASSERT(output_index == compressed_desc.output_index, "Unexpected output index");
			ACL_ASSERT(raw_desc.precision == compressed_desc.precision, "Unexpected precision");
		}
	}
}

static void compare_raw_with_compressed(iallocator& allocator, const track_array& raw_tracks, const compressed_tracks& compressed_tracks_)
{
	// We use the nearest sample to accurately measure the loss that happened, if any
	const sample_rounding_policy rounding_policy = sample_rounding_policy::nearest;

	const uint32_t num_tracks = raw_tracks.get_num_tracks();

	uint32_t num_output_tracks = 0;
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track& raw_track = raw_tracks[track_index];
		const uint32_t output_index = raw_track.get_output_index();
		if (output_index == k_invalid_track_index)
			continue;	// Stripped

		num_output_tracks++;
	}

	ACL_ASSERT(num_output_tracks == compressed_tracks_.get_num_tracks(), "Unexpected num tracks");
	ACL_ASSERT(raw_tracks.get_num_samples_per_track() == compressed_tracks_.get_num_samples_per_track(), "Unexpected num samples");
	ACL_ASSERT(raw_tracks.get_sample_rate() == compressed_tracks_.get_sample_rate(), "Unexpected sample rate");
	ACL_ASSERT(raw_tracks.get_track_type() == compressed_tracks_.get_track_type(), "Unexpected track type");
	ACL_ASSERT(raw_tracks.get_name() == compressed_tracks_.get_name(), "Unexpected track list name");

	const track_category8 track_category = raw_tracks.get_track_category();
	for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
	{
		const track& raw_track = raw_tracks[track_index];
		const uint32_t output_index = raw_track.get_output_index();
		if (output_index == k_invalid_track_index)
			continue;	// Stripped

		if (track_category == track_category8::scalarf)
		{
			const track_desc_scalarf& raw_desc = raw_track.get_description<track_desc_scalarf>();
			track_desc_scalarf desc;
			compressed_tracks_.get_track_description(output_index, desc);

			ACL_ASSERT(raw_desc.precision == desc.precision, "Unexpected precision");
		}
		else
		{
			const track_desc_transformf& raw_desc = raw_track.get_description<track_desc_transformf>();
			track_desc_transformf desc;
			compressed_tracks_.get_track_description(output_index, desc);

			ACL_ASSERT(raw_desc.parent_index == desc.parent_index, "Unexpected parent index");
			ACL_ASSERT(raw_desc.precision == desc.precision, "Unexpected precision");
			ACL_ASSERT(raw_desc.shell_distance == desc.shell_distance, "Unexpected shell_distance");
			ACL_ASSERT(raw_desc.constant_rotation_threshold_angle == desc.constant_rotation_threshold_angle, "Unexpected constant_rotation_threshold_angle");
			ACL_ASSERT(raw_desc.constant_translation_threshold == desc.constant_translation_threshold, "Unexpected constant_translation_threshold");
			ACL_ASSERT(raw_desc.constant_scale_threshold == desc.constant_scale_threshold, "Unexpected constant_scale_threshold");
			ACL_ASSERT(rtm::quat_near_equal(raw_desc.default_value.rotation, desc.default_value.rotation, 0.0F), "Unexpected default_value.rotation");
			ACL_ASSERT(rtm::vector_all_near_equal3(raw_desc.default_value.translation, desc.default_value.translation, 0.0F), "Unexpected default_value.translation");
			ACL_ASSERT(rtm::vector_all_near_equal3(raw_desc.default_value.scale, desc.default_value.scale, 0.0F), "Unexpected default_value.scale");
		}
	}

	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	acl::decompression_context<acl_impl::raw_sampling_decompression_settings> context;
	context.initialize(compressed_tracks_);

	const track_type8 track_type = raw_tracks.get_track_type();
	acl_impl::debug_track_writer writer(allocator, track_type, num_tracks);

	if (track_category == track_category8::transformf)
		writer.initialize_with_defaults(raw_tracks);

	const uint32_t num_samples = raw_tracks.get_num_samples_per_track();
	const float sample_rate = raw_tracks.get_sample_rate();
	const float duration = raw_tracks.get_finite_duration();

	for (uint32_t sample_index = 0; sample_index < num_samples; ++sample_index)
	{
		const float sample_time = rtm::scalar_min(float(sample_index) / sample_rate, duration);

		context.seek(sample_time, rounding_policy);
		context.decompress_tracks(writer);

		for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
		{
			const track& raw_track = raw_tracks[track_index];
			const uint32_t output_index = raw_track.get_output_index();
			if (output_index == k_invalid_track_index)
				continue;	// Track is stripped

			switch (track_type)
			{
			case track_type8::float1f:
			{
				const float raw_sample = *reinterpret_cast<const float*>(raw_track[sample_index]);
				const float compressed_sample = writer.read_float1(track_index);
				ACL_ASSERT(raw_sample == compressed_sample, "Unexpected sample");
				break;
			}
			case track_type8::float2f:
			{
				const rtm::vector4f raw_sample = rtm::vector_load2(reinterpret_cast<const rtm::float2f*>(raw_track[sample_index]));
				const rtm::vector4f compressed_sample = writer.read_float2(track_index);
				ACL_ASSERT(rtm::vector_all_near_equal2(raw_sample, compressed_sample, 0.0F), "Unexpected sample");
				break;
			}
			case track_type8::float3f:
			{
				const rtm::vector4f raw_sample = rtm::vector_load3(reinterpret_cast<const rtm::float3f*>(raw_track[sample_index]));
				const rtm::vector4f compressed_sample = writer.read_float3(track_index);
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_sample, compressed_sample, 0.0F), "Unexpected sample");
				break;
			}
			case track_type8::float4f:
			{
				const rtm::vector4f raw_sample = rtm::vector_load(reinterpret_cast<const rtm::float4f*>(raw_track[sample_index]));
				const rtm::vector4f compressed_sample = writer.read_float4(track_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_sample, compressed_sample, 0.0F), "Unexpected sample");
				break;
			}
			case track_type8::vector4f:
			{
				const rtm::vector4f raw_sample = *reinterpret_cast<const rtm::vector4f*>(raw_track[sample_index]);
				const rtm::vector4f compressed_sample = writer.read_vector4(track_index);
				ACL_ASSERT(rtm::vector_all_near_equal(raw_sample, compressed_sample, 0.0F), "Unexpected sample");
				break;
			}
			case track_type8::qvvf:
			{
				const rtm::qvvf raw_sample = *reinterpret_cast<const rtm::qvvf*>(raw_track[sample_index]);
				const rtm::qvvf compressed_sample = writer.read_qvv(track_index);

				// Rotations can differ a bit due to how we normalize during interpolation
				ACL_ASSERT(rtm::quat_near_equal(raw_sample.rotation, compressed_sample.rotation, 0.0001F), "Unexpected sample");
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_sample.translation, compressed_sample.translation, 0.0F), "Unexpected sample");
				ACL_ASSERT(rtm::vector_all_near_equal3(raw_sample.scale, compressed_sample.scale, 0.0F), "Unexpected sample");
				break;
			}
			default:
				ACL_ASSERT(false, "Unsupported track type");
				break;
			}
		}
	}
}

void validate_convert(iallocator& allocator, const track_array& raw_tracks)
{
	compressed_tracks* compressed_tracks_ = nullptr;
	error_result result = convert_track_list(allocator, raw_tracks, compressed_tracks_);
	ACL_ASSERT(result.empty() && compressed_tracks_ != nullptr, "Convert failed");

	compare_raw_with_compressed(allocator, raw_tracks, *compressed_tracks_);

	track_array new_raw_tracks;
	result = convert_track_list(allocator, *compressed_tracks_, new_raw_tracks);

	const bool is_input_empty = compressed_tracks_->get_num_tracks() == 0;
	ACL_ASSERT(result.empty() && new_raw_tracks.is_empty() == is_input_empty, "Convert failed");

	compare_raw_with_compressed(allocator, new_raw_tracks, *compressed_tracks_);

	allocator.deallocate(compressed_tracks_, compressed_tracks_->get_size());
}
#endif
