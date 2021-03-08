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

#include "benchmark.h"

#include <acl/core/ansi_allocator.h>
#include <acl/core/compressed_tracks.h>
#include <acl/core/memory_utils.h>
#include <acl/compression/compress.h>
#include <acl/compression/convert.h>

#include <benchmark/benchmark.h>

#include <sjson/parser.h>

#include <algorithm>
#include <cstring>
#include <random>
#include <string>
#include <vector>

//////////////////////////////////////////////////////////////////////////
// Constants

static constexpr uint32_t k_cpu_cache_size = 8 * 1024 * 1024;	// Assume a 8 MB cache which is common for L3 modules (iPad, Zen2)

// In practice, CPUs do not always evict the least recently used cache line.
// To ensure every cache line is evicted, we allocate our buffer 4x larger than the CPU cache.
// We use a custom memset function to make sure that streaming writes aren't used which would
// bypass the CPU cache and not evict anything.
static constexpr uint32_t k_flush_buffer_size = k_cpu_cache_size * 4;

// The VMEM Level 1 translation has 512 entries each spanning 1 GB. We'll assume that in the real world
// there is a reasonable chance that memory touched will live within the same 1 GB region and thus be
// in some level of the CPU cache.

// The VMEM Level 2 translation has 512 entries each spanning 2 MB.
// This means the cache line we load to find a page offset contains a span of 16 MB within it (a cache
// line contains 8 entries).
// To ensure we don't touch cache lines that belong to our input buffer as we flush the CPU cache,
// we add sufficient padding at both ends of the flush buffer. Since we'll access it linearly,
// the hardware prefetcher might pull in cache lines ahead. We assume it won't pull more than 4 cache
// lines ahead. This means we need this much padding on each end: 4 * 16 MB = 64 MB
static constexpr uint32_t k_vmem_padding = 16 * 1024 * 1024;
static constexpr uint32_t k_padded_flush_buffer_size = k_vmem_padding + k_flush_buffer_size + k_vmem_padding;

// We allocate 100 copies of the compressed clip and align them to reduce the flush cost
// by flushing only when we loop around. We pad each copy to 16 MB to ensure no VMEM entry sharing in L2.
// A compressed clip that takes less than 160 MB would end up using 16 MB * 100 = 1.56 GB
static constexpr uint32_t k_num_copies = 220;

// Align our clip copy buffer to a 2 MB boundary to further reduce VMEM noise
static constexpr uint32_t k_clip_buffer_alignment = 2 * 1024 * 1024;

//////////////////////////////////////////////////////////////////////////

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
	Memcpy,
};

struct benchmark_transform_decompression_settings final : public acl::default_transform_decompression_settings
{
	// Only support our latest version
	static constexpr acl::compressed_tracks_version16 version_supported() { return acl::compressed_tracks_version16::latest; }

	// No need for safety checks
	static constexpr bool skip_initialize_safety_checks() { return true; }
};

struct benchmark_state
{
	acl::compressed_tracks* compressed_tracks = nullptr;	// Original clip

	acl::compressed_tracks** decompression_instances = nullptr;
	acl::decompression_context<benchmark_transform_decompression_settings>* decompression_contexts = nullptr;
	uint8_t* clip_copy_buffer = nullptr;
	uint8_t* flush_buffer = nullptr;

	uint32_t pose_size = 0;
	uint32_t clip_copy_buffer_size = 0;
};

acl::ansi_allocator s_allocator;
static benchmark_state s_benchmark_state;

void clear_benchmark_state()
{
	acl::deallocate_type_array(s_allocator, s_benchmark_state.decompression_contexts, k_num_copies);
	acl::deallocate_type_array(s_allocator, s_benchmark_state.decompression_instances, k_num_copies);
	acl::deallocate_type_array(s_allocator, s_benchmark_state.clip_copy_buffer, s_benchmark_state.clip_copy_buffer_size);
	acl::deallocate_type_array(s_allocator, s_benchmark_state.flush_buffer, k_padded_flush_buffer_size);

	s_benchmark_state = benchmark_state();
}

static void allocate_static_buffers()
{
	if (s_benchmark_state.flush_buffer != nullptr)
		return;	// Already allocated

	s_benchmark_state.decompression_instances = acl::allocate_type_array<acl::compressed_tracks*>(s_allocator, k_num_copies);
	s_benchmark_state.decompression_contexts = acl::allocate_type_array<acl::decompression_context<benchmark_transform_decompression_settings>>(s_allocator, k_num_copies);
	s_benchmark_state.flush_buffer = acl::allocate_type_array<uint8_t>(s_allocator, k_padded_flush_buffer_size);
}

static void setup_benchmark_state(acl::compressed_tracks& compressed_tracks)
{
	allocate_static_buffers();

	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	const uint32_t compressed_size = compressed_tracks.get_size();

	const uint32_t num_bytes_per_track = (4 + 3 + 3) * sizeof(float);	// Rotation, Translation, Scale
	const uint32_t pose_size = num_tracks * num_bytes_per_track;

	// Each clip is rounded up to a multiple of our VMEM padding
	const uint32_t padded_clip_size = acl::align_to(compressed_size, k_vmem_padding);
	const uint32_t clip_buffer_size = padded_clip_size * k_num_copies;

	acl::compressed_tracks** decompression_instances = s_benchmark_state.decompression_instances;
	acl::decompression_context<benchmark_transform_decompression_settings>* decompression_contexts = s_benchmark_state.decompression_contexts;
	uint8_t* clip_copy_buffer = s_benchmark_state.clip_copy_buffer;

	if (clip_buffer_size > s_benchmark_state.clip_copy_buffer_size)
	{
		// Allocate our new clip copy buffer
		clip_copy_buffer = acl::allocate_type_array_aligned<uint8_t>(s_allocator, clip_buffer_size, k_clip_buffer_alignment);

		s_benchmark_state.clip_copy_buffer = clip_copy_buffer;
		s_benchmark_state.clip_copy_buffer_size = clip_buffer_size;
	}

	printf("Pose size: %u bytes, clip size: %.2f MB\n", pose_size, double(compressed_size) / (1024.0 * 1024.0));

	// Create our copies
	for (uint32_t copy_index = 0; copy_index < k_num_copies; ++copy_index)
	{
		uint8_t* buffer = clip_copy_buffer + (copy_index * padded_clip_size);
		std::memcpy(buffer, &compressed_tracks, compressed_size);

		decompression_instances[copy_index] = reinterpret_cast<acl::compressed_tracks*>(buffer);
	}

	// Create our decompression contexts
	for (uint32_t instance_index = 0; instance_index < k_num_copies; ++instance_index)
		decompression_contexts[instance_index].initialize(*decompression_instances[instance_index]);

	s_benchmark_state.compressed_tracks = &compressed_tracks;
	s_benchmark_state.pose_size = pose_size;
}

static void memset_impl(uint8_t* buffer, size_t buffer_size, uint8_t value)
{
	for (uint8_t* ptr = buffer; ptr < buffer + buffer_size; ++ptr)
		*ptr = value;
}

static void benchmark_decompression(benchmark::State& state)
{
	acl::compressed_tracks& compressed_tracks = *reinterpret_cast<acl::compressed_tracks*>(state.range(0));
	const PlaybackDirection playback_direction = static_cast<PlaybackDirection>(state.range(1));
	const DecompressionFunction decompression_function = static_cast<DecompressionFunction>(state.range(2));

	if (s_benchmark_state.compressed_tracks != &compressed_tracks)
		setup_benchmark_state(compressed_tracks);	// We have a new clip, setup everything

	const float duration = compressed_tracks.get_duration();

	constexpr uint32_t k_num_decompression_samples = 100;
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

	acl::compressed_tracks** decompression_instances = s_benchmark_state.decompression_instances;
	acl::decompression_context<benchmark_transform_decompression_settings>* decompression_contexts = s_benchmark_state.decompression_contexts;
	uint8_t* flush_buffer = s_benchmark_state.flush_buffer;
	const uint32_t pose_size = s_benchmark_state.pose_size;

	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	acl::acl_impl::debug_track_writer pose_writer(s_allocator, acl::track_type8::qvvf, num_tracks);

	// Flush the CPU cache
	memset_impl(flush_buffer + k_vmem_padding, k_flush_buffer_size, 1);

	uint32_t current_context_index = 0;
	uint32_t current_sample_index = 0;
	uint8_t flush_value = 2;
	for (auto _ : state)
	{
		(void)_;

		const auto start = std::chrono::high_resolution_clock::now();

		const float sample_time = sample_times[current_sample_index];

		acl::decompression_context<benchmark_transform_decompression_settings>& context = decompression_contexts[current_context_index];
		context.seek(sample_time, acl::sample_rounding_policy::none);

		switch (decompression_function)
		{
		case DecompressionFunction::DecompressPose:
			context.decompress_tracks(pose_writer);
			break;
		case DecompressionFunction::DecompressBone:
			for (uint32_t bone_index = 0; bone_index < num_tracks; ++bone_index)
				context.decompress_track(bone_index, pose_writer);
			break;
		case DecompressionFunction::Memcpy:
			std::memcpy(pose_writer.tracks_typed.qvvf, decompression_instances[current_context_index], pose_size);
			break;
		}

		const auto end = std::chrono::high_resolution_clock::now();
		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());

		// Move on to the next context and sample
		// We only move on to the next sample once every context has been touched
		current_context_index++;
		if (current_context_index >= k_num_copies)
		{
			current_context_index = 0;
			current_sample_index++;

			if (current_sample_index >= k_num_decompression_samples)
				current_sample_index = 0;

			// Flush the CPU cache
			memset_impl(flush_buffer + k_vmem_padding, k_flush_buffer_size, flush_value++);
		}
	}

	state.counters["Speed"] = benchmark::Counter(s_benchmark_state.pose_size, benchmark::Counter::kIsIterationInvariantRate, benchmark::Counter::OneK::kIs1024);
}

bool parse_metadata(const char* buffer, size_t buffer_size, std::string& out_clip_dir, std::vector<std::string>& out_clips)
{
	sjson::Parser parser(buffer, buffer_size);

	sjson::StringView clip_dir;
	parser.try_read("clip_dir", clip_dir, "");
	out_clip_dir = std::string(clip_dir.c_str(), clip_dir.size());

	if (!parser.array_begins("clips"))
		return false;

	while (!parser.try_array_ends())
	{
		sjson::StringView clip_filename;
		if (parser.read(&clip_filename, 1))
			out_clips.push_back(std::string(clip_filename.c_str(), clip_filename.size()));
	}

	if (!parser.remainder_is_comments_and_whitespace())
		return false;

	return true;
}

bool read_clip(const std::string& clip_dir, const std::string& clip, acl::iallocator& allocator, acl::compressed_tracks*& out_compressed_tracks)
{
	out_compressed_tracks = nullptr;

	std::string clip_filename;
	clip_filename = clip_dir;

#ifdef _WIN32
	clip_filename += '\\';
#else
	clip_filename += '/';
#endif

	clip_filename += clip;

	std::FILE* file = nullptr;

#ifdef _WIN32
	char path[64 * 1024] = { 0 };
	snprintf(path, acl::get_array_size(path), "\\\\?\\%s", clip_filename.c_str());
	fopen_s(&file, path, "rb");
#else
	file = fopen(clip_filename.c_str(), "rb");
#endif

	if (file == nullptr)
		return false;

	// Make sure to enable buffering with a large buffer
	const int setvbuf_result = setvbuf(file, NULL, _IOFBF, 1 * 1024 * 1024);
	if (setvbuf_result != 0)
		return false;

	const int fseek_result = fseek(file, 0, SEEK_END);
	if (fseek_result != 0)
	{
		fclose(file);
		return false;
	}

#ifdef _WIN32
	const size_t file_size = static_cast<size_t>(_ftelli64(file));
#else
	const size_t file_size = static_cast<size_t>(ftello(file));
#endif

	if (file_size == static_cast<size_t>(-1L))
	{
		fclose(file);
		return false;
	}

	rewind(file);

	char* buffer = (char*)allocator.allocate(file_size);
	const size_t result = fread(buffer, 1, file_size, file);
	fclose(file);

	if (result != file_size)
	{
		delete[] buffer;
		return false;
	}

	out_compressed_tracks = reinterpret_cast<acl::compressed_tracks*>(buffer);
	return true;
}

bool prepare_clip(const std::string& clip_name, const acl::compressed_tracks& raw_tracks, std::vector<acl::compressed_tracks*>& out_compressed_clips)
{
	printf("Preparing clip %s ...\n", clip_name.c_str());

	acl::error_result result = raw_tracks.is_valid(false);
	if (result.any())
	{
		printf("    Failed to validate clip!\n");
		return false;
	}

	// Compress our clip
	acl::track_array track_list;
	result = acl::convert_track_list(s_allocator, raw_tracks, track_list);
	if (result.any())
	{
		printf("    Failed to convert clip!\n");
		return false;
	}

	if (track_list.get_track_type() != acl::track_type8::qvvf)
	{
		printf("    Invalid clip track type!\n");
		return false;
	}

	acl::compression_settings settings = acl::get_default_compression_settings();

	acl::qvvf_transform_error_metric error_metric;
	settings.error_metric = &error_metric;

	acl::output_stats stats;

	acl::compressed_tracks* compressed_tracks = nullptr;
	result = acl::compress_track_list(s_allocator, track_list, settings, compressed_tracks, stats);
	if (result.any())
	{
		printf("    Failed to compress clip!\n");
		return false;
	}

	if (compressed_tracks->is_valid(false).any())
	{
		printf("    Invalid compressed clip!\n");
		return false;
	}

	// Dynamically register our benchmark
	benchmark::internal::Benchmark* bench = benchmark::internal::RegisterBenchmarkInternal(new benchmark::internal::FunctionBenchmark(clip_name.c_str(), benchmark_decompression));

	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::DecompressPose });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::DecompressBone });

	// These are for debugging purposes and aren't measured as often
	// By design, ACL's performance should be consistent regardless of the playback direction
	//bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::Memcpy });
	//bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Backward, (int64_t)DecompressionFunction::DecompressPose });
	//bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Backward, (int64_t)DecompressionFunction::DecompressBone });
	//bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Random, (int64_t)DecompressionFunction::DecompressPose });
	//bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Random, (int64_t)DecompressionFunction::DecompressBone });

	// Name our arguments
	bench->ArgNames({ "", "Dir", "Func" });

	// Sometimes the numbers are slightly different from run to run, we'll run a few times
	bench->Repetitions(3);

	// Our benchmark has a very low standard deviation, there is no need to run 100k+ times
	bench->Iterations(10000);

	// Use manual timing since we clear the CPU cache explicitly
	bench->UseManualTime();

	// Add min/max tracking
	bench->ComputeStatistics("min", [](const std::vector<double>& v) { return *std::min_element(std::begin(v), std::end(v)); });
	bench->ComputeStatistics("max", [](const std::vector<double>& v) { return *std::max_element(std::begin(v), std::end(v)); });

	out_compressed_clips.push_back(compressed_tracks);
	return true;
}
