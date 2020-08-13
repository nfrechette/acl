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

#include <acl/core/ansi_allocator.h>
#include <acl/core/compressed_tracks.h>

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

extern acl::ansi_allocator s_allocator;

void clear_benchmark_state();

bool parse_metadata(const char* buffer, size_t buffer_size, std::string& out_clip_dir, std::vector<std::string>& out_clips);

bool read_clip(const std::string& clip_dir, const std::string& clip, acl::iallocator& allocator, acl::compressed_tracks*& out_compressed_tracks);

bool prepare_clip(const std::string& clip_name, const acl::compressed_tracks& raw_tracks, std::vector<acl::compressed_tracks*>& out_compressed_clips);
