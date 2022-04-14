#pragma once

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

bool is_acl_sjson_file(const char* filename);
bool is_acl_bin_file(const char* filename);

int main_impl(int argc, char* argv[]);

#if defined(ACL_USE_SJSON)
#include "acl/core/compressed_database.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/iallocator.h"
#include "acl/compression/compression_settings.h"
#include "acl/compression/track_array.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/decompression/decompression_settings.h"

void validate_accuracy(acl::iallocator& allocator,
	const acl::track_array_qvvf& raw_tracks, const acl::track_array_qvvf& additive_base_tracks,
	const acl::itransform_error_metric& error_metric,
	const acl::compressed_tracks& compressed_tracks_,
	double regression_error_threshold);

void validate_accuracy(acl::iallocator& allocator, const acl::track_array& raw_tracks, const acl::compressed_tracks& tracks, double regression_error_threshold);

void validate_metadata(const acl::track_array& raw_tracks, const acl::compressed_tracks& tracks);
void validate_convert(acl::iallocator& allocator, const acl::track_array& raw_tracks);

void validate_db(acl::iallocator& allocator, const acl::track_array_qvvf& raw_tracks, const acl::track_array_qvvf& additive_base_tracks,
	const acl::compression_database_settings& settings, const acl::itransform_error_metric& error_metric,
	const acl::compressed_tracks& compressed_tracks0, const acl::compressed_tracks& compressed_tracks1);

struct debug_transform_decompression_settings_with_db final : public acl::debug_transform_decompression_settings
{
	using database_settings_type = acl::debug_database_settings;
};
#endif
