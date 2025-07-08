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

#if defined(ACL_IMPL_BENCHMARK_UNPACKING)

#include <rtm/vector4f.h>

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_ref(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	struct PackedTableEntry
	{
		explicit constexpr PackedTableEntry(uint8_t num_bits_)
			: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
			, mask((1U << num_bits_) - 1)
		{}

		float max_value;
		uint32_t mask;
	};

	alignas(64) static constexpr PackedTableEntry k_packed_constants[24] =
	{
		PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
		PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
		PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
		PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
		PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		PackedTableEntry(20), PackedTableEntry(21), PackedTableEntry(22), PackedTableEntry(23),
	};

	const uint32_t bit_shift = 32 - num_bits;
	const uint32_t mask = k_packed_constants[num_bits].mask;
	const float inv_max_value = k_packed_constants[num_bits].max_value;

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8))) & mask;

	return rtm::vector_mul(rtm::vector_set(float(x32), float(y32), float(z32)), inv_max_value);
}

#if defined(RTM_NEON_INTRINSICS)
// Implementation from ACL 2.1
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_neon_v0(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	struct PackedTableEntry
	{
		explicit constexpr PackedTableEntry(uint8_t num_bits_)
			: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
			, mask((1U << num_bits_) - 1)
		{}

		float max_value;
		uint32_t mask;
	};

	alignas(64) static constexpr PackedTableEntry k_packed_constants[24] =
	{
		PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
		PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
		PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
		PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
		PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		PackedTableEntry(20), PackedTableEntry(21), PackedTableEntry(22), PackedTableEntry(23),
	};

	const uint32_t bit_shift = 32 - num_bits;
#if defined(RTM_COMPILER_MSVC)
	// MSVC uses an alias
	uint32x4_t mask = vdupq_n_u32(static_cast<int32_t>(k_packed_constants[num_bits].mask));
#else
	uint32x4_t mask = vdupq_n_u32(k_packed_constants[num_bits].mask);
#endif
	float inv_max_value = k_packed_constants[num_bits].max_value;

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	uint32x2_t xy = vcreate_u32(uint64_t(x32) | (uint64_t(y32) << 32));
	uint32x2_t z = vcreate_u32(uint64_t(z32));
	uint32x4_t value_u32 = vcombine_u32(xy, z);
	value_u32 = vandq_u32(value_u32, mask);
	float32x4_t value_f32 = vcvtq_f32_u32(value_u32);
	return vmulq_n_f32(value_f32, inv_max_value);
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_neon_v1(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// We use at most 23 bits per component and so we only care about the
	// first 23*3+7=76 bits as we might need to shift up to 7 bits past our byte
	// offset. That means we care about the first 10 bytes.
	// Our X component will live within bytes 0-4, Y within 0-6, and Z within 0-9
	// depending on: our initial bit offset and how many bits per component we have
	// For each number of bits per component, we can build a selection mask from our 16-byte
	// value.
	// num bits: {bit range from MSB} = {byte range from MSB}
	// 0 : dummy, not used
	// 1 : {[0,8],  [1,9],   [2,10]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 2 : {[0,9],  [2,11],  [4,13]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 3 : {[0,10], [3,13],  [6,16]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 4 : {[0,11], [4,15],  [8,19]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 5 : {[0,12], [5,17],  [10,22]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 6 : {[0,13], [6,19],  [12,25]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 7 : {[0,14], [7,21],  [14,28]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 8 : {[0,15], [8,23],  [16,31]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 9 : {[0,16], [9,25],  [18,34]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 10: {[0,17], [10,27], [20,37]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 11: {[0,18], [11,29], [22,40]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 12: {[0,19], [12,31], [24,43]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 13: {[0,20], [13,33], [26,46]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 14: {[0,21], [14,35], [28,49]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 15: {[0,22], [15,37], [30,52]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 16: {[0,23], [16,39], [32,55]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 17: {[0,24], [17,41], [34,58]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7]}
	// 18: {[0,25], [18,43], [36,61]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7]}
	// 19: {[0,26], [19,45], [38,64]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7,8]}
	// 20: {[0,27], [20,47], [40,67]} = {[0,1,2,3], [2,3,4,5],   [5,6,7,8]}
	// 21: {[0,28], [21,49], [42,70]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8]}
	// 22: {[0,29], [22,51], [44,73]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8,9]}
	// 23: {[0,30], [23,53], [46,76]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8,9]}
	// As we can see, some values require 5 bytes to reconstruct. As such, we need to use
	// 64-bit values for each lane until we shift out the excess. If we need 8 bytes
	// per mask for each lane, then it becomes obvious that X and Y can use the
	// same mask value: they both live within the first 8 bytes. The Z lane is different.
	// We can build two masks for it: one for values below 16, and another for values
	// equal or above. This allows us to easily select the mask based on the 5th bit:
	// if set, the value is 16 or above. We thus need just 3 mask values:
	// XY: {0,1,2,3,4,5,6,7}
	// Z: {0,1,2,3,4,5,6,7} and {4,5,6,7,8,9,10,11}
	// Z's first mask can thus share the one used by XY.
	//
	// We can use the shuffle to also perform the byte swap operation trivially and treat
	// the resulting 64-bit numbers normally.
	//
	// Each lane now contains the required bits but we must shift by an amount specific to
	// each lane and finally we must mask out the extra bits. The mask can trivially be computed.
	// This leaves the shift offset to figure out.
	// With NEON, we do not have a SIMD right shift that takes an integer in each lane but
	// we do have a SIMD left shift. If we use a negative shift offset, it then becomes a
	// truncating right shift (see vshlq_u32 and the USHL instruction).
	// For each SIMD lane, we want to shift by a custom amount plus the base bit offset.
	// num bits: {bit range from MSB} = {right shift offset}
	// 0 : dummy, not used
	// 1 : {[0],  [1],  [2]}
	// 2 : {[0],  [2],  [4]}
	// 3 : {[0],  [3],  [6]}
	// 4 : {[0],  [4],  [8]}
	// 5 : {[0],  [5],  [10]}
	// 6 : {[0],  [6],  [12]}
	// 7 : {[0],  [7],  [14]}
	// 8 : {[0],  [8],  [16]}
	// 9 : {[0],  [9],  [18]}
	// 10: {[0],  [10], [20]}
	// 11: {[0],  [11], [22]}
	// 12: {[0],  [12], [24]}
	// 13: {[0],  [13], [26]}
	// 14: {[0],  [14], [28]}
	// 15: {[0],  [15], [30]}
	// 16: {[0],  [16], [0]}
	// 17: {[0],  [17], [2]}
	// 18: {[0],  [18], [4]}
	// 19: {[0],  [19], [6]}
	// 20: {[0],  [20], [8]}
	// 21: {[0],  [21], [10]}
	// 22: {[0],  [22], [12]}
	// 23: {[0],  [23], [14]}
	// For the XY lanes, because the first byte is byte 0, the shift offset is simply how
	// many bites the previous lane consumed: always 0 for X, and num bits for Y.
	// Z is different for values 16 and above because the first byte is the 4th. This
	// byte starts at bit offset 32 and so we must shift by that amount plus the extra bits
	// consumed by the prior lane in that byte. It so happens to start at that bit offset.
	// These values can easily be synthetized to avoid a potential cache miss and minimize
	// the number of constants we have.

	// Total size: 4*24 = 96
	struct NEONConstants_t
	{
		float max_value[24];
	};

	alignas(128) static constexpr NEONConstants_t k_packed_constants =
	{
		{
			1.0F, (1.0F / float((1 << 1) - 1)), (1.0F / float((1 << 2) - 1)), (1.0F / float((1 << 3) - 1)),
			(1.0F / float((1 << 4) - 1)), (1.0F / float((1 << 5) - 1)), (1.0F / float((1 << 6) - 1)), (1.0F / float((1 << 7) - 1)),
			(1.0F / float((1 << 8) - 1)), (1.0F / float((1 << 9) - 1)), (1.0F / float((1 << 10) - 1)), (1.0F / float((1 << 11) - 1)),
			(1.0F / float((1 << 12) - 1)), (1.0F / float((1 << 13) - 1)), (1.0F / float((1 << 14) - 1)), (1.0F / float((1 << 15) - 1)),
			(1.0F / float((1 << 16) - 1)), (1.0F / float((1 << 17) - 1)), (1.0F / float((1 << 18) - 1)), (1.0F / float((1 << 19) - 1)),
			(1.0F / float((1 << 20) - 1)), (1.0F / float((1 << 21) - 1)), (1.0F / float((1 << 22) - 1)), (1.0F / float((1 << 23) - 1)),
		}
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	const uint8x16_t raw_bytes = vld1q_u8(vector_data + byte_offset);

	// Select and swizzle using our mask
	const uint8_t swizzle_mask_z_offset = static_cast<uint8_t>((num_bits >> 2) & 0x04);	// num_bits >= 16 ? 4 : 0

#if defined(RTM_NEON64_INTRINSICS)
	const uint8x16_t swizzle_mask_base = vreinterpretq_u8_u64(vmovq_n_u64(0x0001020304050607ULL));
	const uint8x16_t swizzle_mask_xy = swizzle_mask_base;
	const uint8x16_t swizzle_mask_zw = vaddq_u8(swizzle_mask_base, vmovq_n_u8(swizzle_mask_z_offset));

	uint64x2_t xy = vreinterpretq_u64_u8(vqtbl1q_u8(raw_bytes, swizzle_mask_xy));
	uint64x2_t zw = vreinterpretq_u64_u8(vqtbl1q_u8(raw_bytes, swizzle_mask_zw));
#else
	const uint8x8_t swizzle_mask_base = vreinterpret_u8_u64(vmov_n_u64(0x0001020304050607ULL));
	const uint8x8x2_t raw_bytes_split = { vget_low_u8(raw_bytes), vget_high_u8(raw_bytes) };
	const uint8x8_t swizzle_mask_xy = swizzle_mask_base;
	const uint8x8_t swizzle_mask_zw = vadd_u8(swizzle_mask_base, vmov_n_u8(swizzle_mask_z_offset));

	uint64x2_t xy = vdupq_lane_u64(vreinterpret_u64_u8(vtbl1_u8(vget_low_u8(raw_bytes), swizzle_mask_xy)), 0);
	uint64x2_t zw = vdupq_lane_u64(vreinterpret_u64_u8(vtbl2_u8(raw_bytes_split, swizzle_mask_zw)), 0);
#endif

	// Shift out the extra bits
	const int64x2_t shift_offset_xy = vreinterpretq_s64_u64(vcombine_u64(vcreate_u64(base_bit_offset), vcreate_u64(base_bit_offset + num_bits)));
	const int64x2_t shift_offset_zw = vreinterpretq_s64_u64(vmovq_n_u64(base_bit_offset + ((num_bits % 16) * 2)));

	// Shift left to truncate the extra leading bits
	xy = vshlq_u64(xy, shift_offset_xy);
	zw = vshlq_u64(zw, shift_offset_zw);

	// Shift right to bring them in the right place at the bottom
	const int64x2_t shift_num_bits = vmovq_n_s64(-int64_t(64 - num_bits));
	xy = vshlq_u64(xy, shift_num_bits);
	zw = vshlq_u64(zw, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}
#if defined(RTM_NEON64_INTRINSICS)
	const uint32x4_t xyzw_u32 = vuzp1q_u32(vreinterpretq_u32_u64(xy), vreinterpretq_u32_u64(zw));
#else
	const uint32x4_t xyzw_u32 = vuzpq_u32(vreinterpretq_u32_u64(xy), vreinterpretq_u32_u64(zw)).val[0];
#endif

	// Convert to float and re-scale
	const float inv_max_value = k_packed_constants.max_value[num_bits];

	float32x4_t xyzw_f32 = vcvtq_f32_u32(xyzw_u32);
	return vmulq_n_f32(xyzw_f32, inv_max_value);
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_neon_v2(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// We use at most 23 bits per component and so we only care about the
	// first 23*3+7=76 bits as we might need to shift up to 7 bits past our byte
	// offset. That means we care about the first 10 bytes.
	// Our X component will live within bytes 0-4, Y within 0-6, and Z within 0-9
	// depending on: our initial bit offset and how many bits per component we have
	// For each number of bits per component, we can build a selection mask from our 16-byte
	// value.
	// num bits: {bit range from MSB} = {byte range from MSB}
	// 0 : dummy, not used
	// 1 : {[0,8],  [1,9],   [2,10]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}	mask 0 ((x-1)/8) = 0
	// 2 : {[0,9],  [2,11],  [4,13]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 3 : {[0,10], [3,13],  [6,16]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 4 : {[0,11], [4,15],  [8,19]}  = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 5 : {[0,12], [5,17],  [10,22]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 6 : {[0,13], [6,19],  [12,25]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 7 : {[0,14], [7,21],  [14,28]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 8 : {[0,15], [8,23],  [16,31]} = {[0,1,2,3], [0,1,2,3],   [0,1,2,3]}
	// 9 : {[0,16], [9,25],  [18,34]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}	mask 1 ((x-1)/8) = 1
	// 10: {[0,17], [10,27], [20,37]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 11: {[0,18], [11,29], [22,40]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 12: {[0,19], [12,31], [24,43]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 13: {[0,20], [13,33], [26,46]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5]}
	// 14: {[0,21], [14,35], [28,49]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 15: {[0,22], [15,37], [30,52]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 16: {[0,23], [16,39], [32,55]} = {[0,1,2,3], [1,2,3,4],   [2,3,4,5,6]}
	// 17: {[0,24], [17,41], [34,58]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7]}	mask 2 ((x-1)/8) = 2
	// 18: {[0,25], [18,43], [36,61]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7]}
	// 19: {[0,26], [19,45], [38,64]} = {[0,1,2,3], [2,3,4,5],   [4,5,6,7,8]}
	// 20: {[0,27], [20,47], [40,67]} = {[0,1,2,3], [2,3,4,5],   [5,6,7,8]}
	// 21: {[0,28], [21,49], [42,70]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8]}
	// 22: {[0,29], [22,51], [44,73]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8,9]}
	// 23: {[0,30], [23,53], [46,76]} = {[0,1,2,3], [2,3,4,5,6], [5,6,7,8,9]}
	// 32: {[0,39], [32,71], [64,103]} = {[0,1,2,3,4], [4,5,6,7,8], [8,9,10,11,12]}
	// As we can see, some values require 5 bytes to reconstruct. As such, we need to use
	// 64-bit values for each lane until we shift out the excess. If we need 8 bytes
	// per mask for each lane, then it becomes obvious that X and Y can use the
	// same mask value: they both live within the first 8 bytes. The Z lane is different.
	// We can build two masks for it: one for values below 16, and another for values
	// equal or above. This allows us to easily select the mask based on the 5th bit:
	// if set, the value is 16 or above. We thus need just 3 mask values:
	// XY: {0,1,2,3,4,5,6,7}
	// Z: {0,1,2,3,4,5,6,7} and {4,5,6,7,8,9,10,11}
	// Z's first mask can thus share the one used by XY.
	// Note that this is only true up to 23 bits, if we wish to re-use the same logic for
	// 32-bit values, Y needs its own mask and Z needs a third option as well.
	// X: {0,1,2,3,4,5,6,7}
	// Y: {0,1,2,3,4,5,6,7} or {4,5,6,7,8,9,10,11}
	// Z: {0,1,2,3,4,5,6,7} or {4,5,6,7,8,9,10,11} or {8,9,10,11,12,13,14,15}
	//
	// We can use the shuffle to also perform the byte swap operation trivially and treat
	// the resulting 64-bit numbers normally.
	//
	// Each lane now contains the required bits but we must shift by an amount specific to
	// each lane and finally we must mask out the extra bits. The mask can trivially be computed.
	// This leaves the shift offset to figure out.
	// With NEON, we do not have a SIMD right shift that takes an integer in each lane but
	// we do have a SIMD left shift. If we use a negative shift offset, it then becomes a
	// truncating right shift (see vshlq_u32 and the USHL instruction).
	// For each SIMD lane, we want to shift by a custom amount plus the base bit offset.
	// num bits: {bit range from MSB} = {right shift offset}
	// 0 : dummy, not used
	// 1 : {[0],  [1],  [2]}
	// 2 : {[0],  [2],  [4]}
	// 3 : {[0],  [3],  [6]}
	// 4 : {[0],  [4],  [8]}
	// 5 : {[0],  [5],  [10]}
	// 6 : {[0],  [6],  [12]}
	// 7 : {[0],  [7],  [14]}
	// 8 : {[0],  [8],  [16]}
	// 9 : {[0],  [9],  [18]}
	// 10: {[0],  [10], [20]}
	// 11: {[0],  [11], [22]}
	// 12: {[0],  [12], [24]}
	// 13: {[0],  [13], [26]}
	// 14: {[0],  [14], [28]}
	// 15: {[0],  [15], [30]}
	// 16: {[0],  [16], [0]}
	// 17: {[0],  [17], [2]}
	// 18: {[0],  [18], [4]}
	// 19: {[0],  [19], [6]}
	// 20: {[0],  [20], [8]}
	// 21: {[0],  [21], [10]}
	// 22: {[0],  [22], [12]}
	// 23: {[0],  [23], [14]}
	// 32: {[0],  [0], [0]}
	// For the XY lanes, because the first byte is byte 0, the shift offset is simply how
	// many bites the previous lane consumed: always 0 for X, and num bits for Y.
	// Z is different for values 16 and above because the first byte is the 4th. This
	// byte starts at bit offset 32 and so we must shift by that amount plus the extra bits
	// consumed by the prior lane in that byte. It so happens to start at that bit offset.
	// These values can easily be synthetized to avoid a potential cache miss and minimize
	// the number of constants we have.

	struct NEONConstants_t
	{
		explicit constexpr NEONConstants_t(int32_t num_bits_)
			: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
			, shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(-int8_t(64 - num_bits_))
		{}

		float max_value;

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		int8_t shift_num_bits;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr NEONConstants_t k_packed_constants[24] =
	{
		NEONConstants_t(0), NEONConstants_t(1), NEONConstants_t(2), NEONConstants_t(3),
		NEONConstants_t(4), NEONConstants_t(5), NEONConstants_t(6), NEONConstants_t(7),
		NEONConstants_t(8), NEONConstants_t(9), NEONConstants_t(10), NEONConstants_t(11),
		NEONConstants_t(12), NEONConstants_t(13), NEONConstants_t(14), NEONConstants_t(15),
		NEONConstants_t(16), NEONConstants_t(17), NEONConstants_t(18), NEONConstants_t(19),
		NEONConstants_t(20), NEONConstants_t(21), NEONConstants_t(22), NEONConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	const uint8x16_t raw_bytes = vld1q_u8(vector_data + byte_offset);

	// Select and swizzle using our mask
	const uint8_t swizzle_mask_z_offset = static_cast<uint8_t>((num_bits >> 2) & 0x04);	// num_bits >= 16 ? 4 : 0

#if defined(RTM_NEON64_INTRINSICS)
	const uint8x16_t swizzle_mask_base = vreinterpretq_u8_u64(vmovq_n_u64(0x0001020304050607ULL));
	const uint8x16_t swizzle_mask_xy = swizzle_mask_base;
	const uint8x16_t swizzle_mask_zw = vaddq_u8(swizzle_mask_base, vmovq_n_u8(swizzle_mask_z_offset));

	uint64x2_t xy = vreinterpretq_u64_u8(vqtbl1q_u8(raw_bytes, swizzle_mask_xy));
	uint64x2_t zw = vreinterpretq_u64_u8(vqtbl1q_u8(raw_bytes, swizzle_mask_zw));
#else
	const uint8x8_t swizzle_mask_base = vreinterpret_u8_u64(vmov_n_u64(0x0001020304050607ULL));
	const uint8x8x2_t raw_bytes_split = { vget_low_u8(raw_bytes), vget_high_u8(raw_bytes) };
	const uint8x8_t swizzle_mask_xy = swizzle_mask_base;
	const uint8x8_t swizzle_mask_zw = vadd_u8(swizzle_mask_base, vmov_n_u8(swizzle_mask_z_offset));

	uint64x2_t xy = vdupq_lane_u64(vreinterpret_u64_u8(vtbl1_u8(vget_low_u8(raw_bytes), swizzle_mask_xy)), 0);
	uint64x2_t zw = vdupq_lane_u64(vreinterpret_u64_u8(vtbl2_u8(raw_bytes_split, swizzle_mask_zw)), 0);
#endif

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const int8x8_t raw_constant_bytes_s8 = vld1_s8((const int8_t*)&k_packed_constants[num_bits]);
	const int16x8_t raw_constant_bytes_s16 = vmovl_s8(raw_constant_bytes_s8);
	const int32x4_t raw_shift_offsets_s32 = vmovl_s16(vget_high_s16(raw_constant_bytes_s16));
	const int64x2_t base_shift_offset_xy = vmovl_s32(vget_low_s32(raw_shift_offsets_s32));
	const int64x2_t base_shift_offset_zw = vmovl_s32(vget_high_s32(raw_shift_offsets_s32));

	// Shift out the extra bits
	// TODO: shift by base separately, swap add x2 to shift x2, avoid dependency
	const int64x2_t base_bit_offset_s64 = vreinterpretq_s64_u64(vmovq_n_u64(base_bit_offset));
	const int64x2_t shift_offset_xy = vaddq_s64(base_bit_offset_s64, base_shift_offset_xy);
	const int64x2_t shift_offset_zw = vaddq_s64(base_bit_offset_s64, base_shift_offset_zw);

	// Shift left to truncate the extra leading bits
	xy = vshlq_u64(xy, shift_offset_xy);
	zw = vshlq_u64(zw, shift_offset_zw);

	// Shift right to bring them in the right place at the bottom
	const int64x2_t shift_num_bits = vdupq_lane_s64(vget_high_s64(base_shift_offset_zw), 0);
	xy = vshlq_u64(xy, shift_num_bits);
	zw = vshlq_u64(zw, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}
#if defined(RTM_NEON64_INTRINSICS)
	const uint32x4_t xyzw_u32 = vuzp1q_u32(vreinterpretq_u32_u64(xy), vreinterpretq_u32_u64(zw));
#else
	const uint32x4_t xyzw_u32 = vuzpq_u32(vreinterpretq_u32_u64(xy), vreinterpretq_u32_u64(zw)).val[0];
#endif

	// Convert to float and re-scale
	const float32x4_t xyzw_f32 = vcvtq_f32_u32(xyzw_u32);
	return vmulq_n_f32(xyzw_f32, vget_lane_f32(vreinterpret_f32_s8(raw_constant_bytes_s8), 0));
}
#endif	// defined(RTM_NEON_INTRINSICS)

#if defined(RTM_SSE2_INTRINSICS)
// Implementation from ACL 2.1
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse2_v0(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	struct PackedTableEntry
	{
		explicit constexpr PackedTableEntry(uint8_t num_bits_)
			: max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
			, mask((1U << num_bits_) - 1)
		{}

		float max_value;
		uint32_t mask;
	};

	alignas(64) static constexpr PackedTableEntry k_packed_constants[24] =
	{
		PackedTableEntry(0), PackedTableEntry(1), PackedTableEntry(2), PackedTableEntry(3),
		PackedTableEntry(4), PackedTableEntry(5), PackedTableEntry(6), PackedTableEntry(7),
		PackedTableEntry(8), PackedTableEntry(9), PackedTableEntry(10), PackedTableEntry(11),
		PackedTableEntry(12), PackedTableEntry(13), PackedTableEntry(14), PackedTableEntry(15),
		PackedTableEntry(16), PackedTableEntry(17), PackedTableEntry(18), PackedTableEntry(19),
		PackedTableEntry(20), PackedTableEntry(21), PackedTableEntry(22), PackedTableEntry(23),
	};

	const uint32_t bit_shift = 32 - num_bits;
	const __m128i mask = _mm_castps_si128(_mm_load_ps1((const float*)&k_packed_constants[num_bits].mask));
	const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants[num_bits].max_value);

	uint32_t byte_offset = bit_offset / 8;
	uint32_t vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t x32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t y32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	bit_offset += num_bits;

	byte_offset = bit_offset / 8;
	vector_u32 = acl::unaligned_load<uint32_t>(vector_data + byte_offset);
	vector_u32 = acl::byte_swap(vector_u32);
	const uint32_t z32 = (vector_u32 >> (bit_shift - (bit_offset % 8)));

	__m128i int_value = _mm_set_epi32(static_cast<int32_t>(x32), static_cast<int32_t>(z32), static_cast<int32_t>(y32), static_cast<int32_t>(x32));
	int_value = _mm_and_si128(int_value, mask);
	const __m128 value = _mm_cvtepi32_ps(int_value);
	return _mm_mul_ps(value, inv_max_value);
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse2_v1(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as NEON v1 (simd + create constants manually)

	// Total size: 4*24 = 96
	struct SSEConstants_t
	{
		float max_value[24];
	};

	alignas(128) static constexpr SSEConstants_t k_packed_constants =
	{
		{
			1.0F, (1.0F / float((1 << 1) - 1)), (1.0F / float((1 << 2) - 1)), (1.0F / float((1 << 3) - 1)),
			(1.0F / float((1 << 4) - 1)), (1.0F / float((1 << 5) - 1)), (1.0F / float((1 << 6) - 1)), (1.0F / float((1 << 7) - 1)),
			(1.0F / float((1 << 8) - 1)), (1.0F / float((1 << 9) - 1)), (1.0F / float((1 << 10) - 1)), (1.0F / float((1 << 11) - 1)),
			(1.0F / float((1 << 12) - 1)), (1.0F / float((1 << 13) - 1)), (1.0F / float((1 << 14) - 1)), (1.0F / float((1 << 15) - 1)),
			(1.0F / float((1 << 16) - 1)), (1.0F / float((1 << 17) - 1)), (1.0F / float((1 << 18) - 1)), (1.0F / float((1 << 19) - 1)),
			(1.0F / float((1 << 20) - 1)), (1.0F / float((1 << 21) - 1)), (1.0F / float((1 << 22) - 1)), (1.0F / float((1 << 23) - 1)),
		}
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	// [_,0,_,2,_,4,_,6], [_,8,_,10,_,12,_,14]
	const __m128i even_shifted = _mm_srli_epi16(raw_bytes, 8);
	// [1,_,3,_,5,_,7,_], [9,_,11,_,13,_,15,_]
	const __m128i odd_shifted = _mm_slli_epi16(raw_bytes, 8);
	// [1,0,3,2,5,4,7,6], [9,8,11,10,13,12,15,14]
	__m128i rev_raw_bytes = _mm_or_si128(even_shifted, odd_shifted);
	// [7,6,5,4,3,2,1,0], [9,8,11,10,13,12,15,14]
	rev_raw_bytes = _mm_shufflelo_epi16(rev_raw_bytes, 0x1B);
	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	rev_raw_bytes = _mm_shufflehi_epi16(rev_raw_bytes, 0x1B);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_or_si128(_mm_and_si128(rev_raw_bytes, swizzle_mask_z), _mm_andnot_si128(swizzle_mask_z, higher_z));

	// Shift out the extra bits
	const __m128i shift_offset_x = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_set_epi64x(0, base_bit_offset + num_bits);
	const __m128i shift_offset_z = _mm_set_epi64x(0, base_bit_offset + ((num_bits % 16) * 2));

	// Shift left to truncate the extra leading bits
	x = _mm_sll_epi64(x, shift_offset_x);
	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	__m128i xy = _mm_unpacklo_epi64(x, y);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_set_epi64x(0, 64 - num_bits);
	xy = _mm_srl_epi64(xy, shift_num_bits);
	z = _mm_srl_epi64(z, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m128i xzyz_u32 = _mm_or_si128(xy, _mm_slli_epi64(z, 32));

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants.max_value[num_bits]);
	const __m128 xzyz_f32 = _mm_mul_ps(_mm_cvtepi32_ps(xzyz_u32), inv_max_value);

	return _mm_shuffle_ps(xzyz_f32, xzyz_f32, _MM_SHUFFLE(1, 1, 2, 0));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse2_v2(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as NEON v2 (simd + load constants)

	struct SSEConstants_t
	{
		explicit constexpr SSEConstants_t(int32_t num_bits_)
			: shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(static_cast<uint8_t>(64 - num_bits_))
			, max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
		{}

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		uint8_t shift_num_bits;

		float max_value;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr SSEConstants_t k_packed_constants[24] =
	{
		SSEConstants_t(0), SSEConstants_t(1), SSEConstants_t(2), SSEConstants_t(3),
		SSEConstants_t(4), SSEConstants_t(5), SSEConstants_t(6), SSEConstants_t(7),
		SSEConstants_t(8), SSEConstants_t(9), SSEConstants_t(10), SSEConstants_t(11),
		SSEConstants_t(12), SSEConstants_t(13), SSEConstants_t(14), SSEConstants_t(15),
		SSEConstants_t(16), SSEConstants_t(17), SSEConstants_t(18), SSEConstants_t(19),
		SSEConstants_t(20), SSEConstants_t(21), SSEConstants_t(22), SSEConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	// [_,0,_,2,_,4,_,6], [_,8,_,10,_,12,_,14]
	const __m128i even_shifted = _mm_srli_epi16(raw_bytes, 8);
	// [1,_,3,_,5,_,7,_], [9,_,11,_,13,_,15,_]
	const __m128i odd_shifted = _mm_slli_epi16(raw_bytes, 8);
	// [1,0,3,2,5,4,7,6], [9,8,11,10,13,12,15,14]
	__m128i rev_raw_bytes = _mm_or_si128(even_shifted, odd_shifted);
	// [7,6,5,4,3,2,1,0], [9,8,11,10,13,12,15,14]
	rev_raw_bytes = _mm_shufflelo_epi16(rev_raw_bytes, 0x1B);
	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	rev_raw_bytes = _mm_shufflehi_epi16(rev_raw_bytes, 0x1B);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_or_si128(_mm_and_si128(rev_raw_bytes, swizzle_mask_z), _mm_andnot_si128(swizzle_mask_z, higher_z));

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const __m128i zero = _mm_setzero_si128();
	const __m128i raw_constant_bytes_u8 = _mm_loadu_si64(&k_packed_constants[num_bits]);
	const __m128i raw_constant_bytes_u16 = _mm_unpacklo_epi8(raw_constant_bytes_u8, zero);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x01);
	const __m128i shift_offset_z = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x02);

	// Shift left to truncate the extra leading bits
	x = _mm_sll_epi64(x, base_bit_offset_u64);
	y = _mm_sll_epi64(y, base_bit_offset_u64);
	z = _mm_sll_epi64(z, base_bit_offset_u64);

	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	__m128i xy = _mm_unpacklo_epi64(x, y);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x03);
	xy = _mm_srl_epi64(xy, shift_num_bits);
	z = _mm_srl_epi64(z, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m128i xzyz_u32 = _mm_or_si128(xy, _mm_slli_epi64(z, 32));

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_castsi128_ps(_mm_shuffle_epi32(raw_constant_bytes_u8, _MM_SHUFFLE(1, 1, 1, 1)));
	const __m128 xzyz_f32 = _mm_mul_ps(_mm_cvtepi32_ps(xzyz_u32), inv_max_value);

	return _mm_shuffle_ps(xzyz_f32, xzyz_f32, _MM_SHUFFLE(1, 1, 2, 0));
}
#endif	// defined(RTM_SSE2_INTRINSICS)

#if defined(RTM_SSE3_INTRINSICS)
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse3_v0(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE2 v1

	// Total size: 4*24 = 96
	struct SSEConstants_t
	{
		float max_value[24];
	};

	alignas(128) static constexpr SSEConstants_t k_packed_constants =
	{
		{
			1.0F, (1.0F / float((1 << 1) - 1)), (1.0F / float((1 << 2) - 1)), (1.0F / float((1 << 3) - 1)),
			(1.0F / float((1 << 4) - 1)), (1.0F / float((1 << 5) - 1)), (1.0F / float((1 << 6) - 1)), (1.0F / float((1 << 7) - 1)),
			(1.0F / float((1 << 8) - 1)), (1.0F / float((1 << 9) - 1)), (1.0F / float((1 << 10) - 1)), (1.0F / float((1 << 11) - 1)),
			(1.0F / float((1 << 12) - 1)), (1.0F / float((1 << 13) - 1)), (1.0F / float((1 << 14) - 1)), (1.0F / float((1 << 15) - 1)),
			(1.0F / float((1 << 16) - 1)), (1.0F / float((1 << 17) - 1)), (1.0F / float((1 << 18) - 1)), (1.0F / float((1 << 19) - 1)),
			(1.0F / float((1 << 20) - 1)), (1.0F / float((1 << 21) - 1)), (1.0F / float((1 << 22) - 1)), (1.0F / float((1 << 23) - 1)),
		}
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// Reverse the bytes in each 64-bit lane
	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_or_si128(_mm_and_si128(rev_raw_bytes, swizzle_mask_z), _mm_andnot_si128(swizzle_mask_z, higher_z));

	// Shift out the extra bits
	const __m128i shift_offset_x = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_set_epi64x(0, base_bit_offset + num_bits);
	const __m128i shift_offset_z = _mm_set_epi64x(0, base_bit_offset + ((num_bits % 16) * 2));

	// Shift left to truncate the extra leading bits
	x = _mm_sll_epi64(x, shift_offset_x);
	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	__m128i xy = _mm_unpacklo_epi64(x, y);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_set_epi64x(0, 64 - num_bits);
	xy = _mm_srl_epi64(xy, shift_num_bits);
	z = _mm_srl_epi64(z, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m128i xzyz_u32 = _mm_or_si128(xy, _mm_slli_epi64(z, 32));

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants.max_value[num_bits]);
	const __m128 xzyz_f32 = _mm_mul_ps(_mm_cvtepi32_ps(xzyz_u32), inv_max_value);

	return _mm_shuffle_ps(xzyz_f32, xzyz_f32, _MM_SHUFFLE(1, 1, 2, 0));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse3_v1(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as NEON v2 (simd + load constants)

	struct SSEConstants_t
	{
		explicit constexpr SSEConstants_t(int32_t num_bits_)
			: shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(static_cast<uint8_t>(64 - num_bits_))
			, max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
		{}

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		uint8_t shift_num_bits;

		float max_value;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr SSEConstants_t k_packed_constants[24] =
	{
		SSEConstants_t(0), SSEConstants_t(1), SSEConstants_t(2), SSEConstants_t(3),
		SSEConstants_t(4), SSEConstants_t(5), SSEConstants_t(6), SSEConstants_t(7),
		SSEConstants_t(8), SSEConstants_t(9), SSEConstants_t(10), SSEConstants_t(11),
		SSEConstants_t(12), SSEConstants_t(13), SSEConstants_t(14), SSEConstants_t(15),
		SSEConstants_t(16), SSEConstants_t(17), SSEConstants_t(18), SSEConstants_t(19),
		SSEConstants_t(20), SSEConstants_t(21), SSEConstants_t(22), SSEConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_or_si128(_mm_and_si128(rev_raw_bytes, swizzle_mask_z), _mm_andnot_si128(swizzle_mask_z, higher_z));

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const __m128i zero = _mm_setzero_si128();
	const __m128i raw_constant_bytes_u8 = _mm_loadu_si64(&k_packed_constants[num_bits]);
	const __m128i raw_constant_bytes_u16 = _mm_unpacklo_epi8(raw_constant_bytes_u8, zero);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x01);
	const __m128i shift_offset_z = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x02);

	// Shift left to truncate the extra leading bits
	x = _mm_sll_epi64(x, base_bit_offset_u64);
	y = _mm_sll_epi64(y, base_bit_offset_u64);
	z = _mm_sll_epi64(z, base_bit_offset_u64);

	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	__m128i xy = _mm_unpacklo_epi64(x, y);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x03);
	xy = _mm_srl_epi64(xy, shift_num_bits);
	z = _mm_srl_epi64(z, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m128i xzyz_u32 = _mm_or_si128(xy, _mm_slli_epi64(z, 32));

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_castsi128_ps(_mm_shuffle_epi32(raw_constant_bytes_u8, _MM_SHUFFLE(1, 1, 1, 1)));
	const __m128 xzyz_f32 = _mm_mul_ps(_mm_cvtepi32_ps(xzyz_u32), inv_max_value);

	return _mm_shuffle_ps(xzyz_f32, xzyz_f32, _MM_SHUFFLE(1, 1, 2, 0));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse3_v2(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE3 v1

	struct SSEConstants_t
	{
		explicit constexpr SSEConstants_t(int32_t num_bits_)
			: shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(static_cast<uint8_t>(64 - num_bits_))
			, max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
		{}

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		uint8_t shift_num_bits;

		float max_value;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr SSEConstants_t k_packed_constants[24] =
	{
		SSEConstants_t(0), SSEConstants_t(1), SSEConstants_t(2), SSEConstants_t(3),
		SSEConstants_t(4), SSEConstants_t(5), SSEConstants_t(6), SSEConstants_t(7),
		SSEConstants_t(8), SSEConstants_t(9), SSEConstants_t(10), SSEConstants_t(11),
		SSEConstants_t(12), SSEConstants_t(13), SSEConstants_t(14), SSEConstants_t(15),
		SSEConstants_t(16), SSEConstants_t(17), SSEConstants_t(18), SSEConstants_t(19),
		SSEConstants_t(20), SSEConstants_t(21), SSEConstants_t(22), SSEConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	static constexpr uint64_t k_swizzle_masks_z[2] = { 0x0001020304050607ULL, 0x0405060708091011ULL };
	const uint32_t swizzle_mask_z_index = num_bits >> 4; // num_bits >= 16 ? 1 : 0

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_shuffle_epi8(raw_bytes, _mm_loadu_si64(&k_swizzle_masks_z[swizzle_mask_z_index]));

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const __m128i zero = _mm_setzero_si128();
	const __m128i raw_constant_bytes_u8 = _mm_loadu_si64(&k_packed_constants[num_bits]);
	const __m128i raw_constant_bytes_u16 = _mm_unpacklo_epi8(raw_constant_bytes_u8, zero);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x01);
	const __m128i shift_offset_z = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x02);

	// Shift left to truncate the extra leading bits
	x = _mm_sll_epi64(x, base_bit_offset_u64);
	y = _mm_sll_epi64(y, base_bit_offset_u64);
	z = _mm_sll_epi64(z, base_bit_offset_u64);

	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	__m128i xy = _mm_unpacklo_epi64(x, y);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x03);
	xy = _mm_srl_epi64(xy, shift_num_bits);
	z = _mm_srl_epi64(z, shift_num_bits);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m128i xzyz_u32 = _mm_or_si128(xy, _mm_slli_epi64(z, 32));

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_castsi128_ps(_mm_shuffle_epi32(raw_constant_bytes_u8, _MM_SHUFFLE(1, 1, 1, 1)));
	const __m128 xzyz_f32 = _mm_mul_ps(_mm_cvtepi32_ps(xzyz_u32), inv_max_value);

	return _mm_shuffle_ps(xzyz_f32, xzyz_f32, _MM_SHUFFLE(1, 1, 2, 0));
}
#endif	// defined(RTM_SSE3_INTRINSICS)

#if defined(RTM_SSE4_INTRINSICS)
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse4_v0(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE3 v0

	// Total size: 4*24 = 96
	struct SSEConstants_t
	{
		float max_value[24];
	};

	alignas(128) static constexpr SSEConstants_t k_packed_constants =
	{
		{
			1.0F, (1.0F / float((1 << 1) - 1)), (1.0F / float((1 << 2) - 1)), (1.0F / float((1 << 3) - 1)),
			(1.0F / float((1 << 4) - 1)), (1.0F / float((1 << 5) - 1)), (1.0F / float((1 << 6) - 1)), (1.0F / float((1 << 7) - 1)),
			(1.0F / float((1 << 8) - 1)), (1.0F / float((1 << 9) - 1)), (1.0F / float((1 << 10) - 1)), (1.0F / float((1 << 11) - 1)),
			(1.0F / float((1 << 12) - 1)), (1.0F / float((1 << 13) - 1)), (1.0F / float((1 << 14) - 1)), (1.0F / float((1 << 15) - 1)),
			(1.0F / float((1 << 16) - 1)), (1.0F / float((1 << 17) - 1)), (1.0F / float((1 << 18) - 1)), (1.0F / float((1 << 19) - 1)),
			(1.0F / float((1 << 20) - 1)), (1.0F / float((1 << 21) - 1)), (1.0F / float((1 << 22) - 1)), (1.0F / float((1 << 23) - 1)),
		}
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// Reverse the bytes in each 64-bit lane
	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_blendv_epi8(higher_z, rev_raw_bytes, swizzle_mask_z);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_set_epi64x(0, num_bits);
	const __m128i shift_offset_z = _mm_set_epi64x(0, (num_bits % 16) * 2);

	// Shift left to truncate the extra leading bits
	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	__m128i xy = _mm_unpacklo_epi64(x, y);
	__m128i zxzy_u32 = _mm_blend_epi16(xy, _mm_srli_epi64(z, 32), 0x33);

	zxzy_u32 = _mm_sll_epi32(zxzy_u32, base_bit_offset_u64);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_set_epi64x(0, 32 - num_bits);
	zxzy_u32 = _mm_srl_epi32(zxzy_u32, shift_num_bits);

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants.max_value[num_bits]);
	const __m128 zxzy_f32 = _mm_mul_ps(_mm_cvtepi32_ps(zxzy_u32), inv_max_value);

	return _mm_shuffle_ps(zxzy_f32, zxzy_f32, _MM_SHUFFLE(0, 0, 3, 1));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse4_v1(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE3 v1

	struct SSEConstants_t
	{
		explicit constexpr SSEConstants_t(int32_t num_bits_)
			: shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(static_cast<uint8_t>(32 - num_bits_))
			, max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
		{}

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		uint8_t shift_num_bits;

		float max_value;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr SSEConstants_t k_packed_constants[24] =
	{
		SSEConstants_t(0), SSEConstants_t(1), SSEConstants_t(2), SSEConstants_t(3),
		SSEConstants_t(4), SSEConstants_t(5), SSEConstants_t(6), SSEConstants_t(7),
		SSEConstants_t(8), SSEConstants_t(9), SSEConstants_t(10), SSEConstants_t(11),
		SSEConstants_t(12), SSEConstants_t(13), SSEConstants_t(14), SSEConstants_t(15),
		SSEConstants_t(16), SSEConstants_t(17), SSEConstants_t(18), SSEConstants_t(19),
		SSEConstants_t(20), SSEConstants_t(21), SSEConstants_t(22), SSEConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi32((num_bits >> 4) - 1);	// num_bits >= 16 ? 0 : ~0

	// [11,10,9,8,7,6,5,4], [11,10,9,8,7,6,5,4]
	const __m128i higher_z = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 0, 3));

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_blendv_epi8(higher_z, rev_raw_bytes, swizzle_mask_z);

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const __m128i raw_constant_bytes_u8 = _mm_loadu_si64(&k_packed_constants[num_bits]);
	const __m128i raw_constant_bytes_u16 = _mm_cvtepu8_epi16(raw_constant_bytes_u8);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x01);
	const __m128i shift_offset_z = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x02);

	// Shift left to truncate the extra leading bits
	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	__m128i xy = _mm_unpacklo_epi64(x, y);
	__m128i zxzy_u32 = _mm_blend_epi16(xy, _mm_srli_epi64(z, 32), 0x33);

	zxzy_u32 = _mm_sll_epi32(zxzy_u32, base_bit_offset_u64);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x03);
	zxzy_u32 = _mm_srl_epi32(zxzy_u32, shift_num_bits);

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_castsi128_ps(_mm_shuffle_epi32(raw_constant_bytes_u8, _MM_SHUFFLE(1, 1, 1, 1)));
	const __m128 zxzy_f32 = _mm_mul_ps(_mm_cvtepi32_ps(zxzy_u32), inv_max_value);

	return _mm_shuffle_ps(zxzy_f32, zxzy_f32, _MM_SHUFFLE(0, 0, 3, 1));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_sse4_v2(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE3 v2

	struct SSEConstants_t
	{
		explicit constexpr SSEConstants_t(int32_t num_bits_)
			: shift_offset_x(0)
			, shift_offset_y(static_cast<uint8_t>(num_bits_))
			, shift_offset_z(static_cast<uint8_t>((num_bits_ % 16) * 2))
			, shift_num_bits(static_cast<uint8_t>(32 - num_bits_))
			, max_value(num_bits_ == 0 ? 1.0F : (1.0F / float((1 << num_bits_) - 1)))
		{}

		uint8_t shift_offset_x;
		uint8_t shift_offset_y;
		uint8_t shift_offset_z;
		uint8_t shift_num_bits;

		float max_value;
	};

	// Total size: 8 * 24 = 192 (3 cache lines)
	// Align to 256 bytes to avoid straddling over a page boundary
	alignas(256) static constexpr SSEConstants_t k_packed_constants[24] =
	{
		SSEConstants_t(0), SSEConstants_t(1), SSEConstants_t(2), SSEConstants_t(3),
		SSEConstants_t(4), SSEConstants_t(5), SSEConstants_t(6), SSEConstants_t(7),
		SSEConstants_t(8), SSEConstants_t(9), SSEConstants_t(10), SSEConstants_t(11),
		SSEConstants_t(12), SSEConstants_t(13), SSEConstants_t(14), SSEConstants_t(15),
		SSEConstants_t(16), SSEConstants_t(17), SSEConstants_t(18), SSEConstants_t(19),
		SSEConstants_t(20), SSEConstants_t(21), SSEConstants_t(22), SSEConstants_t(23),
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	// Reverse the bytes in each 64-bit lane
	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select and swizzle using our mask
	static constexpr uint64_t k_swizzle_masks_z[2] = { 0x0001020304050607ULL, 0x0405060708091011ULL };
	const uint32_t swizzle_mask_z_index = num_bits >> 4; // num_bits >= 16 ? 1 : 0

	__m128i x = rev_raw_bytes;
	__m128i y = rev_raw_bytes;
	__m128i z = _mm_shuffle_epi8(raw_bytes, _mm_loadu_si64(&k_swizzle_masks_z[swizzle_mask_z_index]));

	// Even though it is easy to compute the shift offsets on demand, we pre-compute them
	// and load them here. We already pay the price of a load instruction for the inverse
	// max value float which is too expensive to compute on demand. As such, we tack on
	// another 4 bytes for the shift offsets and unpack them here. This uses fewer instructions.
	const __m128i raw_constant_bytes_u8 = _mm_loadu_si64(&k_packed_constants[num_bits]);
	const __m128i raw_constant_bytes_u16 = _mm_cvtepu8_epi16(raw_constant_bytes_u8);

	// Shift out the extra bits
	const __m128i base_bit_offset_u64 = _mm_set_epi64x(0, base_bit_offset);
	const __m128i shift_offset_y = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x01);
	const __m128i shift_offset_z = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x02);

	// Shift left to truncate the extra leading bits
	y = _mm_sll_epi64(y, shift_offset_y);
	z = _mm_sll_epi64(z, shift_offset_z);

	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	__m128i xy = _mm_unpacklo_epi64(x, y);
	__m128i zxzy_u32 = _mm_blend_epi16(xy, _mm_srli_epi64(z, 32), 0x33);

	zxzy_u32 = _mm_sll_epi32(zxzy_u32, base_bit_offset_u64);

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_shufflelo_epi16(raw_constant_bytes_u16, 0x03);
	zxzy_u32 = _mm_srl_epi32(zxzy_u32, shift_num_bits);

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_castsi128_ps(_mm_shuffle_epi32(raw_constant_bytes_u8, _MM_SHUFFLE(1, 1, 1, 1)));
	const __m128 zxzy_f32 = _mm_mul_ps(_mm_cvtepi32_ps(zxzy_u32), inv_max_value);

	return _mm_shuffle_ps(zxzy_f32, zxzy_f32, _MM_SHUFFLE(0, 0, 3, 1));
}
#endif	// defined(RTM_SSE4_INTRINSICS)

// TODO: Can use use a float64 multiply to do per lane shift?
// In order to use float64 multiplication to perform a bitwise shift, we need to load our
// bytes such that we don't exceed the 53-bit mantissa
// When loading, instead of swizzling and aligning to the MSB byte boundary, we can
// do the same but align at the 6th byte boundary (bit 48) and truncate the LSB when needed
// meaning we need a load mask like Z
// Once loaded, we can't just multiply to shift left, instead we have to divide to shift
// right (or multiply with inverse). The idea is to align our bits with the LSB instead
// of the MSB like other variants before shifting down to LSB. Once in LSB, we can mask out
// the extra bits instead. But before we can do so, we have to multiply again to shift by
// our base bit offset. We'll have to use it to load a constant.
// We also have to avoid denormals and so must flip a bit in the exponent before we multiply
// but we don't need to clear it as we'll discard it when masking extra bits
// The idea behind this approach isn't to be faster when unpacking XYZ, but rather to allow
// for 8-wide AVX unpacking more easily without relying on AVX2
// Being able to process 2-4 elements at a time will reduce register pressure
// xy = _mm_or_pd(xy, exponent_bit)
// xy = _mm_mul_pd(xy, shift_right_xy)
// z = _mm_srl_epi64(z, shift_right_z)
// xy = _mm_mul_pd(xy, base_bit_offset_shift_right_xy)
// z = _mm_srl_epi64(z, shift_right_base_bit_offset)
// xyz = _mm_shuffle_ps(xy, z, _MM_SHUFFLE(1, 1, 3, 1))
// xyz = _mm_and_ps(xyz, num_bit_mask)
// While this might technically work, even with SSE2 (x_, yz pairs), it is unlikely to be
// any faster because we can process the xyz lanes independently with shifts but they will
// serialize with float mul on top of the required magic to set it up

#if defined(RTM_AVX2_INTRINSICS)
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_uXX_avx2_v0(
	uint32_t num_bits,
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as SSE4 v0

	// Total size: 4*24 = 96
	struct SSEConstants_t
	{
		float max_value[24];
	};

	alignas(128) static constexpr SSEConstants_t k_packed_constants =
	{
		{
			1.0F, (1.0F / float((1 << 1) - 1)), (1.0F / float((1 << 2) - 1)), (1.0F / float((1 << 3) - 1)),
			(1.0F / float((1 << 4) - 1)), (1.0F / float((1 << 5) - 1)), (1.0F / float((1 << 6) - 1)), (1.0F / float((1 << 7) - 1)),
			(1.0F / float((1 << 8) - 1)), (1.0F / float((1 << 9) - 1)), (1.0F / float((1 << 10) - 1)), (1.0F / float((1 << 11) - 1)),
			(1.0F / float((1 << 12) - 1)), (1.0F / float((1 << 13) - 1)), (1.0F / float((1 << 14) - 1)), (1.0F / float((1 << 15) - 1)),
			(1.0F / float((1 << 16) - 1)), (1.0F / float((1 << 17) - 1)), (1.0F / float((1 << 18) - 1)), (1.0F / float((1 << 19) - 1)),
			(1.0F / float((1 << 20) - 1)), (1.0F / float((1 << 21) - 1)), (1.0F / float((1 << 22) - 1)), (1.0F / float((1 << 23) - 1)),
		}
	};

	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t base_bit_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m256i raw_bytes = _mm256_castps_si256(_mm256_broadcast_ps((const __m128*)(vector_data + byte_offset)));

	const __m256i k_byte_swap_mask = _mm256_set1_epi64x(0x0001020304050607ULL);

	// TODO: Load from memory with broadcast
	// Select and swizzle using our mask
	const __m128i swizzle_mask_z = _mm_set1_epi8(static_cast<uint8_t>((num_bits >> 2) & 0x04));	// num_bits >= 16 ? 4 : 0

	// TODO: Could perhaps reverse to zwxy to leverage _mm256_zextsi128_si256 for swizzle mask
	const __m128i zero = _mm_setzero_si128();
	const __m256i swizzle_mask = _mm256_add_epi8(_mm256_set_m128i(swizzle_mask_z, zero), k_byte_swap_mask);

	// Reverse the bytes in each 64-bit lane and line up XYZ
	__m256i xyzw_u64 = _mm256_shuffle_epi8(raw_bytes, swizzle_mask);

	// TODO: set base bit offset once, add once (or shift by bit offset, see AVX)
	// TODO: load constant, unpack 8/16/32/64 for shift offset
	// Shift out the extra bits
	const __m256i shift_offset_xyzw = _mm256_set_epi64x(0, base_bit_offset + ((num_bits % 16) * 2), base_bit_offset + num_bits, base_bit_offset);

	// Shift left to truncate the extra leading bits
	xyzw_u64 = _mm256_sllv_epi64(xyzw_u64, shift_offset_xyzw);

	// TODO: avoid constant load by getting top 128bit (3 cycles), then merge/swizzle (shift/blend 2 cycles, shuffle 1 cycle)
	// Combine and mask our the extra bits
	// As u64, we have: {x, y}, but when we cast to u32, we get: {x, _, y, _}, {z, _, z, _}
	const __m256i k_merge_shuffle_mask = _mm256_setr_epi32(1, 3, 5, 5, 0, 0, 0, 0);
	__m128i xyzw_u32 = _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(xyzw_u64, k_merge_shuffle_mask));

	// Shift right to bring them in the right place at the bottom
	const __m128i shift_num_bits = _mm_set1_epi32(32 - num_bits);
	xyzw_u32 = _mm_srlv_epi32(xyzw_u32, shift_num_bits);

	// Convert to float and re-scale
	const __m128 inv_max_value = _mm_load_ps1(&k_packed_constants.max_value[num_bits]);
	return _mm_mul_ps(_mm_cvtepi32_ps(xyzw_u32), inv_max_value);
}
#endif	// defined(RTM_AVX2_INTRINSICS)

static void bm_unpack_vector3_uXX_ref(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_ref(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_ref(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_ref(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_ref(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_ref);

#if defined(RTM_NEON_INTRINSICS)
static void bm_unpack_vector3_uXX_neon_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_neon_v0(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_neon_v0(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_neon_v0(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_neon_v0(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_neon_v0);

static void bm_unpack_vector3_uXX_neon_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_neon_v1(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_neon_v1(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_neon_v1(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_neon_v1(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_neon_v1);

static void bm_unpack_vector3_uXX_neon_v2(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_neon_v2(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_neon_v2(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_neon_v2(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_neon_v2(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_neon_v2);
#endif	// defined(RTM_NEON_INTRINSICS)

#if defined(RTM_SSE2_INTRINSICS)
static void bm_unpack_vector3_uXX_sse2_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse2_v0(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse2_v0(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse2_v0(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse2_v0(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse2_v0);

static void bm_unpack_vector3_uXX_sse2_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse2_v1(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse2_v1(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse2_v1(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse2_v1(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse2_v1);

static void bm_unpack_vector3_uXX_sse2_v2(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse2_v2(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse2_v2(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse2_v2(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse2_v2(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse2_v2);
#endif	// defined(RTM_SSE2_INTRINSICS)

#if defined(RTM_SSE3_INTRINSICS)
static void bm_unpack_vector3_uXX_sse3_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse3_v0(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse3_v0(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse3_v0(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse3_v0(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse3_v0);

static void bm_unpack_vector3_uXX_sse3_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse3_v1(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse3_v1(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse3_v1(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse3_v1(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse3_v1);

static void bm_unpack_vector3_uXX_sse3_v2(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse3_v2(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse3_v2(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse3_v2(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse3_v2(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse3_v2);
#endif	// defined(RTM_SSE3_INTRINSICS)

#if defined(RTM_SSE4_INTRINSICS)
static void bm_unpack_vector3_uXX_sse4_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse4_v0(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse4_v0(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse4_v0(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse4_v0(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse4_v0);

static void bm_unpack_vector3_uXX_sse4_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse4_v1(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse4_v1(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse4_v1(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse4_v1(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse4_v1);

static void bm_unpack_vector3_uXX_sse4_v2(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_sse4_v2(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_sse4_v2(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_sse4_v2(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_sse4_v2(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_sse4_v2);
#endif	// defined(RTM_SSE4_INTRINSICS)

#if defined(RTM_AVX2_INTRINSICS)
static void bm_unpack_vector3_uXX_avx2_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t num_bits_0 = 5;
	volatile uint32_t num_bits_1 = 6;
	volatile uint32_t num_bits_2 = 7;
	volatile uint32_t num_bits_3 = 8;

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_uXX_avx2_v0(num_bits_0, buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_uXX_avx2_v0(num_bits_1, buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_uXX_avx2_v0(num_bits_2, buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_uXX_avx2_v0(num_bits_3, buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_uXX_avx2_v0);
#endif	// defined(RTM_AVX2_INTRINSICS)
#endif // defined(ACL_IMPL_BENCHMARK_UNPACKING)
