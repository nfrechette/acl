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
#include <iostream>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <sjson/parser.h>

#include <benchmark.h>

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

static int read_metadata(AAssetManager* asset_manager, std::string& out_clip_dir, std::vector<std::string>& out_clips)
{
	void* buffer;
	size_t buffer_size;
	int result = load_file(asset_manager, "metadata.sjson", buffer, buffer_size);
	if (result != 0)
		return result;

	if (!parse_metadata((const char*)buffer, buffer_size, out_clip_dir, out_clips))
	{
		__android_log_print(ANDROID_LOG_ERROR, "acl", "Failed to parse metadata");
		result = -1;
	}

	free(buffer);
	return result;
}

// Inspired from https://stackoverflow.com/questions/8870174/is-stdcout-usable-in-android-ndk
class androidbuf final : public std::streambuf
{
public:
	enum { bufsize = 4096 };
	androidbuf()
	{
		this->setp(buffer, buffer + bufsize - 1);
	}

private:
	int overflow(int c)
	{
		if (c == traits_type::eof())
		{
			*this->pptr() = traits_type::to_char_type(c);
			this->sbumpc();
		}
		return this->sync() ? traits_type::eof() : traits_type::not_eof(c);
	}

	int sync()
	{
		int rc = 0;
		if (this->pbase() != this->pptr())
		{
			char writebuf[bufsize + 1];
			memcpy(writebuf, this->pbase(), this->pptr() - this->pbase());
			writebuf[this->pptr() - this->pbase()] = '\0';

			rc = __android_log_write(ANDROID_LOG_INFO, "acl", writebuf) > 0;
			this->setp(buffer, buffer + bufsize - 1);
		}
		return rc;
	}

	char buffer[bufsize];
};

extern "C" jint Java_com_acl_decompressor_MainActivity_nativeMain(JNIEnv* env, jobject caller, jobject java_asset_manager, jstring java_output_directory)
{
	std::cout.rdbuf(new androidbuf());

	AAssetManager* asset_manager = AAssetManager_fromJava(env, java_asset_manager);

	const char* output_directory = env->GetStringUTFChars(java_output_directory, nullptr);
	__android_log_print(ANDROID_LOG_INFO, "acl", "Benchmark results will be written to: %s", output_directory);

	const std::string output_filename = std::string(output_directory) + "/benchmark_results.json";

	std::string clip_dir;
	std::vector<std::string> clips;
	int result = read_metadata(asset_manager, clip_dir, clips);
	if (result != 0)
	{
		env->ReleaseStringUTFChars(java_output_directory, output_directory);
		return result;
	}

	std::vector<acl::compressed_tracks*> compressed_clips;
	for (const std::string& clip : clips)
	{
		void* clip_buffer = nullptr;
		size_t clip_buffer_size = 0;
		if (load_file(asset_manager, clip.c_str(), clip_buffer, clip_buffer_size) != 0)
		{
			printf("Failed to read clip %s!\n", clip.c_str());
			continue;
		}

		acl::compressed_tracks* raw_tracks = reinterpret_cast<acl::compressed_tracks*>(clip_buffer);

		prepare_clip(clip, *raw_tracks, compressed_clips);

		// We are done with this now
		free(raw_tracks);
	}

	const int num_failed_decompression = clips.size() - compressed_clips.size();

	char argv_0_executable_name[64];
	snprintf(argv_0_executable_name, sizeof(argv_0_executable_name), "Android APK");

	char argv_1_benchmark_out[2048];
	snprintf(argv_1_benchmark_out, sizeof(argv_1_benchmark_out), "--benchmark_out=%s", output_filename.c_str());

	char argv_2_benchmark_out_format[64];
	snprintf(argv_2_benchmark_out_format, sizeof(argv_2_benchmark_out_format), "--benchmark_out_format=json");

	int argc = 3;
	char* argv[3] = { argv_0_executable_name, argv_1_benchmark_out, argv_2_benchmark_out_format };

	benchmark::Initialize(&argc, argv);

	// Run benchmarks
	benchmark::RunSpecifiedBenchmarks();

	// Clean up
	clear_benchmark_state();

	for (acl::compressed_tracks* compressed_tracks : compressed_clips)
		s_allocator.deallocate(compressed_tracks, compressed_tracks->get_size());

	if (num_failed_decompression != 0)
		__android_log_print(ANDROID_LOG_ERROR, "acl", "Number of decompression failures: %d", num_failed_decompression);

	__android_log_print(ANDROID_LOG_INFO, "acl", "Done!");

	env->ReleaseStringUTFChars(java_output_directory, output_directory);

	return num_failed_decompression;
}
