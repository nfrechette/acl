////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2018 Nicholas Frechette & Animation Compression Library contributors
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

#include <CoreFoundation/CoreFoundation.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include <sjson/parser.h>

#include <benchmark/benchmark.h>
#include <sjson/parser.h>

#include <benchmark.h>

static int get_bundle_resource_path(const char* resource_filename, char* out_path, size_t path_max_size)
{
	CFStringRef resource_filename_str = CFStringCreateWithCString(nullptr, resource_filename, kCFStringEncodingUTF8);
	CFURLRef resource_url = CFBundleCopyResourceURL(CFBundleGetMainBundle(), resource_filename_str, nullptr, nullptr);
	CFRelease(resource_filename_str);
	if (!resource_url)
		return -1040;
	Boolean result = CFURLGetFileSystemRepresentation(resource_url, true, (unsigned char*)out_path, path_max_size);
	CFRelease(resource_url);
	if (!result)
		return -1050;
	return 0;
}

static int read_metadata(std::string& out_clip_dir, std::vector<std::string>& out_clips)
{
	char metadata_path[1024];
	int result = get_bundle_resource_path("metadata.sjson", metadata_path, sizeof(metadata_path));
	if (result != 0)
		return result;

	std::ifstream t(metadata_path);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	if (!parse_metadata(str.c_str(), str.size(), out_clip_dir, out_clips))
	{
		printf("Failed to parse metadata\n");
		return -1;
	}

	return 0;
}

static int read_clip(const std::string& clip_filename, std::vector<unsigned char>& out_buffer)
{
	char clip_path[1024];
	int result = get_bundle_resource_path(clip_filename.c_str(), clip_path, 1024);
	if (result != 0)
		return result;

	std::ifstream t(clip_path, std::ios::binary);
	out_buffer = std::vector<unsigned char>(std::istreambuf_iterator<char>(t), {});

	return out_buffer.empty() ? -1 : 0;
}

int main(int argc, char* argv[])
{
	std::string clip_dir;
	std::vector<std::string> clips;
	int result = read_metadata(clip_dir, clips);
	if (result != 0)
		return result;

	char output_directory[1024];
	std::strcpy(output_directory, getenv("HOME"));
	std::strcat(output_directory, "/Documents");

	const std::string output_filename = std::string(output_directory) + "/benchmark_results.json";

	std::vector<acl::compressed_tracks*> compressed_clips;
	for (const std::string& clip : clips)
	{
		std::vector<unsigned char> buffer;
		if (read_clip(clip, buffer) != 0)
		{
			printf("Failed to read clip %s!\n", clip.c_str());
			continue;
		}

		const acl::compressed_tracks* raw_tracks = reinterpret_cast<const acl::compressed_tracks*>(buffer.data());

		prepare_clip(clip, *raw_tracks, compressed_clips);
	}

	const int num_failed_decompression = int(clips.size() - compressed_clips.size());

	char argv_0_executable_name[64];
	snprintf(argv_0_executable_name, sizeof(argv_0_executable_name), "iOS Bundle");

	char argv_1_benchmark_out[2048];
	snprintf(argv_1_benchmark_out, sizeof(argv_1_benchmark_out), "--benchmark_out=%s", output_filename.c_str());

	char argv_2_benchmark_out_format[64];
	snprintf(argv_2_benchmark_out_format, sizeof(argv_2_benchmark_out_format), "--benchmark_out_format=json");

	int bench_argc = 3;
	char* bench_argv[3] = { argv_0_executable_name, argv_1_benchmark_out, argv_2_benchmark_out_format };

	benchmark::Initialize(&bench_argc, bench_argv);

	// Run benchmarks
	benchmark::RunSpecifiedBenchmarks();

	// Clean up
	clear_benchmark_state();

	for (acl::compressed_tracks* compressed_tracks : compressed_clips)
		s_allocator.deallocate(compressed_tracks, compressed_tracks->get_size());

	if (num_failed_decompression != 0)
		printf("Number of decompression failures: %d\n", num_failed_decompression);

	printf("Done!\n");

	return num_failed_decompression;
}
