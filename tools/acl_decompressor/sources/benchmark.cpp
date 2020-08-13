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

// TODO: get an official CPU cache size
#if defined(RTM_SSE2_INTRINSICS)
	static constexpr uint32_t k_cache_size = 33 * 1024 * 1024;		// Assume 32 MB cache
	static constexpr uint32_t k_num_tlb_entries = 4000;
#elif defined(__ANDROID__)
	static constexpr uint32_t k_cache_size = 3 * 1024 * 1024;		// Pixel 3 has 2 MB cache
	static constexpr uint32_t k_num_tlb_entries = 100;
#else
	static constexpr uint32_t k_cache_size = 9 * 1024 * 1024;		// iPad Pro has 8 MB cache
	static constexpr uint32_t k_num_tlb_entries = 2000;
#endif

static constexpr uint32_t k_page_size = 4 * 1024;	// 4 KB is standard

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

struct benchmark_state
{
	acl::compressed_tracks* compressed_tracks = nullptr;	// Original clip

	acl::compressed_tracks** decompression_instances = nullptr;
	acl::decompression_context<acl::default_transform_decompression_settings>* decompression_contexts = nullptr;
	uint8_t* clip_mega_buffer = nullptr;

	uint32_t num_copies = 0;
	uint32_t pose_size = 0;
	uint32_t clip_buffer_size = 0;
};

acl::ansi_allocator s_allocator;
static benchmark_state s_benchmark_state;

void clear_benchmark_state()
{
	acl::deallocate_type_array(s_allocator, s_benchmark_state.decompression_contexts, s_benchmark_state.num_copies);
	acl::deallocate_type_array(s_allocator, s_benchmark_state.decompression_instances, s_benchmark_state.num_copies);
	acl::deallocate_type_array(s_allocator, s_benchmark_state.clip_mega_buffer, s_benchmark_state.num_copies * size_t(s_benchmark_state.clip_buffer_size));

	s_benchmark_state = benchmark_state();
}

static void setup_benchmark_state(acl::compressed_tracks& compressed_tracks)
{
	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	const uint32_t compressed_size = compressed_tracks.get_size();

	const uint32_t num_bytes_per_track = (4 + 3 + 3) * sizeof(float);	// Rotation, Translation, Scale
	const uint32_t pose_size = num_tracks * num_bytes_per_track;

	// We want to divide our CPU cache into the number of poses it can hold
	const uint32_t num_poses_in_cpu_cache = (k_cache_size + pose_size - 1) / pose_size;

	// We want our CPU cache to be cold when we decompress so we duplicate the clip and switch every iteration.
	// Each decompression call will interpolate 2 poses, assume we touch 'pose_size * 2' bytes and at least once memory page.
	const uint32_t num_copies = std::max<uint32_t>((num_poses_in_cpu_cache + 1) / 2, k_num_tlb_entries);

	// Align each buffer to a large multiple of our page size to avoid virtual memory translation noise.
	// When we decompress, our memory is sure to be cold and we want to make it reasonably cold.
	// This is why we use multiple clips to avoid the CPU cache being re-used.
	// However, that is not where it stops. When we hit a cache miss, we also need to translate the
	// virtual address used into a physical address. To do this, the CPU first checks if it already
	// translated an address in the same memory page. This is why we pad the buffer to make sure the
	// next clip in memory doesn't share a page. If we have a TLB miss, it triggers the hardware
	// page walk. This in turn uses the virtual address to lookup in a series of tables where the physical
	// memory lies. On x64, this series of tables is 4 (or 5) levels deep and they live in physical memory. Reading
	// them can in turn trigger cache misses and TLB misses. They can also be prefetched by the hardware
	// since their memory is just ordinary data that lives in the CPU cache and RAM like everything else.
	// To force as many cache misses there as possible and to minimize the impact of prefetching, we thus
	// align each buffer to spread them out in memory.
	const uint32_t clip_buffer_alignment = k_page_size * 32;

	// Make sure to pad our buffers with a page size to avoid TLB noise
	// Also make sure we can contain at least one full pose to handle memcpy decompression
	const uint32_t clip_buffer_size = acl::align_to(std::max<uint32_t>(compressed_size, pose_size), clip_buffer_alignment);

	printf("Pose size: %u, num copies: %u\n", pose_size, num_copies);

	acl::compressed_tracks** decompression_instances = acl::allocate_type_array<acl::compressed_tracks*>(s_allocator, num_copies);
	uint8_t* clip_mega_buffer = acl::allocate_type_array_aligned<uint8_t>(s_allocator, num_copies * size_t(clip_buffer_size), k_page_size);

	// Create our copies
	for (uint32_t copy_index = 0; copy_index < num_copies; ++copy_index)
	{
		uint8_t* buffer = clip_mega_buffer + copy_index * size_t(clip_buffer_size);
		std::memcpy(buffer, &compressed_tracks, compressed_size);

		decompression_instances[copy_index] = reinterpret_cast<acl::compressed_tracks*>(buffer);
	}

	acl::decompression_context<acl::default_transform_decompression_settings>* decompression_contexts = acl::allocate_type_array<acl::decompression_context<acl::default_transform_decompression_settings>>(s_allocator, num_copies);
	for (uint32_t instance_index = 0; instance_index < num_copies; ++instance_index)
		decompression_contexts[instance_index].initialize(*decompression_instances[instance_index]);

	s_benchmark_state.compressed_tracks = &compressed_tracks;
	s_benchmark_state.decompression_instances = decompression_instances;
	s_benchmark_state.decompression_contexts = decompression_contexts;
	s_benchmark_state.clip_mega_buffer = clip_mega_buffer;
	s_benchmark_state.num_copies = num_copies;
	s_benchmark_state.pose_size = pose_size;
	s_benchmark_state.clip_buffer_size = clip_buffer_size;
}

static void benchmark_decompression(benchmark::State& state)
{
	acl::compressed_tracks& compressed_tracks = *reinterpret_cast<acl::compressed_tracks*>(state.range(0));
	const PlaybackDirection playback_direction = static_cast<PlaybackDirection>(state.range(1));
	const DecompressionFunction decompression_function = static_cast<DecompressionFunction>(state.range(2));

	if (s_benchmark_state.compressed_tracks != &compressed_tracks)
	{
		// We have a new clip, clear out our old state and start over
		clear_benchmark_state();
		setup_benchmark_state(compressed_tracks);
	}

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
	acl::decompression_context<acl::default_transform_decompression_settings>* decompression_contexts = s_benchmark_state.decompression_contexts;
	const uint32_t num_copies = s_benchmark_state.num_copies;
	const uint32_t pose_size = s_benchmark_state.pose_size;

	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	acl::acl_impl::debug_track_writer pose_writer(s_allocator, acl::track_type8::qvvf, num_tracks);

	uint32_t current_context_index = 0;
	uint32_t current_sample_index = 0;
	for (auto _ : state)
	{
		const float sample_time = sample_times[current_sample_index];

		acl::decompression_context<acl::default_transform_decompression_settings>& context = decompression_contexts[current_context_index];
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

		// Move on to the next context and sample
		// We only move on to the next sample once every context has been touched
		++current_context_index;
		if (current_context_index >= num_copies)
		{
			current_context_index = 0;
			++current_sample_index;
			if (current_sample_index >= k_num_decompression_samples)
				current_sample_index = 0;
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
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::Memcpy });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::DecompressPose });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Forward, (int64_t)DecompressionFunction::DecompressBone });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Backward, (int64_t)DecompressionFunction::DecompressPose });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Backward, (int64_t)DecompressionFunction::DecompressBone });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Random, (int64_t)DecompressionFunction::DecompressPose });
	bench->Args({ reinterpret_cast<int64_t>(compressed_tracks), (int64_t)PlaybackDirection::Random, (int64_t)DecompressionFunction::DecompressBone });
	bench->ArgNames({ "", "Dir", "Func" });

	// Sometimes the numbers are slightly different from run to run, we'll run a few times
	bench->Repetitions(4);

	bench->ComputeStatistics("min", [](const std::vector<double>& v)
	{
		return *std::min_element(std::begin(v), std::end(v));
	});
	bench->ComputeStatistics("max", [](const std::vector<double>& v)
	{
		return *std::max_element(std::begin(v), std::end(v));
	});

	out_compressed_clips.push_back(compressed_tracks);
	return true;
}
