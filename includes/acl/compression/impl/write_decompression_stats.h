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

#include "acl/core/compressed_tracks_version.h"
#include "acl/core/scope_profiler.h"
#include "acl/core/track_formats.h"
#include "acl/core/utils.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/memory_cache.h"
#include "acl/compression/output_stats.h"
#include "acl/decompression/decompress.h"

#include <rtm/scalard.h>
#include <rtm/scalarf.h>

#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	namespace acl_impl
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
			stat_logging logging, sjson::ObjectWriter& writer, const char* action_type,
			PlaybackDirection playback_direction, DecompressionFunction decompression_function,
			compressed_tracks* compressed_clips[k_num_decompression_evaluations],
			DecompressionContextType* contexts[k_num_decompression_evaluations],
			CPUCacheFlusher* cache_flusher, debug_track_writer& pose_writer)
		{
			const uint32_t num_tracks = compressed_clips[0]->get_num_tracks();
			const float duration = compressed_clips[0]->get_duration();
			const bool is_cold_cache_profiling = cache_flusher != nullptr;

			float sample_times[k_num_decompression_samples];
			for (uint32_t sample_index = 0; sample_index < k_num_decompression_samples; ++sample_index)
			{
				const float normalized_sample_time = float(sample_index) / float(k_num_decompression_samples - 1);
				sample_times[sample_index] = rtm::scalar_clamp(normalized_sample_time, 0.0F, 1.0F) * duration;
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

			// Initialize and clear our contexts
			bool init_success = true;
			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
				init_success |= contexts[clip_index]->initialize(*compressed_clips[clip_index]);

			ACL_ASSERT(init_success, "Failed to initialize decompression context");
			if (!init_success)
				return;

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
							context->seek(sample_time, sample_rounding_policy::none);
							context->decompress_tracks(pose_writer);
						}

						// We yield our time slice and wait for a new one before priming the cache
						// to help keep it warm and minimize the risk that we'll be interrupted during decompression
						std::this_thread::sleep_for(std::chrono::nanoseconds(1));

						scope_profiler timer;

						for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
						{
							// If we measure with a cold CPU cache, we use a different context every time otherwise we use the first one
							DecompressionContextType* context = is_cold_cache_profiling ? contexts[clip_index] : contexts[0];

							context->seek(sample_time, sample_rounding_policy::none);

							switch (decompression_function)
							{
							case DecompressionFunction::DecompressPose:
								context->decompress_tracks(pose_writer);
								break;
							case DecompressionFunction::DecompressBone:
								for (uint32_t bone_index = 0; bone_index < num_tracks; ++bone_index)
									context->decompress_track(bone_index, pose_writer);
								break;
							}
						}

						timer.stop();

						const double elapsed_ms = timer.get_elapsed_milliseconds() / k_num_decompression_evaluations;

						if (are_any_enum_flags_set(logging, stat_logging::exhaustive_decompression))
							data_writer.push(elapsed_ms);

						clip_min_ms = rtm::scalar_min(clip_min_ms, elapsed_ms);
						clip_max_ms = rtm::scalar_max(clip_max_ms, elapsed_ms);
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

		inline void write_memcpy_performance_stats(iallocator& allocator, sjson::ObjectWriter& writer, CPUCacheFlusher* cache_flusher, rtm::qvvf* lossy_pose_transforms, uint32_t num_bones)
		{
			rtm::qvvf* memcpy_src_transforms = allocate_type_array<rtm::qvvf>(allocator, num_bones);

			double decompression_time_ms = 1000000.0;
			for (uint32_t pass_index = 0; pass_index < 3; ++pass_index)
			{
				if (cache_flusher != nullptr)
				{
					cache_flusher->begin_flushing();
					cache_flusher->flush_buffer(memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
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

					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
				}

				double execution_count;
				scope_profiler timer;
				if (cache_flusher != nullptr)
				{
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					execution_count = 1.0;
				}
				else
				{
					// Warm cache is too fast, execute multiple times and divide by the count
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					std::memcpy(lossy_pose_transforms, memcpy_src_transforms, sizeof(rtm::qvvf) * num_bones);
					execution_count = 10.0;
				}
				timer.stop();

				const double elapsed_ms = timer.get_elapsed_milliseconds() / execution_count;
				decompression_time_ms = rtm::scalar_min(decompression_time_ms, elapsed_ms);
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
		inline void write_decompression_performance_stats(iallocator& allocator, compressed_tracks* compressed_clips[k_num_decompression_evaluations], DecompressionContextType* contexts[k_num_decompression_evaluations], stat_logging logging, sjson::ObjectWriter& writer)
		{
			CPUCacheFlusher* cache_flusher = allocate_type<CPUCacheFlusher>(allocator);

			const uint32_t num_tracks = compressed_clips[0]->get_num_tracks();
			debug_track_writer pose_writer(allocator, track_type8::qvvf, num_tracks);

			const uint32_t num_bytes_per_bone = (4 + 3 + 3) * sizeof(float);	// Rotation, Translation, Scale
			writer["pose_size"] = num_tracks * num_bytes_per_bone;

			writer["decompression_time_per_sample"] = [&](sjson::ObjectWriter& per_sample_writer)
			{
				// Cold/Warm CPU cache, memcpy
				write_memcpy_performance_stats(allocator, per_sample_writer, cache_flusher, pose_writer.tracks_typed.qvvf, num_tracks);
				write_memcpy_performance_stats(allocator, per_sample_writer, nullptr, pose_writer.tracks_typed.qvvf, num_tracks);

				// Cold CPU cache, decompress_pose
				write_decompression_performance_stats(logging, per_sample_writer, "forward_pose_cold", PlaybackDirection::Forward, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "backward_pose_cold", PlaybackDirection::Backward, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "random_pose_cold", PlaybackDirection::Random, DecompressionFunction::DecompressPose, compressed_clips, contexts, cache_flusher, pose_writer);

				// Warm CPU cache, decompress_pose
				write_decompression_performance_stats(logging, per_sample_writer, "forward_pose_warm", PlaybackDirection::Forward, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "backward_pose_warm", PlaybackDirection::Backward, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "random_pose_warm", PlaybackDirection::Random, DecompressionFunction::DecompressPose, compressed_clips, contexts, nullptr, pose_writer);

				// Cold CPU cache, decompress_bone
				write_decompression_performance_stats(logging, per_sample_writer, "forward_bone_cold", PlaybackDirection::Forward, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "backward_bone_cold", PlaybackDirection::Backward, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "random_bone_cold", PlaybackDirection::Random, DecompressionFunction::DecompressBone, compressed_clips, contexts, cache_flusher, pose_writer);

				// Warm CPU cache, decompress_bone
				write_decompression_performance_stats(logging, per_sample_writer, "forward_bone_warm", PlaybackDirection::Forward, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "backward_bone_warm", PlaybackDirection::Backward, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, pose_writer);
				write_decompression_performance_stats(logging, per_sample_writer, "random_bone_warm", PlaybackDirection::Random, DecompressionFunction::DecompressBone, compressed_clips, contexts, nullptr, pose_writer);
			};

			deallocate_type(allocator, cache_flusher);
		}

		struct default_transform_decompression_settings_latest final : public default_transform_decompression_settings
		{
			static constexpr compressed_tracks_version16 version_supported() { return compressed_tracks_version16::latest; }
		};

		inline void write_decompression_performance_stats(iallocator& allocator, const compression_settings& settings, const compressed_tracks& compressed_clip, stat_logging logging, sjson::ObjectWriter& writer)
		{
			(void)settings;

			if (compressed_clip.get_algorithm_type() != algorithm_type8::uniformly_sampled)
				return;

#if defined(ACL_HAS_ASSERT_CHECKS)
			// If we can, we use a fast-path that simulates what a real game engine would use
			// by disabling the things they normally wouldn't care about like deprecated formats
			// and debugging features
			const bool use_uniform_fast_path = settings.rotation_format == rotation_format8::quatf_drop_w_variable
				&& settings.translation_format == vector_format8::vector3f_variable
				&& settings.scale_format == vector_format8::vector3f_variable;

			ACL_ASSERT(use_uniform_fast_path, "We do not support profiling the debug code path");
#endif

			compressed_tracks* compressed_clips[k_num_decompression_evaluations];
			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
			{
				void* clip = allocator.allocate(compressed_clip.get_size(), alignof(compressed_tracks));
				std::memcpy(clip, &compressed_clip, compressed_clip.get_size());
				compressed_clips[clip_index] = reinterpret_cast<compressed_tracks*>(clip);
			}

			decompression_context<default_transform_decompression_settings_latest>* contexts[k_num_decompression_evaluations];
			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
				contexts[clip_index] = make_decompression_context<default_transform_decompression_settings_latest>(allocator);

			write_decompression_performance_stats(allocator, compressed_clips, contexts, logging, writer);

			for (uint32_t pass_index = 0; pass_index < k_num_decompression_evaluations; ++pass_index)
				deallocate_type(allocator, contexts[pass_index]);

			for (uint32_t clip_index = 0; clip_index < k_num_decompression_evaluations; ++clip_index)
				allocator.deallocate(compressed_clips[clip_index], compressed_clip.get_size());
		}
	}
}

ACL_IMPL_FILE_PRAGMA_POP

#endif	// #if defined(SJSON_CPP_WRITER)
