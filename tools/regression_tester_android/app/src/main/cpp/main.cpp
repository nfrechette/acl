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

#include <jni.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <sjson/parser.h>

#include <acl_compressor.h>

static int load_file(AAssetManager* asset_manager, const char* filename, void*& out_buffer, size_t& out_buffer_size)
{
	AAsset* asset = AAssetManager_open(asset_manager, filename, AASSET_MODE_UNKNOWN);
	if (asset == nullptr)
	{
		__android_log_print(ANDROID_LOG_ERROR, "acl", "%s not found", filename);
		return -1;
	}

	off_t size = AAsset_getLength(asset);
	out_buffer = (char*)malloc(sizeof(char) * size);
	out_buffer_size = sizeof(char) * size;
	AAsset_read(asset, out_buffer, size);

	AAsset_close(asset);

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

static int read_metadata(AAssetManager* asset_manager, std::vector<std::string>& out_configs, std::vector<std::string>& out_clips)
{
	void* buffer;
	size_t buffer_size;
	int result = load_file(asset_manager, "metadata.sjson", buffer, buffer_size);
	if (result != 0)
		return result;

	result = parse_metadata((const char*)buffer, buffer_size, out_configs, out_clips);
	if (result != 0)
		__android_log_print(ANDROID_LOG_ERROR, "acl", "Failed to parse metadata");

	free(buffer);
	return result;
}

extern "C" jint Java_com_acl_regression_1tests_MainActivity_nativeMain(JNIEnv* env, jobject caller, jobject java_asset_manager)
{
	AAssetManager* asset_manager = AAssetManager_fromJava(env, java_asset_manager);

	std::vector<std::string> configs;
	std::vector<std::string> clips;
	int result = read_metadata(asset_manager, configs, clips);
	if (result != 0)
		return result;

	const int num_configs = (int)configs.size();
	const int num_clips = (int)clips.size();

	char argv_0_executable_name[64];
	char argv_1_regression_test[64];
	char argv_2_config_buffer[64];
	char argv_3_clip_buffer[64];
	char* argv[4] = { argv_0_executable_name, argv_1_regression_test, argv_2_config_buffer, argv_3_clip_buffer };
	snprintf(argv_0_executable_name, 64, "Android APK");
	snprintf(argv_1_regression_test, 64, "-test");

	int num_failed_regression_tests = 0;

	for (int config_index = 0; config_index < num_configs; ++config_index)
	{
		const std::string& config_filename = configs[config_index];
		__android_log_print(ANDROID_LOG_INFO, "acl", "Performing regression tests for configuration: %s (%d / %d)", config_filename.c_str(), config_index + 1, num_configs);

		void* config_buffer;
		size_t config_buffer_size;
		if (load_file(asset_manager, config_filename.c_str(), config_buffer, config_buffer_size) != 0)
			continue;

		snprintf(argv_2_config_buffer, 64, "-config=@%u,%p", (uint32_t)config_buffer_size, config_buffer);

		for (int clip_index = 0; clip_index < num_clips; ++clip_index)
		{
			const std::string& clip_filename = clips[clip_index];

			const int is_input_acl_bin_file = is_acl_bin_file(clip_filename.c_str()) ? 1 : 0;

			void* clip_buffer;
			size_t clip_buffer_size;
			if (load_file(asset_manager, clip_filename.c_str(), clip_buffer, clip_buffer_size) != 0)
				continue;

			snprintf(argv_3_clip_buffer, 64, "-acl=@%u,%p,%d", (uint32_t)clip_buffer_size, clip_buffer, is_input_acl_bin_file);

			result = main_impl(4, argv);
			if (result != 0)
			{
				num_failed_regression_tests++;
				__android_log_print(ANDROID_LOG_ERROR, "acl", "Failed regression test for clip: %s (%d / %d)", clip_filename.c_str(), clip_index + 1, num_clips);
			}
			else
				__android_log_print(ANDROID_LOG_INFO, "acl", "Successful regression test for clip: %s (%d / %d)", clip_filename.c_str(), clip_index + 1, num_clips);

			free(clip_buffer);
		}

		free(config_buffer);
	}

	if (num_failed_regression_tests != 0)
		__android_log_print(ANDROID_LOG_ERROR, "acl", "Number of regression test failures: %d", num_failed_regression_tests);

	__android_log_print(ANDROID_LOG_INFO, "acl", "Done!");

	return num_failed_regression_tests;
}
