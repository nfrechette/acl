#include <catch.hpp>

// Enable allocation tracking
#define ACL_ALLOCATOR_TRACK_NUM_ALLOCATIONS

#include <acl/core/ansi_allocator.h>
#include <acl/core/memory_utils.h>

using namespace acl;

TEST_CASE("ANSI allocator", "[core][memory]")
{
	ANSIAllocator allocator;
	REQUIRE(allocator.get_allocation_count() == 0);

	void* ptr0 = allocator.allocate(32);
	REQUIRE(allocator.get_allocation_count() == 1);

	void* ptr1 = allocator.allocate(48, 256);
	REQUIRE(allocator.get_allocation_count() == 2);
	REQUIRE(is_aligned_to(ptr1, 256));
	allocator.deallocate(ptr1, 48);
	REQUIRE(allocator.get_allocation_count() == 1);

	allocator.deallocate(ptr0, 32);
	REQUIRE(allocator.get_allocation_count() == 0);
}
