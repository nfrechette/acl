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

#include <benchmark.h>

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

#ifdef _WIN32
// The below excludes some other unused services from the windows headers -- see windows.h for details.
#define NOGDICAPMASKS            // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES        // VK_*
#define NOWINMESSAGES            // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES                // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS            // SM_*
#define NOMENUS                    // MF_*
#define NOICONS                    // IDI_*
#define NOKEYSTATES                // MK_*
#define NOSYSCOMMANDS            // SC_*
#define NORASTEROPS                // Binary and Tertiary raster ops
#define NOSHOWWINDOW            // SW_*
#define OEMRESOURCE                // OEM Resource values
#define NOATOM                    // Atom Manager routines
#define NOCLIPBOARD                // Clipboard routines
#define NOCOLOR                    // Screen colors
#define NOCTLMGR                // Control and Dialog routines
#define NODRAWTEXT                // DrawText() and DT_*
#define NOGDI                    // All GDI #defines and routines
#define NOKERNEL                // All KERNEL #defines and routines
#define NOUSER                    // All USER #defines and routines
#define NONLS                    // All NLS #defines and routines
#define NOMB                    // MB_* and MessageBox()
#define NOMEMMGR                // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE                // typedef METAFILEPICT
#define NOMINMAX                // Macros min(a,b) and max(a,b)
#define NOMSG                    // typedef MSG and associated routines
#define NOOPENFILE                // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL                // SB_* and scrolling routines
#define NOSERVICE                // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND                    // Sound driver routines
#define NOTEXTMETRIC            // typedef TEXTMETRIC and associated routines
#define NOWH                    // SetWindowsHook and WH_*
#define NOWINOFFSETS            // GWL_*, GCL_*, associated routines
#define NOCOMM                    // COMM driver routines
#define NOKANJI                    // Kanji support stuff.
#define NOHELP                    // Help engine interface.
#define NOPROFILER                // Profiler interface.
#define NODEFERWINDOWPOS        // DeferWindowPos routines
#define NOMCX                    // Modem Configuration Extensions
#define NOCRYPT
#define NOTAPE
#define NOIMAGE
#define NOPROXYSTUB
#define NORPC

#include <windows.h>
#include <conio.h>

extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

static bool is_sjson_file(const char* filename)
{
	const size_t filename_len = std::strlen(filename);
	return filename_len >= 6 && strncmp(filename + filename_len - 6, ".sjson", 6) == 0;
}

static bool parse_options(int argc, char* argv[], const char*& out_metadata_filename)
{
	out_metadata_filename = nullptr;

	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char* argument = argv[arg_index];

		static constexpr const char* k_metadata_input_file_option = "-metadata=";
		size_t option_length = std::strlen(k_metadata_input_file_option);
		if (std::strncmp(argument, k_metadata_input_file_option, option_length) == 0)
		{
			out_metadata_filename = argument + option_length;
			if (!is_sjson_file(out_metadata_filename))
			{
				printf("Input file must be an SJSON file of the form: [*.sjson]\n");
				return false;
			}

			continue;
		}
	}

	return out_metadata_filename != nullptr;
}

static bool read_metadata_file(const char* metadata_filename, const char*& out_metadata_buffer, size_t& out_metadata_buffer_size)
{
	out_metadata_buffer = nullptr;
	out_metadata_buffer_size = 0;

	std::FILE* file = nullptr;

#ifdef _WIN32
	char path[64 * 1024] = { 0 };
	snprintf(path, acl::get_array_size(path), "\\\\?\\%s", metadata_filename);
	fopen_s(&file, path, "rb");
#else
	file = fopen(metadata_filename, "rb");
#endif

	if (file == nullptr)
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

	char* metadata_buffer = new char[file_size];
	const size_t result = fread(metadata_buffer, 1, file_size, file);
	fclose(file);

	if (result != file_size)
	{
		delete[] metadata_buffer;
		return false;
	}

	out_metadata_buffer = metadata_buffer;
	out_metadata_buffer_size = file_size;

	return true;
}

int main(int argc, char* argv[])
{
#if defined(_WIN32)
	// To improve the consistency of the performance results, pin our process to a specific processor core
	// Set the process affinity to physical core 6, on Ryzen 2950X it is the fastest core of the Die 1
	const DWORD_PTR physical_core_index = 5;
	const DWORD_PTR logical_core_index = physical_core_index * 2;
	SetProcessAffinityMask(GetCurrentProcess(), 1 << logical_core_index);
#endif

	const char* metadata_filename = nullptr;
	if (!parse_options(argc, argv, metadata_filename))
		return -1;

	const char* metadata_buffer = nullptr;
	size_t metadata_buffer_size = 0;
	if (!read_metadata_file(metadata_filename, metadata_buffer, metadata_buffer_size))
		return -2;

	std::string clip_dir;
	std::vector<std::string> clips;
	if (!parse_metadata(metadata_buffer, metadata_buffer_size, clip_dir, clips))
		return -3;

	delete[] metadata_buffer;
	metadata_buffer = nullptr;

	std::vector<acl::compressed_tracks*> compressed_clips;
	for (const std::string& clip : clips)
	{
		acl::compressed_tracks* raw_tracks = nullptr;
		if (!read_clip(clip_dir, clip, s_allocator, raw_tracks))
		{
			printf("Failed to read clip %s!\n", clip.c_str());
			continue;
		}

		prepare_clip(clip, *raw_tracks, compressed_clips);

		s_allocator.deallocate(raw_tracks, raw_tracks->get_size());
	}

	benchmark::Initialize(&argc, argv);

	// Run benchmarks
	benchmark::RunSpecifiedBenchmarks();

	// Clean up
	clear_benchmark_state();

	for (acl::compressed_tracks* compressed_tracks : compressed_clips)
		s_allocator.deallocate(compressed_tracks, compressed_tracks->get_size());

#ifdef _WIN32
	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}
#endif

	return 0;
}
