////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2025 Nicholas Frechette & Animation Compression Library contributors
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

#include "benchmark.h"

#if defined(ACL_IMPL_BENCHMARK_BIT_SET_ITERATION)

#include <acl/core/bit_manip_utils.h>

#include <rtm/quatf.h>
#include <rtm/qvvf.h>

// Interesting notes:
//
// The AppleClang compiler is clever with: packed_entry = ~packed_entry - 0x55555555
// It generates: packed_entry = -0x55555556 - packed_entry
// It yields a single sub instruction with a constant and the second operand comes from memory
// ~0xA56B12DE - 0x55555555 = 0x5A94ED21 - 0x55555555 = 0x053F97CC
// -0x55555556 - 0xA56B12DE = 0xAAAAAAAA - 0xA56B12DE = 0x053F97CC
// VS2022 does the same optimization
//
// The AppleClang compiler is clever with: packed_entry ^= packed_entry & -packed_entry;
// It generates: packed_entry &= packed_entry - 1
// It yields 2 instructions (SUB+ANDS) instead of 3
// 0xA56B12DE ^ (0xA56B12DE & -0xA56B12DE) = 0xA56B12DE ^ 0x00000002 = 0xA56B12DC
// 0xA56B12DE & (0xA56B12DE - 1) = 0xA56B12DE & 0xA56B12DD = 0xA56B12DC
// VS2022 does the same optimization
//
// BE CAREFUL WHEN PROFILING!
// If the bit sets used are too small, the CPU may be able to memorize the branching pattern
// quite well which can skew the results considerably.
//
// On Apple M1:
// Count-trailing-zeroes is fastest for low density (up to ~87.5%) and the ACL 2.1 reference
// is faster with high density (~87.5% and up).
// The 32 and 64-bit variants yield the same assembly, just with wider registers. The 64-bit
// variant is slightly faster overall.
// Light vs heavy work per bit has an impact on non-bit scanning versions because branch
// prediction plays a larger role there. Bit scanning method will have few branch miss-predictions
// because we have a single branch within the inner loop and we'll only miss when exiting the loop.
// The hybrid method that picks between the two is overall the best.
// At low density, the cost of popcount makes it slightly slower than pure CTZ but as density
// grows, we end up faster.
// Both reference implementations (indices vs pointers) perform about the same.
//
// On AMD Zen2:
// Count leading zeroes is faster than count trailing zeroes despite the dependency chain
// being longer but the CTZ 64-bit variant is faster. The reference implementation still
// wins around 87.5% density and above. The hybrid versions perform well and degrade gracefully
// as expected.
// The reference implementation using pointers outperforms indices by a good margin.
//
// Conclusion:
// Both variants combine well within the hybrid versions. When density is low, the bit scanning
// variant are faster because we have fewer branch miss-predictions and we have fewer branches
// taken (1 per set bit). However, when density increases, the number of branches taken approaches
// the count from the reference implementation. There, it gets a slight edge because with most
// bits sets, we can predict much better where to go and so the unrolled loop outperforms bit
// scanning. Combining both ensures that for each integer entry we pick the optimal strategy.
// The added overhead of popcount and the initial branch between both versions ends up yielding
// a net win regardless. It is worth noting that popcount is now required by Win11 and so is
// expected to be present on all modern x64 processors. ARM64 also has a reasonably efficient
// variant.
//

// How many bits to profile with
static constexpr uint32_t k_num_bits_in_bit_set = 6400 * 32;

static constexpr double k_bit_set_densities[] =
{
	// Common
	0.3,	// 0
	0.6,	// 1
	0.9,	// 2

	// Extra
	0.75,	// 3
	0.80,	// 4
	0.85,	// 5
};

// Direct memory access, minimal overhead
struct track_writer_light
{
	rtm::qvvf* output = nullptr;

	RTM_FORCE_INLINE bool skip_track(uint32_t track_index) const
	{
		(void)track_index;
		return false;
	}

	RTM_FORCE_INLINE void RTM_SIMD_CALL write_value(uint32_t track_index, rtm::quatf_arg0 rotation)
	{
		output[track_index].rotation = rotation;
	}
};

// Direct memory access, large overhead
struct track_writer_heavy
{
	rtm::qvvf* output = nullptr;

	RTM_FORCE_INLINE bool skip_track(uint32_t track_index) const
	{
		(void)track_index;
		return false;
	}

	RTM_FORCE_INLINE void RTM_SIMD_CALL write_value(uint32_t track_index, rtm::quatf_arg0 rotation)
	{
		output[track_index].rotation = rtm::quat_normalize(rotation);
	}
};

enum class writer_cost_t
{
	light,
	heavy,
};

template<typename word_type_t>
static void setup_bit_set(double bit_set_density, word_type_t* packed_entries, uint32_t num_packed_entries)
{
	std::srand(81440);

	// rand() only has so much precision, we process 16 entires at a time
	const uint32_t num_groups = (num_packed_entries + 15) / 16;
	constexpr uint32_t k_num_bits_per_word = sizeof(word_type_t) * 8;

	word_type_t* packed_entries_ptr = packed_entries;
	for (uint32_t group_index = 0; group_index < num_groups; ++group_index)
	{
		const uint32_t num_group_entries = (group_index + 1) != num_groups ? 16 : (num_packed_entries % 16);
		const uint32_t num_bits_to_set = uint32_t(double(k_num_bits_per_word) * num_group_entries * bit_set_density);

		uint32_t num_bits_set = 0;
		while (num_bits_set < num_bits_to_set)
		{
			const uint32_t bit_index = std::rand() % (k_num_bits_per_word * num_group_entries);
			const uint32_t word_index = bit_index / k_num_bits_per_word;
			const word_type_t word_bit_mask = word_type_t(1) << (bit_index % k_num_bits_per_word);

			if ((packed_entries_ptr[word_index] & word_bit_mask) != 0)
				continue;	// Already set, try again

			packed_entries_ptr[word_index] |= word_bit_mask;
			num_bits_set++;
		}

		packed_entries_ptr += 16;
	}

#if 0
	for (uint32_t i = 0; i < num_packed_entries; ++i)
		printf("%0u: 0x%X (%f)\n", i, packed_entries[i], double(acl::count_set_bits(packed_entries[i])) / 32.0);
#endif
}

// Reference implementation from ACL 2.1 with minor improvements
// Aims to maximize the number of independent instructions since each
// bit with a group of 4 are independently tested
// MSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_ref(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
	{
		uint32_t packed_entry = packed_entries[entry_index];

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = entry_index == last_entry_index ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		uint32_t curr_entry_track_index = track_index;

		// We might early out below, always skip 16 tracks
		track_index += 16;

		// Process 4 sub-tracks at a time
		while (packed_entry != 0)
		{
			// Requires that entries be packed LSB to MSB
			const uint32_t packed_group = packed_entry;
			const uint32_t curr_group_track_index = curr_entry_track_index;

			// Move to the next group
			packed_entry <<= 8;
			curr_entry_track_index += 4;

			if ((packed_group & 0xAA000000) == 0)
				continue;	// This group contains no default sub-tracks, skip it

			if ((packed_group & 0x80000000) != 0)
			{
				const uint32_t track_index0 = curr_group_track_index + 0;

				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);
			}

			if ((packed_group & 0x20000000) != 0)
			{
				const uint32_t track_index1 = curr_group_track_index + 1;

				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);
			}

			if ((packed_group & 0x08000000) != 0)
			{
				const uint32_t track_index2 = curr_group_track_index + 2;

				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
			}

			if ((packed_group & 0x02000000) != 0)
			{
				const uint32_t track_index3 = curr_group_track_index + 3;

				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
			}
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_ref(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_ref(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_ref(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_ref, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_ref, d90_heavy, 2, writer_cost_t::heavy);

// Reference implementation from ACL 2.1 with minor improvements to iterate using pointers
// MSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_ref_ptr(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		uint32_t curr_entry_track_index = track_index;

		// We might early out below, always skip 16 tracks
		track_index += 16;
		packed_entries_ptr++;

		// Process 4 sub-tracks at a time
		while (packed_entry != 0)
		{
			// Requires that entries be packed LSB to MSB
			const uint32_t packed_group = packed_entry;
			const uint32_t curr_group_track_index = curr_entry_track_index;

			// Move to the next group
			packed_entry <<= 8;
			curr_entry_track_index += 4;

			if ((packed_group & 0xAA000000) == 0)
				continue;	// This group contains no default sub-tracks, skip it

			if ((packed_group & 0x80000000) != 0)
			{
				const uint32_t track_index0 = curr_group_track_index + 0;

				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);
			}

			if ((packed_group & 0x20000000) != 0)
			{
				const uint32_t track_index1 = curr_group_track_index + 1;

				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);
			}

			if ((packed_group & 0x08000000) != 0)
			{
				const uint32_t track_index2 = curr_group_track_index + 2;

				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
			}

			if ((packed_group & 0x02000000) != 0)
			{
				const uint32_t track_index3 = curr_group_track_index + 3;

				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
			}
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_ref_ptr(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_ref_ptr(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_ref_ptr(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_ref_ptr, d90_heavy, 2, writer_cost_t::heavy);

// Iterates over every bit one by one naively
// MSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_naive(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		while (packed_entry != 0)
		{
			// Test MSB
			if ((packed_entry >> 31) != 0)
			{
				const uint32_t curr_track_index = curr_entry_track_index;

				if (!writer.skip_track(curr_track_index))
					writer.write_value(curr_track_index, default_rotation);
			}

			// Shift by two to MSB
			packed_entry <<= 2;
			curr_entry_track_index++;
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_naive(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_naive(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_naive(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_naive, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_naive, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_naive, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_naive, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_naive, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_naive, d90_heavy, 2, writer_cost_t::heavy);

// Uses count leading zeroes to bit scan each entry
// On ARM64, we have a CLZ instruction which is quite efficient
// On Zen2, we have a LZCNT instruction which is quite efficient (1 cycle)
// MSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_bit_scan_clz(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		const uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		while (packed_entry != 0)
		{
			// Requires that entries be packed MSB to LSB
			const uint32_t set_bit_index = acl::count_leading_zeros(packed_entry);
			const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

			// Mask out the bit we just consumed
			packed_entry ^= highest_set_bit;

			// We have 2 bits per sub-track
			const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

			if (!writer.skip_track(curr_track_index))
				writer.write_value(curr_track_index, default_rotation);
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_bit_scan_clz(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_clz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_clz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_clz, d90_heavy, 2, writer_cost_t::heavy);

// Uses count trailing zeroes to bit scan each entry
// On ARM64, we don't have a native CTZ instruction and so we end up with
// RBIT+CLZ which reverses the bits and counts leading zeroes
// On Zen2, we have a TZCNT instruction which is quite efficient (2 cycles)
// LSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_bit_scan_ctz_32(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		const uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		while (packed_entry != 0)
		{
			// Requires that entries be packed LSB to MSB
			const uint32_t lowest_set_bit = packed_entry & -packed_entry;
			const uint32_t set_bit_index = acl::count_trailing_zeros(packed_entry);

			// Mask out the bit we just consumed
			packed_entry ^= lowest_set_bit;

			// We have 2 bits per sub-track
			const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

			if (!writer.skip_track(curr_track_index))
				writer.write_value(curr_track_index, default_rotation);
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_bit_scan_ctz_32(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_ctz_32(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_ctz_32(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_32, d90_heavy, 2, writer_cost_t::heavy);

// Uses count trailing zeroes to bit scan each entry
// Same as above, 64-bit variant
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_bit_scan_ctz_64(
	const uint64_t* packed_entries, uint32_t last_entry_index,
	uint64_t padding_mask,
	track_writer_type& writer)
{
	const uint64_t* packed_entries_ptr = packed_entries;
	const uint64_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint64_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x5555555555555555ULL;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint64_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAAAAAAAAAAULL;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		const uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		while (packed_entry != 0)
		{
			// Requires that entries be packed LSB to MSB
			const uint64_t lowest_set_bit = packed_entry & -packed_entry;
			const uint32_t set_bit_index = uint32_t(acl::count_trailing_zeros(packed_entry));

			// Mask out the bit we just consumed
			packed_entry ^= lowest_set_bit;

			// We have 2 bits per sub-track
			const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

			if (!writer.skip_track(curr_track_index))
				writer.write_value(curr_track_index, default_rotation);
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_bit_scan_ctz_64(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 64;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint64_t* packed_entries = new uint64_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_ctz_64(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFFFFFFFFFULL, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_bit_scan_ctz_64(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFFFFFFFFFULL, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_bit_scan_ctz_64, d90_heavy, 2, writer_cost_t::heavy);

// Uses count leading zeroes to bit scan each entry for low density and reference method
// for high density (82.5% and up) using popcount to determine density
// On ARM64, we have a native CNT instruction which requires 2x parallel ADD instructions
// to implement popcount. Total 5 instructions: MOV+CNT+PADD+PADD+MOV
// On Zen2, we have a POPCNT instruction which is quite efficient (1 cycle)
// MSB first
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_hybrid_clz(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		const uint32_t num_set_bits = acl::count_set_bits(packed_entry);
		// 26 / 32 = 81.25%, we use half of that since we have 2 bits per sub-track
		if (num_set_bits >= 13)
		{
			// High density, use reference impl
			// Process 4 sub-tracks at a time
			while (packed_entry != 0)
			{
				// Requires that entries be packed LSB to MSB
				const uint32_t packed_group = packed_entry;
				const uint32_t curr_group_track_index = curr_entry_track_index;

				// Move to the next group
				packed_entry <<= 8;
				curr_entry_track_index += 4;

				if ((packed_group & 0x80000000) != 0)
				{
					const uint32_t track_index0 = curr_group_track_index + 0;

					if (!writer.skip_track(track_index0))
						writer.write_value(track_index0, default_rotation);
				}

				if ((packed_group & 0x20000000) != 0)
				{
					const uint32_t track_index1 = curr_group_track_index + 1;

					if (!writer.skip_track(track_index1))
						writer.write_value(track_index1, default_rotation);
				}

				if ((packed_group & 0x08000000) != 0)
				{
					const uint32_t track_index2 = curr_group_track_index + 2;

					if (!writer.skip_track(track_index2))
						writer.write_value(track_index2, default_rotation);
				}

				if ((packed_group & 0x02000000) != 0)
				{
					const uint32_t track_index3 = curr_group_track_index + 3;

					if (!writer.skip_track(track_index3))
						writer.write_value(track_index3, default_rotation);
				}
			}
		}
		else
		{
			// Low density, use ctz impl
			while (packed_entry != 0)
			{
				// Requires that entries be packed MSB to LSB
				const uint32_t set_bit_index = acl::count_leading_zeros(packed_entry);
				const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

				// Mask out the bit we just consumed
				packed_entry ^= highest_set_bit;

				// We have 2 bits per sub-track
				const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

				if (!writer.skip_track(curr_track_index))
					writer.write_value(curr_track_index, default_rotation);
			}
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_hybrid_clz(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_hybrid_clz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_hybrid_clz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_clz, d90_heavy, 2, writer_cost_t::heavy);

// Uses count trailing zeroes to bit scan each entry for low density and reference method
// for high density (82.5% and up) using popcount to determine density
// On ARM64, we have a native CNT instruction which requires 2x parallel ADD instructions
// to implement popcount. Total 5 instructions: MOV+CNT+PADD+PADD+MOV
// On Zen2, we have a POPCNT instruction which is quite efficient (1 cycle)
// MSB/LSB first (mixed atm, not correct)
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_hybrid_ctz(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		const uint32_t num_set_bits = acl::count_set_bits(packed_entry);
		// 26 / 32 = 81.25%, we use half of that since we have 2 bits per sub-track
		if (num_set_bits >= 13)
		{
			// High density, use reference impl
			// Process 4 sub-tracks at a time
			while (packed_entry != 0)
			{
				// Requires that entries be packed LSB to MSB
				const uint32_t packed_group = packed_entry;
				const uint32_t curr_group_track_index = curr_entry_track_index;

				// Move to the next group
				packed_entry <<= 8;
				curr_entry_track_index += 4;

				if ((packed_group & 0x80000000) != 0)
				{
					const uint32_t track_index0 = curr_group_track_index + 0;

					if (!writer.skip_track(track_index0))
						writer.write_value(track_index0, default_rotation);
				}

				if ((packed_group & 0x20000000) != 0)
				{
					const uint32_t track_index1 = curr_group_track_index + 1;

					if (!writer.skip_track(track_index1))
						writer.write_value(track_index1, default_rotation);
				}

				if ((packed_group & 0x08000000) != 0)
				{
					const uint32_t track_index2 = curr_group_track_index + 2;

					if (!writer.skip_track(track_index2))
						writer.write_value(track_index2, default_rotation);
				}

				if ((packed_group & 0x02000000) != 0)
				{
					const uint32_t track_index3 = curr_group_track_index + 3;

					if (!writer.skip_track(track_index3))
						writer.write_value(track_index3, default_rotation);
				}
			}
		}
		else
		{
			// Low density, use ctz impl
			while (packed_entry != 0)
			{
				// Requires that entries be packed LSB to MSB
				const uint32_t lowest_set_bit = packed_entry & -packed_entry;
				const uint32_t set_bit_index = acl::count_trailing_zeros(packed_entry);

				// Mask out the bit we just consumed
				packed_entry ^= lowest_set_bit;

				// We have 2 bits per sub-track
				const uint32_t curr_track_index = curr_entry_track_index + (set_bit_index / 2);

				if (!writer.skip_track(curr_track_index))
					writer.write_value(curr_track_index, default_rotation);
			}
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_hybrid_ctz(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_hybrid_ctz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_hybrid_ctz(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_hybrid_ctz, d90_heavy, 2, writer_cost_t::heavy);

// Uses a switch statement to unpack up to 4 bits at a time
// Apple Clang ends up optimizing this as an indirect branch using a lookup table
// that targets the asm within the function. Each case then ends up with an unconditional
// jump to return to the loop start.
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_batched_switch(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	uint32_t track_index = 0;

	while (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// Interleave the sub-tracks in upper 16-bits
		// b10101010,10101010,10101010,10101010 [0, _, 1, _, 2, _, 3, _] [4, _, 5, _, 6, _, 7, _] [8, _, 9, _, 10, _, 11, _] [12, _, 13, _, 14, _, 15, _]
		// or
		// b01010101,01010101,01010101,01010100 [_, 2, _, 3, _, 4, _, 5] [_, 6, _, 7, _, 8, _, 9] [_, 10, _, 11, _, 12, _, 13] [_, 14, _, 15, _, _, _, _]
		// =
		// [0, 2, 1, 3, 2, 4, 3, 5] [4, 6, 5, 7, 6, 8, 7, 9] [8, 10, 9, 11, 10, 12, 11, 13] [12, 14, 13, 15, 14, _, 15, _]
		// We'll read the first nibble of each byte:
		// [0, 2, 1, 3] [4, 6, 5, 7] [8, 10, 9, 11] [12, 14, 13, 15]
		packed_entry |= packed_entry << 3;

		// We have 2 bits per sub-track
		uint32_t curr_entry_track_index = track_index;
		track_index += 16;
		packed_entries_ptr++;

		while (packed_entry != 0)
		{
			// First 4 bits (2 sub-tracks): b1010
			const uint32_t packed_nibble = packed_entry >> 28;

			// Move to the next group
			packed_entry <<= 8;

			const uint32_t curr_group_track_index = curr_entry_track_index;
			curr_entry_track_index += 4;

			switch (packed_nibble)
			{
			default:
			case 0:
				// Nothing to unpack
				break;
			case 1:
			{
				// Unpack: 3
				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 2:
			{
				// Unpack: 1
				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);
				break;
			}
			case 3:
			{
				// Unpack: 1, 3
				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 4:
			{
				// Unpack: 2
				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
				break;
			}
			case 5:
			{
				// Unpack: 2, 3
				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 6:
			{
				// Unpack: 1, 2
				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
				break;
			}
			case 7:
			{
				// Unpack: 1, 2, 3
				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 8:
			{
				// Unpack: 0
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);
				break;
			}
			case 9:
			{
				// Unpack: 0, 3
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 10:
			{
				// Unpack: 0, 1
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);
				break;
			}
			case 11:
			{
				// Unpack: 0, 1, 3
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 12:
			{
				// Unpack: 0, 2
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
				break;
			}
			case 13:
			{
				// Unpack: 0, 2, 3
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			case 14:
			{
				// Unpack: 0, 1, 2
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);
				break;
			}
			case 15:
			{
				// Unpack: 0, 1, 2, 3
				const uint32_t track_index0 = curr_group_track_index + 0;
				if (!writer.skip_track(track_index0))
					writer.write_value(track_index0, default_rotation);

				const uint32_t track_index1 = curr_group_track_index + 1;
				if (!writer.skip_track(track_index1))
					writer.write_value(track_index1, default_rotation);

				const uint32_t track_index2 = curr_group_track_index + 2;
				if (!writer.skip_track(track_index2))
					writer.write_value(track_index2, default_rotation);

				const uint32_t track_index3 = curr_group_track_index + 3;
				if (!writer.skip_track(track_index3))
					writer.write_value(track_index3, default_rotation);
				break;
			}
			}
		}
	}
}

template<class ...args_>
static void bm_bitset_iter_batched_switch(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_batched_switch(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_batched_switch(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_batched_switch, d90_heavy, 2, writer_cost_t::heavy);

#if defined(RTM_COMPILER_CLANG)
	// As of Clang 13, TODO ADD VERSION CHECK
	#define ACL_IMPL_MUST_TAIL [[clang::musttail]]
#elif defined(RTM_COMPILER_GCC) && 0
	// As of GCC 15, TODO ADD VERSION CHECK
	#define ACL_IMPL_MUST_TAIL [[gnu::musttail]]
#else
	// Not supported by MSVC and others
	#define ACL_IMPL_MUST_TAIL
#endif

// Controls which variant is used
// 0: Passes everything except dispatch table ptr by argument
// 1: Passes everything by argument
// 2: Passes a context object and minimizes the number of arguments (for x64)
#define ACL_IMPL_TABLE_DISPATCH_VARIANT 2

#if ACL_IMPL_TABLE_DISPATCH_VARIANT == 1
struct dispatch_table_helper_t;

typedef void (RTM_SIMD_CALL *dispatch_fun_t)(
	dispatch_table_helper_t dispatch_table_,
	const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
	uint32_t packed_entry, uint32_t curr_entry_track_index,
	rtm::quatf_arg0 default_rotation, void* writer);

struct dispatch_table_helper_t
{
	const dispatch_fun_t* dispatch_table;
};
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 2
struct dispatch_helper_t;

typedef void (RTM_SIMD_CALL *dispatch_fun_t)(
	dispatch_helper_t& dispatch_helper,
	uint32_t packed_entry, uint32_t curr_entry_track_index,
	void* writer,
	rtm::quatf_arg4 default_rotation);

struct dispatch_helper_t
{
	rtm::quatf default_rotation;

	const uint32_t* packed_entries_ptr;
	const uint32_t* packed_entries_last_ptr;
	uint32_t padding_mask;

	dispatch_fun_t dispatch_table[16];
};
#endif

template<class track_writer_type>
struct bitset_iter_loopup_helper
{
#if ACL_IMPL_TABLE_DISPATCH_VARIANT == 0
	typedef void (RTM_SIMD_CALL *dispatch_fun_t)(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer);

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_0(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		if (packed_entry == 0)
		{
			// Done with this entry
			if (packed_entries_ptr > packed_entries_last_ptr)
				return;	// Done iterating

			packed_entry = *packed_entries_ptr;

			// Mask out everything but default sub-tracks, this way we can early out when we iterate
			// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
			// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
			// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
			// Finally, we mask out everything but the second bit for each sub-track
			// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
			// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
			packed_entry = ~packed_entry - 0x55555555;

			// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
			const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
			packed_entry &= entry_padding_mask;

			packed_entries_ptr++;
		}

		// Nothing to unpack, move to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_1(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 3
		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_2(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 1
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_3(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 1, 3
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_4(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 2
		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_5(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 2, 3
		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_6(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 1, 2
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_7(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 1, 2, 3
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_8(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_9(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 3
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_10(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 1
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_11(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 1, 3
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_12(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 2
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_13(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 2, 3
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_14(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 1, 2
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_15(
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, track_writer_type& writer)
	{
		// Unpack: 0, 1, 2, 3
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
	}

	static const dispatch_fun_t dispatch_table[16];
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 1
	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_0(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		if (packed_entry == 0)
		{
			// Done with this entry
			if (packed_entries_ptr > packed_entries_last_ptr)
				return;	// Done iterating

			packed_entry = *packed_entries_ptr;

			// Mask out everything but default sub-tracks, this way we can early out when we iterate
			// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
			// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
			// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
			// Finally, we mask out everything but the second bit for each sub-track
			// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
			// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
			packed_entry = ~packed_entry - 0x55555555;

			// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
			const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
			packed_entry &= entry_padding_mask;

			packed_entries_ptr++;
		}

		// Nothing to unpack, move to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_1(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_2(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 1
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_3(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 1, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_4(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 2
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_5(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 2, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_6(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 1, 2
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_7(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 1, 2, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_8(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_9(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_10(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 1
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_11(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 1, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_12(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 2
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_13(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 2, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_14(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 1, 2
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_15(
		dispatch_table_helper_t dispatch_table_,
		const uint32_t* packed_entries_ptr, const uint32_t* packed_entries_last_ptr, uint32_t padding_mask,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		rtm::quatf_arg0 default_rotation, void* writer_)
	{
		// Unpack: 0, 1, 2, 3
		track_writer_type& writer = *(track_writer_type*)writer_;
		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move on to next nibble
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		//(*dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);

		//const dispatch_fun_t2* dispatch_table__ = (const dispatch_fun_t2*)dispatch_table_;
		//(*dispatch_table__[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
		ACL_IMPL_MUST_TAIL return (*dispatch_table_.dispatch_table[packed_nibble])(dispatch_table_, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer_);
	}

	static const dispatch_fun_t dispatch_table[16];
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 2
	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_0(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		if (packed_entry == 0)
		{
			// Done with this entry
			const uint32_t* packed_entries_ptr = dispatch_helper.packed_entries_ptr;
			const uint32_t* packed_entries_last_ptr = dispatch_helper.packed_entries_last_ptr;

			if (packed_entries_ptr > packed_entries_last_ptr)
				return;	// Done iterating

			packed_entry = *packed_entries_ptr;

			// Mask out everything but default sub-tracks, this way we can early out when we iterate
			// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
			// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
			// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
			// Finally, we mask out everything but the second bit for each sub-track
			// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
			// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
			packed_entry = ~packed_entry - 0x55555555;

			// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
			const uint32_t padding_mask = dispatch_helper.padding_mask;
			const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
			packed_entry &= entry_padding_mask;

			// Interleave the sub-tracks in upper 16-bits
			// b10101010,10101010,10101010,10101010 [0, _, 1, _, 2, _, 3, _] [4, _, 5, _, 6, _, 7, _] [8, _, 9, _, 10, _, 11, _] [12, _, 13, _, 14, _, 15, _]
			// or
			// b01010101,01010101,01010101,01010100 [_, 2, _, 3, _, 4, _, 5] [_, 6, _, 7, _, 8, _, 9] [_, 10, _, 11, _, 12, _, 13] [_, 14, _, 15, _, _, _, _]
			// =
			// [0, 2, 1, 3, 2, 4, 3, 5] [4, 6, 5, 7, 6, 8, 7, 9] [8, 10, 9, 11, 10, 12, 11, 13] [12, 14, 13, 15, 14, _, 15, _]
			// We'll read the first nibble of each byte:
			// [0, 2, 1, 3] [4, 6, 5, 7] [8, 10, 9, 11] [12, 14, 13, 15]
			packed_entry |= packed_entry << 3;

			packed_entries_ptr++;
			dispatch_helper.packed_entries_ptr = packed_entries_ptr;
		}

		// Nothing to unpack, move to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_1(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_2(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 1
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_3(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 1, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_4(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 2
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_5(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 2, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_6(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 1, 2
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_7(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 1, 2, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_8(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_9(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_10(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 1
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_11(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 1, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_12(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 2
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_13(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 2, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_14(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 1, 2
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static void RTM_SIMD_CALL bitset_iter_loopup_table_entry_15(
		dispatch_helper_t& dispatch_helper,
		uint32_t packed_entry, uint32_t curr_entry_track_index,
		void* writer_,
		rtm::quatf_arg4 default_rotation)
	{
		// Unpack: 0, 1, 2, 3
		//const rtm::quatf default_rotation = dispatch_helper.default_rotation;
		track_writer_type& writer = *(track_writer_type*)writer_;

		// Move on to next nibble
		const uint32_t packed_nibble = packed_entry >> 28;
		const dispatch_fun_t next_fun = dispatch_helper.dispatch_table[packed_nibble];

		const uint32_t track_index0 = curr_entry_track_index + 0;
		if (!writer.skip_track(track_index0))
			writer.write_value(track_index0, default_rotation);

		const uint32_t track_index1 = curr_entry_track_index + 1;
		if (!writer.skip_track(track_index1))
			writer.write_value(track_index1, default_rotation);

		const uint32_t track_index2 = curr_entry_track_index + 2;
		if (!writer.skip_track(track_index2))
			writer.write_value(track_index2, default_rotation);

		const uint32_t track_index3 = curr_entry_track_index + 3;
		if (!writer.skip_track(track_index3))
			writer.write_value(track_index3, default_rotation);

		// Move to the next group
		packed_entry <<= 8;
		curr_entry_track_index += 4;

		ACL_IMPL_MUST_TAIL return (*next_fun)(dispatch_helper, packed_entry, curr_entry_track_index, writer_, default_rotation);
	}

	static const dispatch_fun_t dispatch_table[16];
#endif
};

#if ACL_IMPL_TABLE_DISPATCH_VARIANT == 0
template<class track_writer_type>
const typename bitset_iter_loopup_helper<track_writer_type>::dispatch_fun_t bitset_iter_loopup_helper<track_writer_type>::dispatch_table[16] =
{
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_0,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_1,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_2,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_3,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_4,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_5,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_6,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_7,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_8,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_9,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_10,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_11,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_12,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_13,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_14,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_15,
};
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 1 || ACL_IMPL_TABLE_DISPATCH_VARIANT == 2
template<class track_writer_type>
const dispatch_fun_t bitset_iter_loopup_helper<track_writer_type>::dispatch_table[16] =
{
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_0,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_1,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_2,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_3,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_4,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_5,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_6,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_7,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_8,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_9,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_10,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_11,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_12,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_13,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_14,
	&bitset_iter_loopup_helper<track_writer_type>::bitset_iter_loopup_table_entry_15,
};
#endif

// Uses a lookup table to unpack up to 4 bits at a time
// The idea is to do like the switch table above but to use tail call optimization
// to chain into the next group of 4 bits. This increases the number of branch
// instructions allowing for more opportunity for branch prediction to memorize
// a useful pattern
template<class track_writer_type>
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
void bitset_iter_lookup_table(
	const uint32_t* packed_entries, uint32_t last_entry_index,
	uint32_t padding_mask,
	track_writer_type& writer)
{
	const uint32_t* packed_entries_ptr = packed_entries;
	const uint32_t* packed_entries_last_ptr = packed_entries + last_entry_index;

	const rtm::quatf default_rotation = rtm::quat_identity();

	bitset_iter_loopup_helper<track_writer_type> helper;

	if (packed_entries_ptr <= packed_entries_last_ptr)
	{
		uint32_t packed_entry = *packed_entries_ptr;

		// Mask out everything but default sub-tracks, this way we can early out when we iterate
		// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
		// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
		// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
		// Finally, we mask out everything but the second bit for each sub-track
		// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
		// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
		packed_entry = ~packed_entry - 0x55555555;

		// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
		const uint32_t entry_padding_mask = packed_entries_ptr == packed_entries_last_ptr ? padding_mask : 0xAAAAAAAA;
		packed_entry &= entry_padding_mask;

		// We have 2 bits per sub-track
		uint32_t curr_entry_track_index = 0;
		packed_entries_ptr++;

#if ACL_IMPL_TABLE_DISPATCH_VARIANT == 0
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;
		packed_entry <<= 8;

		(*helper.dispatch_table[packed_nibble])(packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, writer);
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 1
		// First 4 bits (2 sub-tracks): b1010
		uint32_t packed_nibble = packed_entry >> 28;

		// Second 4 bits (2 sub-tracks): b0101
		// Combine with the first
		packed_nibble |= packed_entry >> 25;

		// Mask out rest
		// What remains are out sub-tracks but interleaved: 0, 2, 1, 3
		packed_nibble &= 0x0F;
		packed_entry <<= 8;

		dispatch_table_helper_t dispatch_helper{ helper.dispatch_table };
		(*helper.dispatch_table[packed_nibble])(dispatch_helper, packed_entries_ptr, packed_entries_last_ptr, padding_mask, packed_entry, curr_entry_track_index, default_rotation, &writer);
#elif ACL_IMPL_TABLE_DISPATCH_VARIANT == 2
		// Interleave the sub-tracks in upper 16-bits
		// b10101010,10101010,10101010,10101010 [0, _, 1, _, 2, _, 3, _] [4, _, 5, _, 6, _, 7, _] [8, _, 9, _, 10, _, 11, _] [12, _, 13, _, 14, _, 15, _]
		// or
		// b01010101,01010101,01010101,01010100 [_, 2, _, 3, _, 4, _, 5] [_, 6, _, 7, _, 8, _, 9] [_, 10, _, 11, _, 12, _, 13] [_, 14, _, 15, _, _, _, _]
		// =
		// [0, 2, 1, 3, 2, 4, 3, 5] [4, 6, 5, 7, 6, 8, 7, 9] [8, 10, 9, 11, 10, 12, 11, 13] [12, 14, 13, 15, 14, _, 15, _]
		// We'll read the first nibble of each byte:
		// [0, 2, 1, 3] [4, 6, 5, 7] [8, 10, 9, 11] [12, 14, 13, 15]
		packed_entry |= packed_entry << 3;

		const uint32_t packed_nibble = packed_entry >> 28;
		packed_entry <<= 8;

		dispatch_helper_t dispatch_helper;
		//dispatch_helper.default_rotation = default_rotation;
		dispatch_helper.packed_entries_ptr = packed_entries_ptr;
		dispatch_helper.packed_entries_last_ptr = packed_entries_last_ptr;
		dispatch_helper.padding_mask = padding_mask;
		std::memcpy(dispatch_helper.dispatch_table, helper.dispatch_table, sizeof(helper.dispatch_table));

		(*helper.dispatch_table[packed_nibble])(dispatch_helper, packed_entry, curr_entry_track_index, &writer, default_rotation);
#endif
	}
}

template<class ...args_>
static void bm_bitset_iter_lookup_table(benchmark::State& state, args_&&... args)
{
	const auto args_tuple = std::make_tuple(std::move(args)...);
	const double bit_set_density = k_bit_set_densities[std::get<0>(args_tuple)];
	const writer_cost_t writer_cost = std::get<1>(args_tuple);

	constexpr uint32_t k_num_bits_per_entry = 32;
	constexpr uint32_t k_num_packed_entries = k_num_bits_in_bit_set / k_num_bits_per_entry;
	uint32_t* packed_entries = new uint32_t[k_num_packed_entries];
	std::fill(packed_entries, packed_entries + k_num_packed_entries, 0);

	setup_bit_set(bit_set_density, packed_entries, k_num_packed_entries);

	rtm::qvvf* output = new rtm::qvvf[k_num_packed_entries * k_num_bits_per_entry];

	if (writer_cost == writer_cost_t::light)
	{
		track_writer_light writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_lookup_table(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}
	else
	{
		track_writer_heavy writer;
		writer.output = output;

		for (auto _ : state)
			bitset_iter_lookup_table(packed_entries, k_num_packed_entries - 1, 0xFFFFFFFFU, writer);
	}

	delete[] packed_entries;
	delete[] output;
}

BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d30_light, 0, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d60_light, 1, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d75_light, 3, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d80_light, 4, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d85_light, 5, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d90_light, 2, writer_cost_t::light);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d30_heavy, 0, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d60_heavy, 1, writer_cost_t::heavy);
BENCHMARK_CAPTURE(bm_bitset_iter_lookup_table, d90_heavy, 2, writer_cost_t::heavy);
#endif // defined(ACL_IMPL_BENCHMARK_BIT_SET_ITERATION)
