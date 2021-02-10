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

#include <catch2/catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS
#define ACL_ALLOCATOR_TRACK_ALL_ALLOCATIONS

#include <acl/core/ansi_allocator.h>
#include <acl/core/memory_utils.h>

using namespace acl;

TEST_CASE("ANSI allocator", "[core][memory]")
{
	ansi_allocator allocator;
	CHECK(allocator.get_allocation_count() == 0);

	void* ptr0 = allocator.allocate(32);
	CHECK(allocator.get_allocation_count() == 1);

	void* ptr1 = allocator.allocate(48, 256);
	CHECK(allocator.get_allocation_count() == 2);
	CHECK(is_aligned_to(ptr1, 256));
	allocator.deallocate(ptr1, 48);
	CHECK(allocator.get_allocation_count() == 1);

	allocator.deallocate(ptr0, 32);
	CHECK(allocator.get_allocation_count() == 0);
}
