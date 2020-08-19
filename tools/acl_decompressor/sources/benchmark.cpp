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

// TODO: get these values from the CPU/OS
#if defined(RTM_SSE2_INTRINSICS)
	static constexpr uint32_t k_cache_size = 33 * 1024 * 1024;		// Assume 32 MB CPU cache
	static constexpr uint32_t k_num_tlb_entries = 4000;
	static constexpr uint32_t k_num_tlb_cache_entries = 2100;		// About 8.4 GB
#elif defined(__ANDROID__)
	static constexpr uint32_t k_cache_size = 3 * 1024 * 1024;		// Pixel 3 has 2 MB CPU cache
	static constexpr uint32_t k_num_tlb_entries = 100;
	static constexpr uint32_t k_num_tlb_cache_entries = 512;		// About 2 GB
#else
	static constexpr uint32_t k_cache_size = 9 * 1024 * 1024;		// iPad Pro has 8 MB CPU cache
	static constexpr uint32_t k_num_tlb_entries = 2000;
	static constexpr uint32_t k_num_tlb_cache_entries = 512;		// About 2 GB
#endif

static constexpr uint32_t k_page_size = 4 * 1024;					// 4 KB is standard, iOS uses 16 KB
static constexpr uint32_t k_min_clip_spacing = 17 * 1024 * 1024;	// Each L2 page table offset covers 16 MB with 4 KB pages
static constexpr uint32_t k_num_clips_to_allocate = 64;				// About 1.1 GB @ 17 MB/clip

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
	uint32_t pose_size = 0;
};

acl::ansi_allocator s_allocator;
static benchmark_state s_benchmark_state;
static uint32_t s_largest_clip_size = 0;
static uint8_t* s_memory_buffer = nullptr;
static uint8_t* s_tlb_flush_buffer = nullptr;
static acl::compressed_tracks* s_decompression_instances[k_num_clips_to_allocate];
static acl::decompression_context<acl::default_transform_decompression_settings> s_decompression_contexts[k_num_clips_to_allocate];

//////////////////////////////////////////////////////////////////////////
// With 64 bit pointers, the bottom 48 bits are used, the rest have the leading sign replicated.
// It is broken down like this from MSB to LSB for 4KB pages:
//    - Bits [48, 64) reserved
//    - Bits [39, 48) page map L4 offset
//    - Bits [30, 39) page map L3 offset (page directory pointer)
//    - Bits [21, 30) page map L2 offset (page directory offset)
//    - Bits [12, 21) page map L1 offset (page table offset)
//    - Bits [0, 12) offset within our 4KB page
//
// We have a 12 bit offset within our 4 KB page and 4x 9 bit offsets each within a lookup table.
// Lookups start at the top with the L4 offset. It is applied to the page map base pointer stored
// in a special CPU register. To lookup the physical page referenced, it goes like this:
// physical page number = base_pointer[L4_offset][L3_offset][L2_offset][L1_offset]
// Our final memory location is then: physical page number * 4 KB + page_offset
//
// A page offset references bytes in the [0, 4KB) range.
// Each L1 offset covers 4 KB (total 2 MB).
// Each L2 offset covers 2 MB (total 1 GB).
// Each L3 offset covers 1 GB (total 512 GB).
// Each L4 offset covers 512 GB (total 256 TB)
//
// Some CPUs have 5 levels to support more memory but we'll ignore those for now.
//
// Each offset is used to read a pointer to a table the next level offsets into. Since the page table
// is stored in memory, it is also cached in the L2/L3 CPU caches (and possibly the L1).
// Each table entry is 64 bits (8 bytes) and thus up to 8 entries fit inside
// a 64 bytes cache line. Reading neighboring cache lines from the page table might also trigger CPU
// hardware prefetching.
//
// We'll assume that the L4 and L3 offsets are always constant or in the CPU cache. We want to simulate a
// cache miss where both the L1 and L2 offsets also miss. This is reasonable since many games have less
// than 1 GB of animation state, including animations. We would expect 2-3 L3 offset misses at most regardless
// how many animations we decompress for a frame. This means that we want to align each clip to a 2 MB boundary.
// We also want each clip to be 16 MB apart to make sure the L2 offsets don't share a cache line. We also want
// to access the clips in a random ordering to ensure no prefetching can occur (todo: find an optimal ordering).
//
// Each clip being aligned on a 4 KB page boundary will end up aliasing within the CPU cache. A Ryzen 2950X CPU
// for example has an 8-way L1 cache, 8-way L2 cache, and 16-way L3 cache. This means up to 16 clips can fit in
// the CPU cache regardless of how much data we touch since they all alias and compete for cache line slots.
// For safety, 64 clips should be enough to guarantee clips are properly evicted before we loop.
//
// The CPU also has a TLB cache. A Ryzen 2950X has 1532 entries in the L2 for 4 KB pages. Zen2 CPUs have 2048 DTLB entries.
// This means we need to touch at least that many 4 KB pages before we can loop. Note that touching this many
// contiguous pages also means evicting a significant amount of CPU cache. Due to aliasing, it should evict all our old entries
// related to our page table as well.
//
// The simplest solution to ensure each decompression call triggers cache misses as well as L1 and L2 offset misses
// is thus to allocate 64 clips each spaced apart by at least 16 MB and accessed randomly. Once that is done, we simply
// need to touch at least 2048 memory pages. When we loop back to the first clip, the TLB will contain junk and the CPU caches
// will contain older clips that won't be touched again until they are evicted. This represents the least amount of overhead
// possible.
//////////////////////////////////////////////////////////////////////////

static void setup_initial_benchmark_state()
{
	const uint32_t clip_block_size = acl::align_to(s_largest_clip_size, k_min_clip_spacing);
	const size_t buffer_size = size_t(clip_block_size) * k_num_clips_to_allocate;
	s_memory_buffer = new uint8_t[buffer_size];

	const size_t tlb_buffer_size = size_t(k_page_size) * k_num_tlb_cache_entries;
	s_tlb_flush_buffer = new uint8_t[tlb_buffer_size];
}

static void setup_benchmark_state(acl::compressed_tracks& compressed_tracks)
{
	if (s_memory_buffer == nullptr)
		setup_initial_benchmark_state();

	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	const uint32_t compressed_size = compressed_tracks.get_size();

	const uint32_t num_bytes_per_track = (4 + 3 + 3) * sizeof(float);	// Rotation, Translation, Scale
	const uint32_t pose_size = num_tracks * num_bytes_per_track;
	const uint32_t clip_block_size = acl::align_to(compressed_size, k_min_clip_spacing);

	printf("Pose size: %u\n", pose_size);

	// Create our copies
	for (uint32_t copy_index = 0; copy_index < k_num_clips_to_allocate; ++copy_index)
	{
		uint8_t* buffer = s_memory_buffer + copy_index * size_t(clip_block_size);
		std::memcpy(buffer, &compressed_tracks, compressed_size);

		s_decompression_instances[copy_index] = reinterpret_cast<acl::compressed_tracks*>(buffer);
	}

	// Randomize our clips
	std::shuffle(&s_decompression_instances[0], &s_decompression_instances[k_num_clips_to_allocate], std::default_random_engine(0));

	// Initialize our context objects
	for (uint32_t instance_index = 0; instance_index < k_num_clips_to_allocate; ++instance_index)
		s_decompression_contexts[instance_index].initialize(*s_decompression_instances[instance_index]);

	s_benchmark_state.compressed_tracks = &compressed_tracks;
	s_benchmark_state.pose_size = pose_size;
}

static void benchmark_decompression(benchmark::State& state)
{
	acl::compressed_tracks& compressed_tracks = *reinterpret_cast<acl::compressed_tracks*>(state.range(0));
	const PlaybackDirection playback_direction = static_cast<PlaybackDirection>(state.range(1));
	const DecompressionFunction decompression_function = static_cast<DecompressionFunction>(state.range(2));

	if (s_benchmark_state.compressed_tracks != &compressed_tracks)
		setup_benchmark_state(compressed_tracks);	// We have a new clip, setup our state

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

	acl::compressed_tracks** decompression_instances = s_decompression_instances;
	acl::decompression_context<acl::default_transform_decompression_settings>* decompression_contexts = s_decompression_contexts;
	const uint32_t pose_size = s_benchmark_state.pose_size;

	const uint32_t num_tracks = compressed_tracks.get_num_tracks();
	acl::acl_impl::debug_track_writer pose_writer(s_allocator, acl::track_type8::qvvf, num_tracks);

	// TODO: Turn off cpu frequency
	// TODO: measure baseline before these changes
	// TODO: measure with these changes and tweak numbers a bit to validate

	uint32_t current_context_index = 0;
	uint32_t current_sample_index = 0;
	for (auto _ : state)
	{
		const auto start = std::chrono::high_resolution_clock::now();

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

		const auto end = std::chrono::high_resolution_clock::now();

		// Move on to the next context and sample
		// We only move on to the next sample once every context has been touched
		++current_context_index;
		if (current_context_index >= k_num_clips_to_allocate)
		{
			current_context_index = 0;
			++current_sample_index;
			if (current_sample_index >= k_num_decompression_samples)
				current_sample_index = 0;

			// Evict our TLB
			for (uint32_t page_index = 0; page_index < k_num_tlb_cache_entries; ++page_index)
				s_tlb_flush_buffer[page_index * size_t(k_page_size)]++;
		}

		const auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
		state.SetIterationTime(elapsed_seconds.count());
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

	bench->UseManualTime();

	bench->ComputeStatistics("min", [](const std::vector<double>& v)
	{
		return *std::min_element(std::begin(v), std::end(v));
	});
	bench->ComputeStatistics("max", [](const std::vector<double>& v)
	{
		return *std::max_element(std::begin(v), std::end(v));
	});

	s_largest_clip_size = std::max<uint32_t>(s_largest_clip_size, compressed_tracks->get_size());

	out_compressed_clips.push_back(compressed_tracks);
	return true;
}
