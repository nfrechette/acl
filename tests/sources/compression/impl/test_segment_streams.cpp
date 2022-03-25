////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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

#include <catch2/catch.hpp>

#include <acl/compression/impl/segment_streams.h>
#include <acl/core/ansi_allocator.h>

TEST_CASE("Segment splitting", "[compression][impl]")
{
	acl::ansi_allocator allocator;

	acl::acl_impl::compression_segmenting_settings settings;
	settings.ideal_num_samples = 16;
	settings.max_num_samples = 31;

	// Test cases where no segmenting is performed
	uint32_t num_estimated_segments = 0;
	uint32_t num_segments = 0;
	uint32_t* num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 0, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 0);
	CHECK(num_segments == 0);
	CHECK(num_samples_per_segment == nullptr);

	num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 31, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 0);
	CHECK(num_segments == 0);
	CHECK(num_samples_per_segment == nullptr);

	// Max is exceeded, we have 2 segments
	num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 32, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 2);
	CHECK(num_segments == 2);
	REQUIRE(num_samples_per_segment != nullptr);
	CHECK(num_samples_per_segment[0] == 16);
	CHECK(num_samples_per_segment[1] == 16);
	acl::deallocate_type_array(allocator, num_samples_per_segment, num_estimated_segments);

	// 3 estimated, with balancing since last segment is too small
	num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 35, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 3);
	CHECK(num_segments == 2);
	REQUIRE(num_samples_per_segment != nullptr);
	CHECK(num_samples_per_segment[0] == 18);
	CHECK(num_samples_per_segment[1] == 17);
	CHECK(num_samples_per_segment[2] == 0);
	acl::deallocate_type_array(allocator, num_samples_per_segment, num_estimated_segments);

	// 3 estimated, with maximum balancing
	num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 39, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 3);
	CHECK(num_segments == 2);
	REQUIRE(num_samples_per_segment != nullptr);
	CHECK(num_samples_per_segment[0] == 20);
	CHECK(num_samples_per_segment[1] == 19);
	CHECK(num_samples_per_segment[2] == 0);
	acl::deallocate_type_array(allocator, num_samples_per_segment, num_estimated_segments);

	// 3 ideal, no need for balancing, but we currently do because of a bug, not optimal :(
	num_samples_per_segment = acl::acl_impl::split_samples_per_segment(allocator, 48, settings, num_estimated_segments, num_segments);

	CHECK(num_estimated_segments == 3);
	CHECK(num_segments == 2);
	REQUIRE(num_samples_per_segment != nullptr);
	CHECK(num_samples_per_segment[0] == 24);
	CHECK(num_samples_per_segment[1] == 24);
	CHECK(num_samples_per_segment[2] == 0);
	acl::deallocate_type_array(allocator, num_samples_per_segment, num_estimated_segments);
}
