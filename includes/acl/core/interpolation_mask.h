#pragma once

#include "acl/core/error_result.h"
#include "acl/core/iallocator.h"
#include "acl/core/interpolation_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	/////////////////////////////////////////////////////////////////////////////////
	// An interpolation_mask is a bit field with 2-bits per element, specifying a
	// SampleRoundingPolicy for each element
	class alignas(16) interpolation_mask final
	{
	public:
		////////////////////////////////////////////////////////////////////////////////
		// Make an interpolation mask for the given number of elements.
		static ErrorResult make_from_num_elements(IAllocator& allocator, uint32_t num_elements, interpolation_mask*& out_mask)
		{
			uint32_t buffer_size = 0;
			buffer_size += sizeof(interpolation_mask);
			ACL_ASSERT(is_aligned_to(buffer_size, alignof(interpolation_mask)), "Invalid alignment");
			buffer_size += sizeof(uint32_t) * ((uint32_t(num_elements) + 16 - 1) / 16); // 2 bits per element

			// Ensure we have sufficient padding for unaligned 16 byte loads
			buffer_size += 15;

			uint8_t* buffer = allocate_type_array_aligned<uint8_t>(allocator, buffer_size, 16);
			if (!buffer)
			{
				return ErrorResult("Failed to allocate interpolation_mask");
			}
			std::memset(buffer, 0, buffer_size);

			out_mask = new(buffer) interpolation_mask(buffer_size, num_elements);
			out_mask->m_size = buffer_size;
			out_mask->m_num_elements = num_elements;

			return ErrorResult();
		}

		////////////////////////////////////////////////////////////////////////////////
		// Returns the number of elements in the mask
		uint32_t get_num_elements() const noexcept { return m_num_elements; }

		////////////////////////////////////////////////////////////////////////////////
		// Returns the size in bytes of the interpolation mask.
		// Includes the 'interpolation_mask' instance size.
		uint32_t get_num_bytes() const noexcept { return m_size; }

		////////////////////////////////////////////////////////////////////////////////
		// Get the SampleRoundingPolicy for a particular element in the mask
		SampleRoundingPolicy get(uint32_t index) const
		{
			uint32_t * const bitfield = get_bitfield();
			ACL_ASSERT(index < m_num_elements, "Invalid bit index %u (num elements = %u)", index, m_num_elements);
			uint32_t const bits = bitfield[index / 16];
			uint32_t const shift = (30 - (index % 16) * 2);
			uint32_t const mask = 0x03 << shift;
			return static_cast<SampleRoundingPolicy>((bits & mask) >> shift);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Set the SampleRoundingPolicy for a particular element in the mask
		void set(uint32_t index, SampleRoundingPolicy value)
		{
			uint32_t * const bitfield = get_bitfield();
			ACL_ASSERT(index < m_num_elements, "Invalid bit index %u (num elements = %u)", index, m_num_elements);
			uint32_t const bits = bitfield[index / 16];
			uint32_t const shift = (30 - (index % 16) * 2);
			uint32_t const mask = 0x3 << shift;
			bitfield[index / 16] = (bits & ~mask) | (static_cast<uint32_t>(value) << shift);
		}

	private:
		
		////////////////////////////////////////////////////////////////////////////////
		// Hide everything
		interpolation_mask(uint32_t size_of_buffer, uint32_t num_elements) noexcept
			: m_size(size_of_buffer)
			, m_num_elements(num_elements)
		{
		}
		interpolation_mask(const interpolation_mask&) = delete;
		interpolation_mask(interpolation_mask&&) = delete;
		interpolation_mask* operator=(const interpolation_mask&) = delete;
		interpolation_mask* operator=(interpolation_mask&&) = delete;

		////////////////////////////////////////////////////////////////////////////////
		// Returns a pointer to the underlying bit-field data for this interpolation mask 
		uint32_t * get_bitfield() const { return add_offset_to_ptr<uint32_t>(this, sizeof(this)); }

		////////////////////////////////////////////////////////////////////////////////
		// Raw buffer details.
		////////////////////////////////////////////////////////////////////////////////

		// Total size in bytes of the interpolation mask, including sizeof(interpolation_mask)
		// and the underlying bit-field that immediately follows this struct
		uint32_t m_size;

		////////////////////////////////////////////////////////////////////////////////
		// Interpolation mask header.
		////////////////////////////////////////////////////////////////////////////////

		uint32_t m_num_elements;
		
		uint8_t padding[8];

		//////////////////////////////////////////////////////////////////////////
		// Interpolation mask bits follows here in memory.
		//////////////////////////////////////////////////////////////////////////
	};
}

ACL_IMPL_FILE_PRAGMA_POP
