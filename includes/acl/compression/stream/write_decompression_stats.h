#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2019 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/compiler_utils.h"
#include "acl/core/memory_cache.h"
#include "acl/core/scope_profiler.h"
#include "acl/core/utils.h"
#include "acl/algorithm/uniformly_sampled/decoder.h"
#include "acl/compression/output_stats.h"
#include "acl/decompression/default_output_writer.h"

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	constexpr uint32_t k_num_decompression_samples = 100;
	constexpr uint32_t k_num_decompression_evaluations = 100;

	enum class PlaybackDirection
	{
		Forward,
		Backward,
		Random,
	};

	enum class DecompressionFunction
	{
		DecompressPose,
		DecompressBone,
	};

	template<class DecompressionContextType>
	inline void write_decompression_performance_stats(
		StatLogging logging, sjson::ObjectWriter& writer, const char* action_type,
		PlaybackDirection playback_direction, DecompressionFunction decompression_function,
		CompressedClip* compressed_clips[k_num_decompression_evaluations],
		DecompressionContextType* contexts[k_num_decompression_evaluations],
		CPUCacheFlusher* cache_flusher, Transform_32* lossy_pose_transforms)
	{
		const ClipHeader& clip_header = get_clip_header(*compressed_clips[0]);
		const float duration = calculate_duration(clip_header.num_samples, clip_header.sample_rate);
		const uint16_t num_bones = clip_header.num_bones;
		const bool is_cold_cache_profiling = cache_flusher != nullptr;

		float sample_times[k_num_decompression_samples];
		for (uint32_t sample_index = 0; sample_index < k_num_decompression_samples; ++sample_index)
		{
			const float normalized_sample_time = float(sample_index) / float(k_num_decompression_samples - 1);
			sample_times[sample_index] = clamp(normalized_sample_time, 0.0F, 1.0F) * duration;
		}

		switch (playback_direction)
		{
		case PlaybackDirection::Forward:
		default:
			break;
		case PlaybackDirection::Backward:
			std::reverse(&sample_times[0], &sample_times[k_num_decompression_samples]);
			break;
		case PlaybackDirection::Random:
			std::shuffle(&sample_times[0], &sample_times[k_num_decompression_samples], std::default_random_engine(0));
			break;
		}

		DefaultOutputWriter pose_writer(lossy_pose_transforms, num_bones);

		// Initialize and clear our contexts
		for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
			contexts[clip_index]->initialize(*compressed_clips[clip_index]);

		writer[action_type] = [&](sjson::ObjectWriter& action_writer)
		{

			double clip_max_ms = 0.0;
			double clip_min_ms = 1000000.0;
			double clip_total_ms = 0.0;
			double clip_time_ms[k_num_decompression_samples];

			action_writer["data"] = [&](sjson::ArrayWriter& data_writer)
			{
				for (uint32_t sample_index = 0; sample_index < k_num_decompression_samples; ++sample_index)
				{
					const float sample_time = sample_times[sample_index];

					// Clearing the context ensures the decoder cannot reuse any state cached from the last sample.
					if (playback_direction == PlaybackDirection::Random)
					{
						for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
							contexts[clip_index]->initialize(*compressed_clips[clip_index]);
					}

					// Clear the CPU cache if necessary
					if (is_cold_cache_profiling)
					{
						cache_flusher->begin_flushing();
						for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
						{
							cache_flusher->flush_buffer(contexts[clip_index], sizeof(DecompressionContextType));
							cache_flusher->flush_buffer(compressed_clips[clip_index], compressed_clips[clip_index]->get_size());
						}
						cache_flusher->end_flushing();
					}
					else
					{
						// If we want the cache warm, decompress everything once to prime it
						DecompressionContextType* context = contexts[0];
						context->seek(sample_time, SampleRoundingPolicy::None);
						context->decompress_pose(pose_writer);
					}

					// We yield our time slice and wait for a new one before priming the cache
					// to help keep it warm and minimize the risk that we'll be interrupted during decompression
					std::this_thread::sleep_for(std::chrono::nanoseconds(1));

					ScopeProfiler timer;

					for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
					{
						// If we measure with a cold CPU cache, we use a different context every time otherwise we use the first one
						DecompressionContextType* context = is_cold_cache_profiling ? contexts[clip_index] : contexts[0];

						context->seek(sample_time, SampleRoundingPolicy::None);

						switch (decompression_function)
						{
						case DecompressionFunction::DecompressPose:
							context->decompress_pose(pose_writer);
							break;
						case DecompressionFunction::DecompressBone:
							for (uint16_t bone_index = 0; bone_index < num_bones; ++bone_index)
								context->decompress_bone(bone_index, &lossy_pose_transforms[bone_index].rotation, &lossy_pose_transforms[bone_index].translation, &lossy_pose_transforms[bone_index].scale);
							break;
						}
					}

					timer.stop();

					const double elapsed_ms = timer.get_elapsed_milliseconds() / k_num_decompression_evaluations;

					if (are_any_enum_flags_set(logging, StatLogging::ExhaustiveDecompression))
						data_writer.push(elapsed_ms);

					clip_min_ms = min(clip_min_ms, elapsed_ms);
					clip_max_ms = max(clip_max_ms, elapsed_ms);
					clip_total_ms += elapsed_ms;
					clip_time_ms[sample_index] = elapsed_ms;
				}
			};

			std::sort(&clip_time_ms[0], &clip_time_ms[k_num_decompression_samples]);

			action_writer["min_time_ms"] = clip_min_ms;
			action_writer["max_time_ms"] = clip_max_ms;
			action_writer["avg_time_ms"] = clip_total_ms / double(k_num_decompression_samples);
			action_writer["med_time_ms"] = clip_time_ms[k_num_decompression_samples / 2];
		};
	}

	inline void write_memcpy_performance_stats(IAllocator& allocator, sjson::ObjectWriter& writer, CPUCacheFlusher* cache_flusher, Transform_32* lossy_pose_transforms, uint16_t num_bones)
	{
		Transform_32* memcpy_src_transforms = allocate_type_array<Transform_32>(allocator, num_bones);

		double decompression_time_ms = 1000000.0;
		for (uint32_t pass_index = 0; pass_index < 3; ++pass_index)
		{
			if (cache_flusher != nullptr)
			{
				cache_flusher->begin_flushing();
				cache_flusher->flush_buffer(memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				cache_flusher->end_flushing();

				// Now that the cache is cold, yield our time slice and wait for a new one
				// This helps minimize the risk that we'll be interrupted during decompression
				std::this_thread::sleep_for(std::chrono::nanoseconds(1));
			}
			else
			{
				// We yield our time slice and wait for a new one before priming the cache
				// to help keep it warm and minimize the risk that we'll be interrupted during decompression
				std::this_thread::sleep_for(std::chrono::nanoseconds(1));

				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
			}

			double execution_count;
			ScopeProfiler timer;
			if (cache_flusher != nullptr)
			{
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				execution_count = 1.0;
			}
			else
			{
				// Warm cache is too fast, execute multiple times and divide by the count
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(Transform_32) * num_bones);
				execution_count = 10.0;
			}
			timer.stop();

			const double elapsed_ms = timer.get_elapsed_milliseconds() / execution_count;
			decompression_time_ms = min(decompression_time_ms, elapsed_ms);
		}

		writer[cache_flusher != nullptr ? "memcpy_cold" : "memcpy_warm"] = [&](sjson::ObjectWriter& memcpy_writer)
		{
			memcpy_writer["data"] = [&](sjson::ArrayWriter&) {};
			memcpy_writer["min_time_ms"] = decompression_time_ms;
			memcpy_writer["max_time_ms"] = decompression_time_ms;
			memcpy_writer["avg_time_ms"] = decompression_time_ms;
		};

		deallocate_type_array(allocator, memcpy_src_transforms, num_bones);
	}

	template<class DecompressionContextType>
	inline void write_decompression_performance_stats(IAllocator& allocator, CompressedClip* compressed_clips[k_num_decompression_evaluations], DecompressionContextType* contexts[k_num_decompression_evaluations], StatLogging logging, sjson::ObjectWriter& writer)
	{
		CPUCacheFlusher* cache_flusher = allocate_type<CPUCacheFlusher>(allocator);

		const ClipHeader& clip_header = get_clip_header(*compressed_clips[0]);
		Transform_32* lossy_pose_transforms = allocate_type_array<Transform_32>(allocator, clip_header.num_bones);

		const uint32_t num_bytes_per_bone = (4 + 3 + 3) * sizeof(float);	// Rotation, Translation, Scale
		writer["pose_size"] = uint32_t(clip_header.num_bones) * num_bytes_per_bone;

		writer["decompression_time_per_sample"] = [&](sjson::ObjectWriter& per_sample_writer)
		{
			// Cold/Warm CPU cache, memcpy
			write_memcpy_performance_stats(allocator, per_sample_writer, cache_flusher, lossy_pose_transforms, clip_header.num_bones);
			write_memcpy_performance_stats(allocator, per_sample_writer, nullptr, lossy_pose_transforms, clip_header.num_bones);

			// Cold CPU cache, decompress_pose
			write_decompression_performance_stats(logging, per_sample_writer, "forward_pose_cold", PlaybackDirection::Forward, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "backward_pose_cold", PlaybackDirection::Backward, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "random_pose_cold", PlaybackDirection::Random, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);

			// Warm CPU cache, decompress_pose
			write_decompression_performance_stats(logging, per_sample_writer, "forward_pose_warm", PlaybackDirection::Forward, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "backward_pose_warm", PlaybackDirection::Backward, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "random_pose_warm", PlaybackDirection::Random, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, lossy_pose_transforms);

			// Cold CPU cache, decompress_bone
			write_decompression_performance_stats(logging, per_sample_writer, "forward_bone_cold", PlaybackDirection::Forward, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "backward_bone_cold", PlaybackDirection::Backward, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "random_bone_cold", PlaybackDirection::Random, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, lossy_pose_transforms);

			// Warm CPU cache, decompress_bone
			write_decompression_performance_stats(logging, per_sample_writer, "forward_bone_warm", PlaybackDirection::Forward, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "backward_bone_warm", PlaybackDirection::Backward, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, lossy_pose_transforms);
			write_decompression_performance_stats(logging, per_sample_writer, "random_bone_warm", PlaybackDirection::Random, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, lossy_pose_transforms);
		};

		deallocate_type_array(allocator, lossy_pose_transforms, clip_header.num_bones);
		deallocate_type(allocator, cache_flusher);
	}

	inline void write_decompression_performance_stats(IAllocator& allocator, const CompressionSettings& settings, const CompressedClip& compressed_clip, StatLogging logging, sjson::ObjectWriter& writer)
	{
		(void)settings;

		switch (compressed_clip.get_algorithm_type())
		{
		case AlgorithmType8::UniformlySampled:
		{
#if defined(ACL_HAS_ASSERT_CHECKS)
			// If we can, we use a fast-path that simulates what a real game engine would use
			// by disabling the things they normally wouldn't care about like deprecated formats
			// and debugging features
			const bool use_uniform_fast_path = settings.rotation_format == RotationFormat8::QuatDropW_Variable
				&& settings.translation_format == VectorFormat8::Vector3_Variable
				&& settings.scale_format == VectorFormat8::Vector3_Variable
				&& are_all_enum_flags_set(settings.range_reduction, RangeReductionFlags8::AllTracks)
				&& settings.segmenting.enabled;

			ACL_ASSERT(use_uniform_fast_path, "We do not support profiling the debug code path");
#endif

			CompressedClip* compressed_clips[k_num_decompression_evaluations];
			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
			{
				void* clip = allocator.allocate(compressed_clip.get_size(), alignof(CompressedClip));
				std::memcpy(clip, &compressed_clip, compressed_clip.get_size());
				compressed_clips[clip_index] = reinterpret_cast<CompressedClip*>(clip);
			}

			uniformly_sampled::DecompressionContext<uniformly_sampled::DefaultDecompressionSettings>* contexts[k_num_decompression_evaluations];
			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
				contexts[clip_index] = uniformly_sampled::make_decompression_context<uniformly_sampled::DefaultDecompressionSettings>(allocator);

			write_decompression_performance_stats(allocator, compressed_clips, contexts, logging, writer);

			for (uint32_t pass_index = 0; pass_index < k_num_decompression_evaluations; ++pass_index)
				contexts[pass_index]->release();

			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
				allocator.deallocate(compressed_clips[clip_index], compressed_clip.get_size());
			break;
		}
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP

#endif	// #if defined(SJSON_CPP_WRITER)
