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

#include <acl_compressor.h>

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

static int parse_metadata(const char* buffer, size_t buffer_size, std::vector<std::string>& out_configs, std::vector<std::string>& out_clips)
{
	sjson::Parser parser(buffer, buffer_size);

	if (!parser.array_begins("configs"))
		return -100;

	while (!parser.try_array_ends())
	{
		sjson::StringView config_filename;
		if (parser.read(&config_filename, 1))
			out_configs.push_back(std::string(config_filename.c_str(), config_filename.size()));
	}

	if (!parser.array_begins("clips"))
		return -500;

	while (!parser.try_array_ends())
	{
		sjson::StringView clip_filename;
		if (parser.read(&clip_filename, 1))
			out_clips.push_back(std::string(clip_filename.c_str(), clip_filename.size()));
	}

	if (!parser.remainder_is_comments_and_whitespace())
		return -1000;

	return 0;
}

static int read_metadata(std::vector<std::string>& out_configs, std::vector<std::string>& out_clips)
{
	char metadata_path[1024];
	int result = get_bundle_resource_path("metadata.sjson", metadata_path, sizeof(metadata_path));
	if (result != 0)
		return result;

	std::ifstream t(metadata_path);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string str = buffer.str();

	result = parse_metadata(str.c_str(), str.size(), out_configs, out_clips);
	if (result != 0)
		printf("Failed to parse metadata\n");

	return result;
}

int main(int argc, char* argv[])
{
	std::vector<std::string> configs;
	std::vector<std::string> clips;
	int result = read_metadata(configs, clips);
	if (result != 0)
		return result;

	const int num_configs = (int)configs.size();
	const int num_clips = (int)clips.size();

	char argv_0_executable_name[64];
	char argv_1_regression_test[64];
	char argv_2_config_path[1024];
	char argv_3_clip_path[1024];
	char* compressor_argv[4] = { argv_0_executable_name, argv_1_regression_test, argv_2_config_path, argv_3_clip_path };
	snprintf(argv_0_executable_name, 64, "iOS Bundle");
	snprintf(argv_1_regression_test, 64, "-test");

	int num_failed_regression_tests = 0;

	for (int config_index = 0; config_index < num_configs; ++config_index)
	{
		const std::string& config_filename = configs[config_index];
		printf("Performing regression tests for configuration: %s (%d / %d)\n", config_filename.c_str(), config_index + 1, num_configs);

		char config_path[1024];
		if (get_bundle_resource_path(config_filename.c_str(), config_path, 1024) != 0)
			continue;

		snprintf(argv_2_config_path, 1024, "-config=%s", config_path);

		for (int clip_index = 0; clip_index < num_clips; ++clip_index)
		{
			const std::string& clip_filename = clips[clip_index];

			char clip_path[1024];
			if (get_bundle_resource_path(clip_filename.c_str(), clip_path, 1024) != 0)
				continue;

			snprintf(argv_3_clip_path, 1024, "-acl=%s", clip_path);

			result = main_impl(4, compressor_argv);
			if (result != 0)
			{
				num_failed_regression_tests++;
				printf("Failed regression test for clip: %s (%d / %d)\n", clip_filename.c_str(), clip_index + 1, num_clips);
			}
			else
				printf("Successful regression test for clip: %s (%d / %d)\n", clip_filename.c_str(), clip_index + 1, num_clips);
		}
	}

	if (num_failed_regression_tests != 0)
		printf("Number of regression test failures: %d\n", num_failed_regression_tests);

	printf("Done!\n");

	return num_failed_regression_tests;
}
