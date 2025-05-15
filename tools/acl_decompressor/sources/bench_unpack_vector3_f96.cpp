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
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_ref(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;
	uint64_t vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 0);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint64_t x64 = vector_u64;

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 4);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint64_t y64 = vector_u64;

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 8);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint64_t z64 = vector_u64;

	const float x = acl::aligned_load<float>(&x64);
	const float y = acl::aligned_load<float>(&y64);
	const float z = acl::aligned_load<float>(&z64);

	return rtm::vector_set(x, y, z);
}

#if defined(RTM_NEON_INTRINSICS)
// Implementation from ACL 2.1
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_neon_v0(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;

	uint8x16_t x64y64_u8 = vrev64q_u8(vld1q_u8(vector_data + byte_offset + 0));
	uint64x2_t x64_tmp = vreinterpretq_u64_u8(x64y64_u8);
	uint64x2_t tmp_y64 = vreinterpretq_u64_u8(vextq_u8(x64y64_u8, x64y64_u8, 4));

	const uint64x2_t shift_offset64 = vdupq_n_u64(shift_offset);
	x64_tmp = vshlq_u64(x64_tmp, shift_offset64);
	tmp_y64 = vshlq_u64(tmp_y64, shift_offset64);
	uint32x2_t xy32 = vreinterpret_u32_u64(vsri_n_u64(vget_high_u32(tmp_y64), vget_low_u64(x64_tmp), 32));

	uint8x8_t z64_u8 = vrev64_u8(vld1_u8(vector_data + byte_offset + 8));
	uint64x1_t z64 = vreinterpret_u64_u8(z64_u8);
	z64 = vshl_u64(z64, vdup_n_u64(shift_offset));

	const uint32x4_t xyz32 = vcombine_u32(xy32, vrev64_u32(vreinterpret_u32_u64(z64)));
	return vreinterpretq_f32_u32(xyz32);
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_neon_v1(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as unpack_vector3_uXX_unsafe, see comment there
	// 32: {[0,39], [32,71], [64,103]} = {[0,1,2,3,4], [4,5,6,7,8], [8,9,10,11,12]}
	// Notice that X and Z start at their natural offset
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const uint8x16_t raw_bytes = vld1q_u8(vector_data + byte_offset);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const uint8x16_t rev_raw_bytes = vrev64q_u8(raw_bytes);

	// Select each component
	const uint8x16_t xz = rev_raw_bytes;
	const uint8x16_t y_ = vextq_u8(rev_raw_bytes, rev_raw_bytes, 12);	// [11,10,9,8,7,6,5,4], _

	// Combine our pairs
	uint64x2_t xy = vcombine_u64(vget_low_u64(vreinterpretq_u64_u8(xz)), vget_low_u64(vreinterpretq_u64_u8(y_)));
	uint64x2_t zz = vdupq_lane_u64(vget_high_u64(xz), 0);

	// Shift out the extra bits
	const int64x2_t shift_offset_s64 = vdupq_n_s64(shift_offset);
	xy = vshlq_u64(xy, shift_offset_s64);
	zz = vshlq_u64(zz, shift_offset_s64);

	// Combine our result and cast
	// As u64, we have: {x, y}, but when we cast to u32, we get: {_, x, _, y}
	const uint32x4_t xyzw_u32 = vuzpq_u32(vreinterpretq_u32_u64(xy), vreinterpretq_u32_u64(zz)).val[1];
	return vreinterpretq_f32_u32(xyzw_u32);
}
#endif	// defined(RTM_NEON_INTRINSICS)

#if defined(RTM_NEON64_INTRINSICS)
// Implementation from ACL 2.1
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_neon64_v0(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;
	uint64_t vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 0);
	vector_u64 = acl::byte_swap(vector_u64);

	const uint64_t x64 = (vector_u64 >> (32 - shift_offset)) & uint64_t(0x00000000FFFFFFFFULL);

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 4);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;

	const uint64_t y64 = vector_u64 & uint64_t(0xFFFFFFFF00000000ULL);

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 8);
	vector_u64 = acl::byte_swap(vector_u64);

	const uint64_t z64 = vector_u64 >> (32 - shift_offset);

	const uint32x2_t xy = vcreate_u32(x64 | y64);
	const uint32x2_t z = vcreate_u32(z64);
	const uint32x4_t value_u32 = vcombine_u32(xy, z);
	return vreinterpretq_f32_u32(value_u32);
}
#endif	// defined(RTM_NEON64_INTRINSICS)

#if defined(RTM_SSE2_INTRINSICS)
// Implementation from ACL 2.1
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_sse2_v0(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;
	uint64_t vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 0);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint32_t x32 = uint32_t(vector_u64);

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 4);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint32_t y32 = uint32_t(vector_u64);

	vector_u64 = acl::unaligned_load<uint64_t>(vector_data + byte_offset + 8);
	vector_u64 = acl::byte_swap(vector_u64);
	vector_u64 <<= shift_offset;
	vector_u64 >>= 32;

	const uint32_t z32 = uint32_t(vector_u64);

	return _mm_castsi128_ps(_mm_set_epi32(static_cast<int32_t>(x32), static_cast<int32_t>(z32), static_cast<int32_t>(y32), static_cast<int32_t>(x32)));
}

RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_sse2_v1(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as NEON v1
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

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

	// Select each component and combine our pairs
	// [7,6,5,4,3,2,1,0], [11,10,9,8,7,6,5,4]
	__m128i xy = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 1, 0));
	// [15,14,13,12,11,10,9,8], [15,14,13,12,11,10,9,8]
	__m128i zw = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(3, 2, 3, 2));

	// Shift out the extra bits
	const __m128i shift_offset_s64 = _mm_set1_epi64x(shift_offset);
	xy = _mm_sll_epi64(xy, shift_offset_s64);
	zw = _mm_sll_epi64(zw, shift_offset_s64);

	// Combine our result and cast
	// As u64, we have: {x, y}, but when we cast to u32, we get: {_, x, _, y}
	return _mm_shuffle_ps(_mm_castsi128_ps(xy), _mm_castsi128_ps(zw), _MM_SHUFFLE(3, 1, 3, 1));
}
#endif	// defined(RTM_SSE2_INTRINSICS)

#if defined(RTM_SSE3_INTRINSICS)
RTM_DISABLE_SECURITY_COOKIE_CHECK RTM_FORCE_NOINLINE
rtm::vector4f RTM_SIMD_CALL unpack_vector3_f96_sse3_v0(
	const uint8_t* vector_data,
	uint32_t bit_offset)
{
	// Same principle as NEON v1
	const uint32_t byte_offset = bit_offset / 8;
	const uint32_t shift_offset = bit_offset % 8;

	// Load 16 bytes
	// [0,1,2,3,4,5,6,7], [8,9,10,11,12,13,14,15]
	const __m128i raw_bytes = _mm_loadu_si128((const __m128i*)(vector_data + byte_offset));

	const __m128i k_byte_swap_mask = _mm_setr_epi32(0x04050607, 0x00010203, 0x0c0d0e0f, 0x08090a0b);

	// [7,6,5,4,3,2,1,0], [15,14,13,12,11,10,9,8]
	const __m128i rev_raw_bytes = _mm_shuffle_epi8(raw_bytes, k_byte_swap_mask);

	// Select each component and combine our pairs
	// [7,6,5,4,3,2,1,0], [11,10,9,8,7,6,5,4]
	__m128i xy = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(0, 3, 1, 0));
	// [15,14,13,12,11,10,9,8], [15,14,13,12,11,10,9,8]
	__m128i zw = _mm_shuffle_epi32(rev_raw_bytes, _MM_SHUFFLE(3, 2, 3, 2));

	// Shift out the extra bits
	const __m128i shift_offset_s64 = _mm_set1_epi64x(shift_offset);
	xy = _mm_sll_epi64(xy, shift_offset_s64);
	zw = _mm_sll_epi64(zw, shift_offset_s64);

	// Combine our result and cast
	// As u64, we have: {x, y}, but when we cast to u32, we get: {_, x, _, y}
	return _mm_shuffle_ps(_mm_castsi128_ps(xy), _mm_castsi128_ps(zw), _MM_SHUFFLE(3, 1, 3, 1));
}
#endif	// defined(RTM_SSE3_INTRINSICS)

static void bm_unpack_vector3_f96_ref(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_ref(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_ref(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_ref(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_ref(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_ref);

#if defined(RTM_NEON_INTRINSICS)
static void bm_unpack_vector3_f96_neon_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_neon_v0(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_neon_v0(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_neon_v0(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_neon_v0(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_neon_v0);

static void bm_unpack_vector3_f96_neon_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_neon_v1(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_neon_v1(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_neon_v1(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_neon_v1(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_neon_v1);
#endif	// defined(RTM_NEON_INTRINSICS)

#if defined(RTM_NEON64_INTRINSICS)
static void bm_unpack_vector3_f96_neon64_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_neon64_v0(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_neon64_v0(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_neon64_v0(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_neon64_v0(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_neon64_v0);
#endif	// defined(RTM_NEON64_INTRINSICS)

#if defined(RTM_SSE2_INTRINSICS)
static void bm_unpack_vector3_f96_sse2_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_sse2_v0(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_sse2_v0(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_sse2_v0(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_sse2_v0(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_sse2_v0);

static void bm_unpack_vector3_f96_sse2_v1(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_sse2_v1(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_sse2_v1(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_sse2_v1(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_sse2_v1(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_sse2_v1);
#endif	// defined(RTM_SSE2_INTRINSICS)

#if defined(RTM_SSE3_INTRINSICS)
static void bm_unpack_vector3_f96_sse3_v0(benchmark::State& state)
{
	uint8_t buffer[128] = { 0 };
	rtm::vector4f v0 = rtm::vector_zero();
	rtm::vector4f v1 = rtm::vector_zero();
	rtm::vector4f v2 = rtm::vector_zero();
	rtm::vector4f v3 = rtm::vector_zero();

	// Prevent compiler from specializing the call with a constant
	volatile uint32_t bit_offset_0 = 4;
	volatile uint32_t bit_offset_1 = 5;
	volatile uint32_t bit_offset_2 = 6;
	volatile uint32_t bit_offset_3 = 7;

	for (auto _ : state)
	{
		v0 = rtm::vector_add(unpack_vector3_f96_sse3_v0(buffer, bit_offset_0), v0);
		v1 = rtm::vector_add(unpack_vector3_f96_sse3_v0(buffer, bit_offset_1), v1);
		v2 = rtm::vector_add(unpack_vector3_f96_sse3_v0(buffer, bit_offset_2), v2);
		v3 = rtm::vector_add(unpack_vector3_f96_sse3_v0(buffer, bit_offset_3), v3);
	}

	benchmark::DoNotOptimize(buffer);
	benchmark::DoNotOptimize(v0);
	benchmark::DoNotOptimize(v1);
	benchmark::DoNotOptimize(v2);
	benchmark::DoNotOptimize(v3);
}

BENCHMARK(bm_unpack_vector3_f96_sse3_v0);
#endif	// defined(RTM_SSE3_INTRINSICS)
#endif // defined(ACL_IMPL_BENCHMARK_UNPACKING)
