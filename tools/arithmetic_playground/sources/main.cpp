////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/core/error.h"
#include "acl/core/track_types.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_64.h"

#include <conio.h>

#if !defined(_WINDOWS_)
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

#include <Windows.h>
#endif    // _WINDOWS_

#define ACL_DEBUG_ARITHMETIC	0
#define ACL_DEBUG_BIT_RATE		14
#define ACL_DEBUG_BONE			10

static const size_t k_num_segment_values = 18;
static const bool k_remap_fp_range = false;
static const bool k_enable_float64 = true;
static const bool k_enable_float32 = true;
static const bool k_enable_fp = true;
static const bool k_dump_error = false;

using namespace acl;

static alignas(16) const uint64_t k_raw_data[]
{
	uint64_t(0xbfc24b48b8f03ffcull), uint64_t(0xbfc1115cc7c50094ull), uint64_t(0xbfb17e488a5ce18dull), uint64_t(0x3fef4e743f849140ull),
	uint64_t(0xbfb87cd0500e28baull), uint64_t(0xbfe41384434c47e1ull), uint64_t(0xbfdddffdd08a2b72ull), uint64_t(0x3fe3b584c09ecbcbull),
	uint64_t(0xbfb2aab51c92a658ull), uint64_t(0xbfe38b5d63c9e14full), uint64_t(0xbfdebee08ca8d7c1ull), uint64_t(0x3fe40197cda90f2full),
	uint64_t(0xbfb18798cbb86977ull), uint64_t(0xbfe40695426eb0cbull), uint64_t(0xbfdf9f7a4bf887deull), uint64_t(0x3fe3300abc0412d9ull),
	uint64_t(0xbfb05a8c8b3c0ef2ull), uint64_t(0xbfe480f2de74f678ull), uint64_t(0xbfe03eab0ab67b39ull), uint64_t(0x3fe2513eb6d13b6cull),
	uint64_t(0xbfad89d7b00e94feull), uint64_t(0xbfe5167a60976c69ull), uint64_t(0xbfe0c8cbd0402af2ull), uint64_t(0x3fe126d71003c343ull),
	uint64_t(0x3fa99d06ce84d3ddull), uint64_t(0x3fe5c0826c352e87ull), uint64_t(0x3fe165e4cf0264c2ull), uint64_t(0xbfdf57d3a4edc973ull),
	uint64_t(0x3fa5cbf7878e6354ull), uint64_t(0x3fe6509117a915a7ull), uint64_t(0x3fe1eeaba46d6dc2ull), uint64_t(0xbfdc783a31c67c9dull),
	uint64_t(0x3fa4458c6c6366dfull), uint64_t(0x3fe687dd61f28f98ull), uint64_t(0x3fe221f425202371ull), uint64_t(0xbfdb462ad38ddf25ull),
	uint64_t(0x3fa498d95d133f1bull), uint64_t(0x3fe67bde0a9f60c0ull), uint64_t(0x3fe2171e64f3f001ull), uint64_t(0xbfdb8940906b5db5ull),
	uint64_t(0x3fa78037733da5ccull), uint64_t(0x3fe615326cd53578ull), uint64_t(0x3fe1b42e8c9e71deull), uint64_t(0xbfddb768d1af62acull),
	uint64_t(0xbfaba1ed36bf0fbeull), uint64_t(0xbfe56c90e72352b5ull), uint64_t(0xbfe1179c024c337full), uint64_t(0x3fe06d968e313519ull),
	uint64_t(0xbfb047ca1d409b98ull), uint64_t(0xbfe4883a0938caf1ull), uint64_t(0xbfe0454f4d98fb0full), uint64_t(0x3fe2437045604903ull),
	uint64_t(0xbfb35c98365b8d7full), uint64_t(0xbfe33c653e6ba63aull), uint64_t(0xbfde2e95775ecb8full), uint64_t(0x3fe480d66db3501bull),
	uint64_t(0xbfb53a6562d02b8bull), uint64_t(0xbfe256ecad62d6b5ull), uint64_t(0xbfdc93094054b0c3ull), uint64_t(0x3fe5d42bec0e70b8ull),
	uint64_t(0xbfb5d110917813fcull), uint64_t(0xbfe204ee7f4d2c4aull), uint64_t(0xbfdc0414e370a3b3ull), uint64_t(0x3fe6435ae0f99b5aull),
	uint64_t(0xbfb5b083251f181aull), uint64_t(0xbfe2157a82b67cfbull), uint64_t(0xbfdc21e30e299bc3ull), uint64_t(0x3fe62cfebfeec65cull),
	uint64_t(0xbfb51ca788fb3792ull), uint64_t(0xbfe2612cfd61d990ull), uint64_t(0xbfdca915e42b62d1ull), uint64_t(0x3fe5c4c09897d31aull),
	uint64_t(0xbfb434a50031fe52ull), uint64_t(0xbfe2d305221a02b1ull), uint64_t(0xbfdd754555e30ecbull), uint64_t(0x3fe5205748865acfull),
	uint64_t(0xbfb362d051599372ull), uint64_t(0xbfe3378376d2ade1ull), uint64_t(0xbfde286fb937bf66ull), uint64_t(0x3fe48794b072423cull),
	uint64_t(0xbfb2b774a51ccf01ull), uint64_t(0xbfe384f4c990a913ull), uint64_t(0xbfdeb407e05dda79ull), uint64_t(0x3fe40bd15114f4b1ull),
	uint64_t(0xbfb21fdfa2d82abaull), uint64_t(0xbfe3c7632e70704bull), uint64_t(0xbfdf2be279a5f233ull), uint64_t(0x3fe39d72f2f895a7ull),
	uint64_t(0xbfb19560a9dc3668ull), uint64_t(0xbfe403dcf260e19full), uint64_t(0xbfdf97b5c0d06ce9ull), uint64_t(0x3fe335e0df6dd279ull),
	uint64_t(0xbfb06e377a4daaddull), uint64_t(0xbfe47bc14428b3b1ull), uint64_t(0xbfe0387ac3f6a5aeull), uint64_t(0x3fe25c408b8f750aull),
	uint64_t(0xbfac310976a46a76ull), uint64_t(0xbfe5543d70f1c37dull), uint64_t(0xbfe100ee64cbd286ull), uint64_t(0x3fe0a37afc3c46c1ull),
	uint64_t(0x3fa768bb6883ea0eull), uint64_t(0x3fe6198ac7efdba4ull), uint64_t(0x3fe1b7b34ce8dd11ull), uint64_t(0xbfdda25c70b8220dull),
	uint64_t(0x3fa471df3dd37bfeull), uint64_t(0x3fe68268ee6d7513ull), uint64_t(0x3fe21c6bc45b03d5ull), uint64_t(0xbfdb664c16d47072ull),
	uint64_t(0x3fa34f65bf0e40d9ull), uint64_t(0x3fe6a8ad6e48cee9ull), uint64_t(0x3fe240f2dfd93c0cull), uint64_t(0xbfda86e7a8f45a4eull),
	uint64_t(0x3fa4b8b55d5a2e21ull), uint64_t(0x3fe6786431a42106ull), uint64_t(0x3fe2132d84f59f61ull), uint64_t(0xbfdb9e8c37cf87c3ull),
	uint64_t(0x3fa7b607865cdc5dull), uint64_t(0x3fe60b616bd31083ull), uint64_t(0x3fe1ac1d0bc574f7ull), uint64_t(0xbfdde6f4eaf8679cull),
	uint64_t(0xbfabf2271cb2290dull), uint64_t(0xbfe56250f52f9da2ull), uint64_t(0xbfe10c5a713da86bull), uint64_t(0x3fe0860995a86c84ull),
	uint64_t(0xbfb0f6a26d09cbcaull), uint64_t(0xbfe43eef89ae8402ull), uint64_t(0xbfe005057733354aull), uint64_t(0x3fe2c9ad8cf86862ull),
	uint64_t(0xbfb412d9f2b4e5d2ull), uint64_t(0xbfe2e4a0a7410ddcull), uint64_t(0xbfdd93c7a4b3c3b5ull), uint64_t(0x3fe5066864d3b8ceull),
	uint64_t(0xbfb5e2b109222a72ull), uint64_t(0xbfe2017ca1bc1b11ull), uint64_t(0xbfdbf9ec7183bc37ull), uint64_t(0x3fe6490ff67d7bb7ull),
	uint64_t(0xbfb6ef1d6eb7331bull), uint64_t(0xbfe1678abbffd533ull), uint64_t(0xbfdaee1648be59a8ull), uint64_t(0x3fe70e3aa13aa23dull),
};

static alignas(16) const uint64_t k_clip_range[]
{
	uint64_t(0xbfc24b48b8f03ffcull), uint64_t(0xbfe6a8ad6e48cee9ull), uint64_t(0xbfe240f2dfd93c0cull), uint64_t(0x3fda86e7a8f45a4eull),
	uint64_t(0xbfa34f65bf0e40d9ull), uint64_t(0xbfc1115cc7c50094ull), uint64_t(0xbfb17e488a5ce18dull), uint64_t(0x3fef4e743f849140ull),
};

static alignas(16) const uint64_t k_segment_range[]
{
	uint64_t(0x0000000000000000ull), uint64_t(0x3f70101020000000ull), uint64_t(0x3f70101020000000ull), uint64_t(0x3f90101020000000ull),
	uint64_t(0x3fef7f7f80000000ull), uint64_t(0x3ff0000000000000ull), uint64_t(0x3ff0000000000000ull), uint64_t(0x3ff0000000000000ull),
};

static const Vector4_64* k_values_64 = reinterpret_cast<const Vector4_64*>(&k_raw_data[0]);
static const size_t k_num_values = sizeof(k_raw_data) / sizeof(Vector4_64);

struct Vector4_FP
{
	uint64_t x;
	uint64_t y;
	uint64_t z;
	uint64_t w;
};

static uint64_t scalar_to_fp(double input, uint8_t num_bits, bool is_unsigned)
{
	// Input is signed, fp is unsigned
	if (!is_unsigned)
		input = (input * 0.5) + 0.5;

	// Input values are in the range [0 .. 1] but fractional fixed point data types
	// can only perform arithmetic on values constructed from powers of two.
	// As such, our values are in the range [0 .. 1[
	// To handle this, we remap our input to the new range: remapped = input * ((1 << num_bits) - 1) / (1 << num_bits)
	// The scale factor ((1 << num_bits) - 1) / (1 << num_bits) is smaller than 1.0
	double scale = double((uint64_t(1ull) << num_bits) - 1) / double(uint64_t(1ull) << num_bits);
	if (k_remap_fp_range)
		input = input * scale;
	return (uint64_t)symmetric_round(input * double(uint64_t(1ull) << num_bits)) & ((uint64_t(1ull) << num_bits) - 1);
}

static double scalar_from_fp_64(uint64_t input, uint8_t num_bits, bool is_unsigned)
{
	uint64_t max_value = uint64_t(1ull) << num_bits;
	double value = safe_to_double(input) / safe_to_double(max_value);
	// See comment above as to why we remap the range
	// The scale factor (1 << num_bits) / ((1 << num_bits) - 1) is larger than 1.0
	double scale = double(uint64_t(1ull) << num_bits) / double((uint64_t(1ull) << num_bits) - 1);
	if (k_remap_fp_range)
		value *= scale;
	if (!is_unsigned)
		value = (value * 2.0) - 1.0;
	return value;
}

static Vector4_FP vector_to_fp(const Vector4_64& input, uint8_t num_bits, bool is_unsigned)
{
	return Vector4_FP{ scalar_to_fp(vector_get_x(input), num_bits, is_unsigned), scalar_to_fp(vector_get_y(input), num_bits, is_unsigned), scalar_to_fp(vector_get_z(input), num_bits, is_unsigned), scalar_to_fp(vector_get_w(input), num_bits, is_unsigned) };
}

static Vector4_64 vector_from_fp_64(const Vector4_FP& input, uint8_t num_bits, bool is_unsigned)
{
	return vector_set(scalar_from_fp_64(input.x, num_bits, is_unsigned), scalar_from_fp_64(input.y, num_bits, is_unsigned), scalar_from_fp_64(input.z, num_bits, is_unsigned), scalar_from_fp_64(input.w, num_bits, is_unsigned));
}

static Vector4_FP vector_min(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x < rhs.x ? lhs.x : rhs.x, lhs.y < rhs.y ? lhs.y : rhs.y, lhs.z < rhs.z ? lhs.z : rhs.z, lhs.w < rhs.w ? lhs.w : rhs.w };
}

static Vector4_FP vector_max(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x > rhs.x ? lhs.x : rhs.x, lhs.y > rhs.y ? lhs.y : rhs.y, lhs.z > rhs.z ? lhs.z : rhs.z, lhs.w > rhs.w ? lhs.w : rhs.w };
}

static Vector4_FP vector_sub(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w };
}

static Vector4_FP vector_set(uint64_t xyzw)
{
	return Vector4_FP{ xyzw, xyzw, xyzw, xyzw };
}

static Vector4_FP vector_zero_fp()
{
	return Vector4_FP{ 0, 0, 0, 0 };
}

static Vector4_FP vector_equal_mask(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x == rhs.x ? ~uint64_t(0ull) : 0, lhs.y == rhs.y ? ~uint64_t(0ull) : 0, lhs.z == rhs.z ? ~uint64_t(0ull) : 0, lhs.w == rhs.w ? ~uint64_t(0ull) : 0 };
}

static Vector4_FP vector_shift_left(const Vector4_FP& input, uint8_t shift)
{
	return Vector4_FP{ input.x << shift, input.y << shift, input.z << shift, input.w << shift };
}

static Vector4_FP vector_shift_right(const Vector4_FP& input, uint8_t shift)
{
	return Vector4_FP{ input.x >> shift, input.y >> shift, input.z >> shift, input.w >> shift };
}

static Vector4_FP vector_and(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x & rhs.x, lhs.y & rhs.y, lhs.z & rhs.z, lhs.w & rhs.w };
}

static Vector4_FP vector_add(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w };
}

static Vector4_FP vector_div(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z, lhs.w / rhs.w };
}

static Vector4_FP vector_mul(const Vector4_FP& lhs, const Vector4_FP& rhs)
{
	return Vector4_FP{ lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z, lhs.w * rhs.w };
}

static Vector4_FP vector_blend(const Vector4_FP& mask, const Vector4_FP& if_true, const Vector4_FP& if_false)
{
	return Vector4_FP{ mask.x == 0 ? if_false.x : if_true.x, mask.y == 0 ? if_false.y : if_true.y, mask.z == 0 ? if_false.z : if_true.z, mask.w == 0 ? if_false.w : if_true.w };
}

static Vector4_FP vector_convert(const Vector4_FP& input, uint8_t from_bits, uint8_t to_bits)
{
	if (from_bits > to_bits)
	{
		// Truncating our value with rounding
		uint8_t num_truncated_bits = from_bits - to_bits;
		Vector4_FP bias = vector_set(uint64_t(1) << (num_truncated_bits - 1));
		return vector_shift_right(vector_add(input, bias), num_truncated_bits);
	}
	else if (from_bits < to_bits)
	{
		// Expanding up by scaling our value
		//Vector4_FP dest_base = vector_set((uint64_t(1) << to_bits) - 1);
		//Vector4_FP src_base = vector_set((uint64_t(1) << from_bits) - 1);
		//return vector_div(vector_mul(input, dest_base), src_base);
		return vector_shift_left(input, to_bits - from_bits);
	}
	else
		return input;	// No change
}

static void calculate_range_64(const Vector4_64* values, size_t num_values, Vector4_64& out_min, Vector4_64& out_max)
{
	Vector4_64 min = values[0];
	Vector4_64 max = min;

	for (size_t i = 1; i < num_values; ++i)
	{
		const Vector4_64& value = values[i];
		min = vector_min(min, value);
		max = vector_max(max, value);
	}

	out_min = min;
	out_max = max;
}

static void calculate_range_32(const Vector4_32* values, size_t num_values, Vector4_32& out_min, Vector4_32& out_max)
{
	Vector4_32 min = values[0];
	Vector4_32 max = min;

	for (size_t i = 1; i < num_values; ++i)
	{
		const Vector4_32& value = values[i];
		min = vector_min(min, value);
		max = vector_max(max, value);
	}

	out_min = min;
	out_max = max;
}

static void calculate_range_fp(const Vector4_FP* values, size_t num_values, Vector4_FP& out_min, Vector4_FP& out_max)
{
	Vector4_FP min = values[0];
	Vector4_FP max = min;

	for (size_t i = 1; i < num_values; ++i)
	{
		const Vector4_FP& value = values[i];
		min = vector_min(min, value);
		max = vector_max(max, value);
	}

	out_min = min;
	out_max = max;
}

static void normalize_64(const Vector4_64* values, size_t num_values, const Vector4_64& range_min, const Vector4_64& range_max, Vector4_64* out_normalized_values)
{
	const Vector4_64 range_extent = vector_sub(range_max, range_min);
	const Vector4_64 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001));

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& value = values[i];
		Vector4_64 normalized_value = vector_div(vector_sub(value, range_min), range_extent);
		normalized_value = vector_blend(is_range_zero_mask, vector_zero_64(), normalized_value);
		out_normalized_values[i] = normalized_value;
	}
}

static void normalize_32(const Vector4_32* values, size_t num_values, const Vector4_32& range_min, const Vector4_32& range_max, Vector4_32* out_normalized_values)
{
	const Vector4_32 range_extent = vector_sub(range_max, range_min);
	const Vector4_32 is_range_zero_mask = vector_less_than(range_extent, vector_set(0.000000001f));

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_32& value = values[i];
		Vector4_32 normalized_value = vector_div(vector_sub(value, range_min), range_extent);
		normalized_value = vector_blend(is_range_zero_mask, vector_zero_32(), normalized_value);
		out_normalized_values[i] = normalized_value;
	}
}

static void normalize_clip_fp(const Vector4_FP* values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_normalized_values)
{
	// Range: 0.32
	// Values: 0.32
	// Output: 0.32
	const Vector4_FP range_extent = vector_sub(range_max, range_min);
	//const Vector4_FP is_range_zero_mask = vector_equal_mask(range_extent, vector_zero_fp());

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& value = values[i];
		Vector4_FP offset_shifted = vector_shift_left(vector_sub(value, range_min), 32);
		//Vector4_FP normalized_value = vector_div(offset_shifted, range_extent);
		//normalized_value = vector_blend(is_range_zero_mask, vector_zero_fp(), normalized_value);
		Vector4_FP normalized_value;
		normalized_value.x = range_extent.x != 0 ? (offset_shifted.x / range_extent.x) : 0;
		normalized_value.y = range_extent.y != 0 ? (offset_shifted.y / range_extent.y) : 0;
		normalized_value.z = range_extent.z != 0 ? (offset_shifted.z / range_extent.z) : 0;
		normalized_value.w = range_extent.w != 0 ? (offset_shifted.w / range_extent.w) : 0;
		out_normalized_values[i] = normalized_value;
	}
}

static void normalize_segment_fp(const Vector4_FP* values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_normalized_values)
{
	const Vector4_FP range_extent = vector_sub(range_max, range_min);
	//const Vector4_FP is_range_zero_mask = vector_equal_mask(range_extent, vector_zero_fp());

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& value = values[i];
		Vector4_FP offset = vector_sub(value, range_min);
		//Vector4_FP normalized_value = vector_div(offset, range_extent);
		//normalized_value = vector_blend(is_range_zero_mask, vector_zero_fp(), normalized_value);
		Vector4_FP normalized_value;
		normalized_value.x = range_extent.x != 0 ? (offset.x / range_extent.x) : 0;
		normalized_value.y = range_extent.y != 0 ? (offset.y / range_extent.y) : 0;
		normalized_value.z = range_extent.z != 0 ? (offset.z / range_extent.z) : 0;
		normalized_value.w = range_extent.w != 0 ? (offset.w / range_extent.w) : 0;
		out_normalized_values[i] = normalized_value;
	}
}

static size_t pack_scalar_unsigned_64(double input, size_t num_bits)
{
	ACL_ENSURE(input >= 0.0 && input <= 1.0, "Invalue input value: 0.0 <= %f <= 1.0", input);
	size_t max_value = (1 << num_bits) - 1;
	return static_cast<size_t>(symmetric_round(input * safe_to_double(max_value)));
}

static void unpack_scalar_unsigned_64(size_t input, size_t num_bits, double& out_result)
{
	size_t max_value = (1 << num_bits) - 1;
	ACL_ENSURE(input <= max_value, "Invalue input value: %ull <= 1.0", input);
	out_result = safe_to_double(input) / safe_to_double(max_value);
}

static void pack_vector4_32(const Vector4_64& vector, uint8_t* out_vector_data)
{
	size_t vector_x = pack_scalar_unsigned_64(vector_get_x(vector), 8);
	size_t vector_y = pack_scalar_unsigned_64(vector_get_y(vector), 8);
	size_t vector_z = pack_scalar_unsigned_64(vector_get_z(vector), 8);
	size_t vector_w = pack_scalar_unsigned_64(vector_get_w(vector), 8);

	out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
	out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
	out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	out_vector_data[3] = safe_static_cast<uint8_t>(vector_w);
}

static void unpack_vector4_32(const uint8_t* vector_data, Vector4_64& out_result)
{
	uint8_t x8 = vector_data[0];
	uint8_t y8 = vector_data[1];
	uint8_t z8 = vector_data[2];
	uint8_t w8 = vector_data[3];
	double x, y, z, w;
	unpack_scalar_unsigned_64(x8, 8, x);
	unpack_scalar_unsigned_64(y8, 8, y);
	unpack_scalar_unsigned_64(z8, 8, z);
	unpack_scalar_unsigned_64(w8, 8, w);
	out_result = vector_set(x, y, z, w);
}

static void fixup_range_64(Vector4_64& range_min, Vector4_64& range_max)
{
	double padding_dbl; unpack_scalar_unsigned_64(1, 8, padding_dbl);
	const Vector4_64 padding = vector_set(padding_dbl);
	const Vector4_64 one = vector_set(1.0);
	const Vector4_64 zero = vector_zero_64();

	Vector4_64 clamped_range_min = vector_max(vector_sub(range_min, padding), zero);
	Vector4_64 clamped_range_max = vector_min(vector_add(range_max, padding), one);

	alignas(8) uint8_t buffer[8];
	pack_vector4_32(clamped_range_min, &buffer[0]);
	unpack_vector4_32(&buffer[0], clamped_range_min);
	pack_vector4_32(clamped_range_max, &buffer[0]);
	unpack_vector4_32(&buffer[0], clamped_range_max);

	range_min = clamped_range_min;
	range_max = clamped_range_max;
}

static size_t pack_scalar_unsigned_32(float input, size_t num_bits)
{
	ACL_ENSURE(input >= 0.0f && input <= 1.0f, "Invalue input value: 0.0 <= %f <= 1.0", input);
	size_t max_value = (1 << num_bits) - 1;
	return static_cast<size_t>(symmetric_round(input * safe_to_float(max_value)));
}

static void unpack_scalar_unsigned_32(size_t input, size_t num_bits, float& out_result)
{
	size_t max_value = (1 << num_bits) - 1;
	ACL_ENSURE(input <= max_value, "Invalue input value: %ull <= 1.0", input);
	out_result = safe_to_float(input) / safe_to_float(max_value);
}

static void pack_vector4_32(const Vector4_32& vector, uint8_t* out_vector_data)
{
	size_t vector_x = pack_scalar_unsigned_32(vector_get_x(vector), 8);
	size_t vector_y = pack_scalar_unsigned_32(vector_get_y(vector), 8);
	size_t vector_z = pack_scalar_unsigned_32(vector_get_z(vector), 8);
	size_t vector_w = pack_scalar_unsigned_32(vector_get_w(vector), 8);

	out_vector_data[0] = safe_static_cast<uint8_t>(vector_x);
	out_vector_data[1] = safe_static_cast<uint8_t>(vector_y);
	out_vector_data[2] = safe_static_cast<uint8_t>(vector_z);
	out_vector_data[3] = safe_static_cast<uint8_t>(vector_w);
}

static void unpack_vector4_32(const uint8_t* vector_data, Vector4_32& out_result)
{
	uint8_t x8 = vector_data[0];
	uint8_t y8 = vector_data[1];
	uint8_t z8 = vector_data[2];
	uint8_t w8 = vector_data[3];
	float x, y, z, w;
	unpack_scalar_unsigned_32(x8, 8, x);
	unpack_scalar_unsigned_32(y8, 8, y);
	unpack_scalar_unsigned_32(z8, 8, z);
	unpack_scalar_unsigned_32(w8, 8, w);
	out_result = vector_set(x, y, z, w);
}

static void fixup_range_32(Vector4_32& range_min, Vector4_32& range_max)
{
	float padding_flt; unpack_scalar_unsigned_32(1, 8, padding_flt);
	const Vector4_32 padding = vector_set(padding_flt);
	const Vector4_32 one = vector_set(1.0f);
	const Vector4_32 zero = vector_zero_32();

	Vector4_32 clamped_range_min = vector_max(vector_sub(range_min, padding), zero);
	Vector4_32 clamped_range_max = vector_min(vector_add(range_max, padding), one);

	alignas(8) uint8_t buffer[8];
	pack_vector4_32(clamped_range_min, &buffer[0]);
	unpack_vector4_32(&buffer[0], clamped_range_min);
	pack_vector4_32(clamped_range_max, &buffer[0]);
	unpack_vector4_32(&buffer[0], clamped_range_max);

	range_min = clamped_range_min;
	range_max = clamped_range_max;
}

static void fixup_range_fp(Vector4_FP& range_min, Vector4_FP& range_max)
{
	// Input range: 0.32
	// Output range: 0.8
	Vector4_FP clamped_range_min = vector_shift_right(range_min, 24);
	Vector4_FP clamped_range_max = vector_min(vector_shift_right(vector_add(range_max, vector_set(uint64_t(0x80))), 24), vector_set(uint64_t(0xFF)));

	// Range format is now 8 bits
	range_min = clamped_range_min;
	range_max = clamped_range_max;
}

static void pack_vector3_n(const Vector4_64& vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, uint8_t* out_vector_data)
{
	size_t vector_x = pack_scalar_unsigned_64(vector_get_x(vector), XBits);
	size_t vector_y = pack_scalar_unsigned_64(vector_get_y(vector), YBits);
	size_t vector_z = pack_scalar_unsigned_64(vector_get_z(vector), ZBits);

	uint64_t vector_u64 = safe_static_cast<uint64_t>((vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z);

	// Unaligned write
	uint64_t* data = reinterpret_cast<uint64_t*>(out_vector_data);
	*data = vector_u64;
}

static void pack_vector3_n(const Vector4_32& vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, uint8_t* out_vector_data)
{
	size_t vector_x = pack_scalar_unsigned_32(vector_get_x(vector), XBits);
	size_t vector_y = pack_scalar_unsigned_32(vector_get_y(vector), YBits);
	size_t vector_z = pack_scalar_unsigned_32(vector_get_z(vector), ZBits);

	uint64_t vector_u64 = safe_static_cast<uint64_t>((vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z);

	// Unaligned write
	uint64_t* data = reinterpret_cast<uint64_t*>(out_vector_data);
	*data = vector_u64;
}

static void quantize_64(const Vector4_64* normalized_values, size_t num_values, uint8_t bit_rate, Vector4_32* out_quantized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& normalized_value = normalized_values[i];
		uint8_t* quantized_value = reinterpret_cast<uint8_t*>(&out_quantized_values[i]);
		pack_vector3_n(normalized_value, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, quantized_value);
	}
}

static void quantize_32(const Vector4_32* normalized_values, size_t num_values, uint8_t bit_rate, Vector4_32* out_quantized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_32& normalized_value = normalized_values[i];
		uint8_t* quantized_value = reinterpret_cast<uint8_t*>(&out_quantized_values[i]);
		pack_vector3_n(normalized_value, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, quantized_value);
	}
}

static void quantize_fp(const Vector4_FP* normalized_values, size_t num_values, uint8_t bit_rate, bool use_segment_range_reduction, Vector4_32* out_quantized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	// 0.75 = 191.25 = 0xBF = b1011 1111 (8 bits)
	// 0.75 = 11.25 = 0xB = b1011 (4 bits)
	// 0.8 = 12.00 = 0xC = b1100
	for (size_t i = 0; i < num_values; ++i)
	{
		Vector4_FP normalized_value = normalized_values[i];
		uint8_t src_bit_rate = use_segment_range_reduction ? 24 : 32;
		Vector4_FP quantized_value = vector_convert(normalized_value, src_bit_rate, num_bits_at_bit_rate);
		uint32_t x = uint32_t(quantized_value.x);
		uint32_t y = uint32_t(quantized_value.y);
		uint32_t z = uint32_t(quantized_value.z);
		uint32_t w = uint32_t(quantized_value.w);

		uint8_t* out_quantized_value = reinterpret_cast<uint8_t*>(&out_quantized_values[i]);
		memcpy(out_quantized_value + 0 * sizeof(uint32_t), &x, sizeof(uint32_t));
		memcpy(out_quantized_value + 1 * sizeof(uint32_t), &y, sizeof(uint32_t));
		memcpy(out_quantized_value + 2 * sizeof(uint32_t), &z, sizeof(uint32_t));
		memcpy(out_quantized_value + 3 * sizeof(uint32_t), &w, sizeof(uint32_t));
	}
}

static Vector4_64 unpack_vector3_n_64(uint8_t XBits, uint8_t YBits, uint8_t ZBits, const uint8_t* vector_data)
{
	uint64_t vector_u64 = *safe_ptr_cast<const uint64_t>(vector_data);
	uint64_t x64 = vector_u64 >> (YBits + ZBits);
	uint64_t y64 = (vector_u64 >> ZBits) & ((1 << YBits) - 1);
	uint64_t z64 = vector_u64 & ((1 << ZBits) - 1);
	double x, y, z;
	unpack_scalar_unsigned_64(x64, XBits, x);
	unpack_scalar_unsigned_64(y64, YBits, y);
	unpack_scalar_unsigned_64(z64, ZBits, z);
	return vector_set(x, y, z);
}

static Vector4_32 unpack_vector3_n_32(uint8_t XBits, uint8_t YBits, uint8_t ZBits, const uint8_t* vector_data)
{
	uint64_t vector_u64 = *safe_ptr_cast<const uint64_t>(vector_data);
	uint64_t x64 = vector_u64 >> (YBits + ZBits);
	uint64_t y64 = (vector_u64 >> ZBits) & ((1 << YBits) - 1);
	uint64_t z64 = vector_u64 & ((1 << ZBits) - 1);
	float x, y, z;
	unpack_scalar_unsigned_32(x64, XBits, x);
	unpack_scalar_unsigned_32(y64, YBits, y);
	unpack_scalar_unsigned_32(z64, ZBits, z);
	return vector_set(x, y, z);
}

static void dequantize_64(const Vector4_32* quantized_values, size_t num_values, uint8_t bit_rate, Vector4_64* out_normalized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	for (size_t i = 0; i < num_values; ++i)
	{
		const uint8_t* quantized_value = reinterpret_cast<const uint8_t*>(&quantized_values[i]);
		out_normalized_values[i] = unpack_vector3_n_64(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, quantized_value);
	}
}

static void dequantize_32(const Vector4_32* quantized_values, size_t num_values, uint8_t bit_rate, Vector4_32* out_normalized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	for (size_t i = 0; i < num_values; ++i)
	{
		const uint8_t* quantized_value = reinterpret_cast<const uint8_t*>(&quantized_values[i]);
		out_normalized_values[i] = unpack_vector3_n_32(num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, quantized_value);
	}
}

static void dequantize_fp(const Vector4_32* quantized_values, size_t num_values, uint8_t bit_rate, bool use_segment_range_reduction, Vector4_FP* out_normalized_values)
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);

	for (size_t i = 0; i < num_values; ++i)
	{
		const uint8_t* quantized_value = reinterpret_cast<const uint8_t*>(&quantized_values[i]);
		uint32_t x, y, z, w;
		memcpy(&x, quantized_value + 0 * sizeof(uint32_t), sizeof(uint32_t));
		memcpy(&y, quantized_value + 1 * sizeof(uint32_t), sizeof(uint32_t));
		memcpy(&z, quantized_value + 2 * sizeof(uint32_t), sizeof(uint32_t));
		memcpy(&w, quantized_value + 3 * sizeof(uint32_t), sizeof(uint32_t));

		Vector4_FP tmp = { x, y, z, w };
		uint8_t target_bit_rate = use_segment_range_reduction ? 24 : 32;
		tmp = vector_convert(tmp, num_bits_at_bit_rate, target_bit_rate);

		out_normalized_values[i] = tmp;
	}
}

static void denormalize_64(const Vector4_64* normalized_values, size_t num_values, const Vector4_64& range_min, const Vector4_64& range_max, Vector4_64* out_values)
{
	const Vector4_64 range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& normalized_value = normalized_values[i];
		out_values[i] = vector_mul_add(normalized_value, range_extent, range_min);
	}
}

static void denormalize_32(const Vector4_32* normalized_values, size_t num_values, const Vector4_32& range_min, const Vector4_32& range_max, Vector4_32* out_values)
{
	const Vector4_32 range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_32& normalized_value = normalized_values[i];
		out_values[i] = vector_mul_add(normalized_value, range_extent, range_min);
	}
}

static void denormalize_clip_fp(const Vector4_FP* normalized_values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_values)
{
	const Vector4_FP range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& normalized_value = normalized_values[i];
		Vector4_FP result = vector_mul(normalized_value, range_extent);
		result = vector_shift_right(result, 32);	// Truncate
		result = vector_add(result, range_min);
		out_values[i] = result;
	}
}

static void denormalize_segment_fp(const Vector4_FP* normalized_values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_values)
{
	const Vector4_FP range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& normalized_value = normalized_values[i];
		Vector4_FP result = vector_mul(normalized_value, range_extent);
		result = vector_add(result, range_min);
		out_values[i] = result;
	}
}

static void print_error_64(const Vector4_64* raw_values, size_t num_values, const Vector4_64* lossy_values, uint8_t bit_rate, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
	if (k_dump_error)
		printf("Bit rate: %u (%u, %u, %u)\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate);
	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& raw_value = raw_values[i];
		const Vector4_64& lossy_value = lossy_values[i];
		Vector4_64 delta = vector_abs(vector_sub(raw_value, lossy_value));
		if (k_dump_error)
			printf("%2zu: { %.6f, %.6f, %.6f }\n", i, vector_get_x(delta), vector_get_y(delta), vector_get_z(delta));
		out_errors[bit_rate][i] = delta;
	}
}

static void print_error_32(const Vector4_64* raw_values, size_t num_values, const Vector4_32* lossy_values, uint8_t bit_rate, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
	if (k_dump_error)
		printf("Bit rate: %u (%u, %u, %u)\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate);
	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& raw_value = raw_values[i];
		Vector4_64 lossy_value = vector_cast(lossy_values[i]);
		Vector4_64 delta = vector_abs(vector_sub(raw_value, lossy_value));
		if (k_dump_error)
			printf("%2zu: { %.6f, %.6f, %.6f }\n", i, vector_get_x(delta), vector_get_y(delta), vector_get_z(delta));
		out_errors[bit_rate][i] = delta;
	}
}

static void print_error_fp(const Vector4_64* raw_values, size_t num_values, const Vector4_FP* lossy_values, uint8_t bit_rate, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
	if (k_dump_error)
		printf("Bit rate: %u (%u, %u, %u)\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate);
	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& raw_value = raw_values[i];
		Vector4_64 lossy_value = vector_from_fp_64(lossy_values[i], 32, false);
		Vector4_64 delta = vector_abs(vector_sub(raw_value, lossy_value));
		if (k_dump_error)
			printf("%2zu: { %.6f, %.6f, %.6f }\n", i, vector_get_x(delta), vector_get_y(delta), vector_get_z(delta));
		out_errors[bit_rate][i] = delta;
	}
}

static void measure_error_64(bool use_segment_range_reduction, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	if (k_dump_error)
		printf("Error for arithmetic: float64\n");
	if (k_dump_error && use_segment_range_reduction)
		printf("With segment range reduction\n");

	Vector4_64 clip_min_64;
	Vector4_64 clip_max_64;
	calculate_range_64(k_values_64, k_num_values, clip_min_64, clip_max_64);

	Vector4_64 clip_normalized_values_64[k_num_segment_values];
	normalize_64(k_values_64, k_num_segment_values, clip_min_64, clip_max_64, clip_normalized_values_64);

#if ACL_DEBUG_ARITHMETIC
	printf("Clip range min: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_min_64), vector_get_y(clip_min_64), vector_get_z(clip_min_64));
	printf("Clip range max: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_max_64), vector_get_y(clip_max_64), vector_get_z(clip_max_64));
	Vector4_FP clip_min_fp = vector_to_fp(clip_min_64, 32, false);
	Vector4_FP clip_max_fp = vector_to_fp(clip_max_64, 32, false);
	printf("Clip range min: { %16I64X, %16I64X, %16I64X }\n", clip_min_fp.x, clip_min_fp.y, clip_min_fp.z);
	printf("Clip range max: { %16I64X, %16I64X, %16I64X }\n", clip_max_fp.x, clip_max_fp.y, clip_max_fp.z);

	printf("Clip value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(k_values_64[ACL_DEBUG_BONE]), vector_get_y(k_values_64[ACL_DEBUG_BONE]), vector_get_z(k_values_64[ACL_DEBUG_BONE]));
	Vector4_FP clip_value0_fp = vector_to_fp(k_values_64[ACL_DEBUG_BONE], 32, false);
	printf("Clip value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_value0_fp.x, clip_value0_fp.y, clip_value0_fp.z);

	printf("Clip normalized value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(clip_normalized_values_64[ACL_DEBUG_BONE]), vector_get_y(clip_normalized_values_64[ACL_DEBUG_BONE]), vector_get_z(clip_normalized_values_64[ACL_DEBUG_BONE]));
	Vector4_FP clip_normalized_value0_fp = vector_to_fp(clip_normalized_values_64[ACL_DEBUG_BONE], 32, true);
	printf("Clip normalized value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_normalized_value0_fp.x, clip_normalized_value0_fp.y, clip_normalized_value0_fp.z);
#endif

	Vector4_64 segment_min_64;
	Vector4_64 segment_max_64;
	Vector4_64 segment_normalized_values_64[k_num_segment_values];
	if (use_segment_range_reduction)
	{
		calculate_range_64(clip_normalized_values_64, k_num_segment_values, segment_min_64, segment_max_64);

#if ACL_DEBUG_ARITHMETIC
		printf("Segment range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		Vector4_FP segment_min_fp = vector_to_fp(segment_min_64, 32, true);
		Vector4_FP segment_max_fp = vector_to_fp(segment_max_64, 32, true);
		printf("Segment range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		fixup_range_64(segment_min_64, segment_max_64);

#if ACL_DEBUG_ARITHMETIC
		printf("Segment* range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment* range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		segment_min_fp = vector_to_fp(segment_min_64, 32, true);
		segment_max_fp = vector_to_fp(segment_max_64, 32, true);
		printf("Segment* range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment* range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		normalize_64(clip_normalized_values_64, k_num_segment_values, segment_min_64, segment_max_64, segment_normalized_values_64);
	}
	else
		memcpy(segment_normalized_values_64, clip_normalized_values_64, sizeof(segment_normalized_values_64));

	Vector4_32 quantized_values_64[k_num_segment_values] = { 0 };
	Vector4_64 dequantized_segment_normalized_values_64[k_num_segment_values];
	Vector4_64 dequantized_clip_normalized_values_64[k_num_segment_values];
	Vector4_64 dequantized_values_64[k_num_segment_values];
	for (uint8_t i = 1; i < NUM_BIT_RATES - 1; ++i)
	{
		quantize_64(segment_normalized_values_64, k_num_segment_values, i, quantized_values_64);
		dequantize_64(quantized_values_64, k_num_segment_values, i, dequantized_segment_normalized_values_64);

		if (use_segment_range_reduction)
			denormalize_64(dequantized_segment_normalized_values_64, k_num_segment_values, segment_min_64, segment_max_64, dequantized_clip_normalized_values_64);
		else
			memcpy(dequantized_clip_normalized_values_64, dequantized_segment_normalized_values_64, sizeof(dequantized_segment_normalized_values_64));

		denormalize_64(dequantized_clip_normalized_values_64, k_num_segment_values, clip_min_64, clip_max_64, dequantized_values_64);

#if ACL_DEBUG_ARITHMETIC
		if (i == ACL_DEBUG_BIT_RATE)
		{
			printf("Quantized value %u: { %16X, %16X, %16X }\n", ACL_DEBUG_BONE, ((uint32_t*)&quantized_values_64[ACL_DEBUG_BONE])[0], ((uint32_t*)&quantized_values_64[ACL_DEBUG_BONE])[1], ((uint32_t*)&quantized_values_64[ACL_DEBUG_BONE])[2]);
			printf("Clip norm value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_clip_normalized_values_64[ACL_DEBUG_BONE]), vector_get_y(dequantized_clip_normalized_values_64[ACL_DEBUG_BONE]), vector_get_z(dequantized_clip_normalized_values_64[ACL_DEBUG_BONE]));
			Vector4_FP clip_norm_value0_fp = vector_to_fp(dequantized_clip_normalized_values_64[ACL_DEBUG_BONE], 32, true);
			printf("Clip norm value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_norm_value0_fp.x, clip_norm_value0_fp.y, clip_norm_value0_fp.z);
			printf("Lossy value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_values_64[ACL_DEBUG_BONE]), vector_get_y(dequantized_values_64[ACL_DEBUG_BONE]), vector_get_z(dequantized_values_64[ACL_DEBUG_BONE]));
			Vector4_FP lossy_value0_fp = vector_to_fp(dequantized_values_64[ACL_DEBUG_BONE], 32, false);
			printf("Lossy value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, lossy_value0_fp.x, lossy_value0_fp.y, lossy_value0_fp.z);
		}
#else
		print_error_64(k_values_64, k_num_segment_values, dequantized_values_64, i, out_errors);
#endif
	}

	printf("\n");
}

static void measure_error_32(bool use_segment_range_reduction, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	if (k_dump_error)
		printf("Error for arithmetic: float32\n");
	if (k_dump_error && use_segment_range_reduction)
		printf("With segment range reduction\n");

	Vector4_32 values_32[k_num_values];
	for (size_t i = 0; i < k_num_values; ++i)
		values_32[i] = vector_cast(k_values_64[i]);

	Vector4_32 clip_min_32;
	Vector4_32 clip_max_32;
	calculate_range_32(values_32, k_num_values, clip_min_32, clip_max_32);

	Vector4_32 clip_normalized_values_32[k_num_segment_values];
	normalize_32(values_32, k_num_segment_values, clip_min_32, clip_max_32, clip_normalized_values_32);

#if ACL_DEBUG_ARITHMETIC
	Vector4_64 clip_min_64 = vector_cast(clip_min_32);
	Vector4_64 clip_max_64 = vector_cast(clip_max_32);
	printf("Clip range min: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_min_64), vector_get_y(clip_min_64), vector_get_z(clip_min_64));
	printf("Clip range max: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_max_64), vector_get_y(clip_max_64), vector_get_z(clip_max_64));
	Vector4_FP clip_min_fp = vector_to_fp(clip_min_64, 32, false);
	Vector4_FP clip_max_fp = vector_to_fp(clip_max_64, 32, false);
	printf("Clip range min: { %16I64X, %16I64X, %16I64X }\n", clip_min_fp.x, clip_min_fp.y, clip_min_fp.z);
	printf("Clip range max: { %16I64X, %16I64X, %16I64X }\n", clip_max_fp.x, clip_max_fp.y, clip_max_fp.z);

	Vector4_64 clip_value0_64 = vector_cast(values_32[ACL_DEBUG_BONE]);
	printf("Clip value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(clip_value0_64), vector_get_y(clip_value0_64), vector_get_z(clip_value0_64));
	Vector4_FP clip_value0_fp = vector_to_fp(clip_value0_64, 32, false);
	printf("Clip value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_value0_fp.x, clip_value0_fp.y, clip_value0_fp.z);

	Vector4_64 clip_normalized_value_64 = vector_cast(clip_normalized_values_32[ACL_DEBUG_BONE]);
	printf("Clip normalized value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(clip_normalized_value_64), vector_get_y(clip_normalized_value_64), vector_get_z(clip_normalized_value_64));
	Vector4_FP clip_normalized_value0_fp = vector_to_fp(clip_normalized_value_64, 32, true);
	printf("Clip normalized value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_normalized_value0_fp.x, clip_normalized_value0_fp.y, clip_normalized_value0_fp.z);
#endif

	Vector4_32 segment_min_32;
	Vector4_32 segment_max_32;
	Vector4_32 segment_normalized_values_32[k_num_segment_values];
	if (use_segment_range_reduction)
	{
		calculate_range_32(clip_normalized_values_32, k_num_segment_values, segment_min_32, segment_max_32);

#if ACL_DEBUG_ARITHMETIC
		Vector4_64 segment_min_64 = vector_cast(segment_min_32);
		Vector4_64 segment_max_64 = vector_cast(segment_max_32);
		printf("Segment range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		Vector4_FP segment_min_fp = vector_to_fp(segment_min_64, 32, true);
		Vector4_FP segment_max_fp = vector_to_fp(segment_max_64, 32, true);
		printf("Segment range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		fixup_range_32(segment_min_32, segment_max_32);

#if ACL_DEBUG_ARITHMETIC
		segment_min_64 = vector_cast(segment_min_32);
		segment_max_64 = vector_cast(segment_max_32);
		printf("Segment* range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment* range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		segment_min_fp = vector_to_fp(segment_min_64, 32, true);
		segment_max_fp = vector_to_fp(segment_max_64, 32, true);
		printf("Segment* range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment* range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		normalize_32(clip_normalized_values_32, k_num_segment_values, segment_min_32, segment_max_32, segment_normalized_values_32);
	}
	else
		memcpy(segment_normalized_values_32, clip_normalized_values_32, sizeof(segment_normalized_values_32));

	Vector4_32 quantized_values_32[k_num_segment_values] = { 0 };
	Vector4_32 dequantized_segment_normalized_values_32[k_num_segment_values];
	Vector4_32 dequantized_clip_normalized_values_32[k_num_segment_values];
	Vector4_32 dequantized_values_32[k_num_segment_values];
	for (uint8_t i = 1; i < NUM_BIT_RATES - 1; ++i)
	{
		quantize_32(segment_normalized_values_32, k_num_segment_values, i, quantized_values_32);
		dequantize_32(quantized_values_32, k_num_segment_values, i, dequantized_segment_normalized_values_32);

		if (use_segment_range_reduction)
			denormalize_32(dequantized_segment_normalized_values_32, k_num_segment_values, segment_min_32, segment_max_32, dequantized_clip_normalized_values_32);
		else
			memcpy(dequantized_clip_normalized_values_32, dequantized_segment_normalized_values_32, sizeof(dequantized_segment_normalized_values_32));

		denormalize_32(dequantized_clip_normalized_values_32, k_num_segment_values, clip_min_32, clip_max_32, dequantized_values_32);

#if ACL_DEBUG_ARITHMETIC
		if (i == ACL_DEBUG_BIT_RATE)
		{
			printf("Quantized value %u: { %16X, %16X, %16X }\n", ACL_DEBUG_BONE, ((uint32_t*)&quantized_values_32[ACL_DEBUG_BONE])[0], ((uint32_t*)&quantized_values_32[ACL_DEBUG_BONE])[1], ((uint32_t*)&quantized_values_32[ACL_DEBUG_BONE])[2]);
			Vector4_64 dequantized_clip_normalized_value0_64 = vector_cast(dequantized_clip_normalized_values_32[ACL_DEBUG_BONE]);
			printf("Clip norm value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_clip_normalized_value0_64), vector_get_y(dequantized_clip_normalized_value0_64), vector_get_z(dequantized_clip_normalized_value0_64));
			Vector4_FP clip_norm_value0_fp = vector_to_fp(dequantized_clip_normalized_value0_64, 32, true);
			printf("Clip norm value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_norm_value0_fp.x, clip_norm_value0_fp.y, clip_norm_value0_fp.z);
			Vector4_64 dequantized_value0_64 = vector_cast(dequantized_values_32[ACL_DEBUG_BONE]);
			printf("Lossy value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_value0_64), vector_get_y(dequantized_value0_64), vector_get_z(dequantized_value0_64));
			Vector4_FP lossy_value0_fp = vector_to_fp(dequantized_value0_64, 32, false);
			printf("Lossy value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, lossy_value0_fp.x, lossy_value0_fp.y, lossy_value0_fp.z);
		}
#else
		print_error_32(k_values_64, k_num_segment_values, dequantized_values_32, i, out_errors);
#endif
	}

	printf("\n");
}

static void measure_error_fp(bool use_segment_range_reduction, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	if (k_dump_error)
		printf("Error for arithmetic: fixed point\n");
	if (k_dump_error && use_segment_range_reduction)
		printf("With segment range reduction\n");

	Vector4_FP values_fp[k_num_values];
	for (size_t i = 0; i < k_num_values; ++i)
		values_fp[i] = vector_to_fp(k_values_64[i], 32, false);

	Vector4_FP clip_min_fp;	// 0.32
	Vector4_FP clip_max_fp;	// 0.32
	calculate_range_fp(values_fp, k_num_values, clip_min_fp, clip_max_fp);

	Vector4_FP clip_normalized_values_fp[k_num_segment_values];		// 0.32
	normalize_clip_fp(values_fp, k_num_segment_values, clip_min_fp, clip_max_fp, clip_normalized_values_fp);

#if ACL_DEBUG_ARITHMETIC
	Vector4_64 clip_min_64 = vector_from_fp_64(clip_min_fp, 32, false);
	Vector4_64 clip_max_64 = vector_from_fp_64(clip_max_fp, 32, false);
	printf("Clip range min: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_min_64), vector_get_y(clip_min_64), vector_get_z(clip_min_64));
	printf("Clip range max: { %.10f, %.10f, %.10f }\n", vector_get_x(clip_max_64), vector_get_y(clip_max_64), vector_get_z(clip_max_64));
	printf("Clip range min: { %16I64X, %16I64X, %16I64X }\n", clip_min_fp.x, clip_min_fp.y, clip_min_fp.z);
	printf("Clip range max: { %16I64X, %16I64X, %16I64X }\n", clip_max_fp.x, clip_max_fp.y, clip_max_fp.z);

	Vector4_64 clip_value0_64 = vector_from_fp_64(values_fp[ACL_DEBUG_BONE], 32, false);
	printf("Clip value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(clip_value0_64), vector_get_y(clip_value0_64), vector_get_z(clip_value0_64));
	printf("Clip value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, values_fp[ACL_DEBUG_BONE].x, values_fp[ACL_DEBUG_BONE].y, values_fp[ACL_DEBUG_BONE].z);

	Vector4_64 clip_normalized_value_64 = vector_from_fp_64(clip_normalized_values_fp[ACL_DEBUG_BONE], 32, true);
	printf("Clip normalized value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(clip_normalized_value_64), vector_get_y(clip_normalized_value_64), vector_get_z(clip_normalized_value_64));
	printf("Clip normalized value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, clip_normalized_values_fp[ACL_DEBUG_BONE].x, clip_normalized_values_fp[ACL_DEBUG_BONE].y, clip_normalized_values_fp[ACL_DEBUG_BONE].z);
#endif

	Vector4_FP segment_min_fp;	// 0.8
	Vector4_FP segment_max_fp;	// 0.8
	Vector4_FP segment_normalized_values_fp[k_num_segment_values];	// 0.24
	if (use_segment_range_reduction)
	{
		calculate_range_fp(clip_normalized_values_fp, k_num_segment_values, segment_min_fp, segment_max_fp);

#if ACL_DEBUG_ARITHMETIC
		Vector4_64 segment_min_64 = vector_from_fp_64(segment_min_fp, 8, true);
		Vector4_64 segment_max_64 = vector_from_fp_64(segment_max_fp, 8, true);
		printf("Segment range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		printf("Segment range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		fixup_range_fp(segment_min_fp, segment_max_fp);

#if ACL_DEBUG_ARITHMETIC
		segment_min_64 = vector_from_fp_64(segment_min_fp, 8, true);
		segment_max_64 = vector_from_fp_64(segment_max_fp, 8, true);
		printf("Segment* range min: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_min_64), vector_get_y(segment_min_64), vector_get_z(segment_min_64));
		printf("Segment* range max: { %.10f, %.10f, %.10f }\n", vector_get_x(segment_max_64), vector_get_y(segment_max_64), vector_get_z(segment_max_64));
		printf("Segment* range min: { %16I64X, %16I64X, %16I64X }\n", segment_min_fp.x, segment_min_fp.y, segment_min_fp.z);
		printf("Segment* range max: { %16I64X, %16I64X, %16I64X }\n", segment_max_fp.x, segment_max_fp.y, segment_max_fp.z);
#endif

		normalize_segment_fp(clip_normalized_values_fp, k_num_segment_values, segment_min_fp, segment_max_fp, segment_normalized_values_fp);
	}
	else
		memcpy(segment_normalized_values_fp, clip_normalized_values_fp, sizeof(segment_normalized_values_fp));

	Vector4_32 quantized_values_fp[k_num_segment_values] = { 0 };
	Vector4_FP dequantized_segment_normalized_values_fp[k_num_segment_values];	// 0.24
	Vector4_FP dequantized_clip_normalized_values_fp[k_num_segment_values];		// 0.32
	Vector4_FP dequantized_values_fp[k_num_segment_values];						// 0.32
	for (uint8_t i = 1; i < NUM_BIT_RATES - 1; ++i)
	{
		quantize_fp(segment_normalized_values_fp, k_num_segment_values, i, use_segment_range_reduction, quantized_values_fp);
		dequantize_fp(quantized_values_fp, k_num_segment_values, i, use_segment_range_reduction, dequantized_segment_normalized_values_fp);

		if (use_segment_range_reduction)
			denormalize_segment_fp(dequantized_segment_normalized_values_fp, k_num_segment_values, segment_min_fp, segment_max_fp, dequantized_clip_normalized_values_fp);
		else
			memcpy(dequantized_clip_normalized_values_fp, dequantized_segment_normalized_values_fp, sizeof(dequantized_segment_normalized_values_fp));

		denormalize_clip_fp(dequantized_clip_normalized_values_fp, k_num_segment_values, clip_min_fp, clip_max_fp, dequantized_values_fp);

#if ACL_DEBUG_ARITHMETIC
		if (i == ACL_DEBUG_BIT_RATE)
		{
			printf("Quantized value %u: { %16X, %16X, %16X }\n", ACL_DEBUG_BONE, ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[0], ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[1], ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[2]);
			Vector4_64 dequantized_clip_normalized_value0_64 = vector_from_fp_64(dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE], 32, true);
			printf("Clip norm value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_clip_normalized_value0_64), vector_get_y(dequantized_clip_normalized_value0_64), vector_get_z(dequantized_clip_normalized_value0_64));
			printf("Clip norm value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].x, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].y, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].z);
			Vector4_64 dequantized_value0_64 = vector_from_fp_64(dequantized_values_fp[ACL_DEBUG_BONE], 32, false);
			printf("Lossy value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_value0_64), vector_get_y(dequantized_value0_64), vector_get_z(dequantized_value0_64));
			printf("Lossy value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, dequantized_values_fp[ACL_DEBUG_BONE].x, dequantized_values_fp[ACL_DEBUG_BONE].y, dequantized_values_fp[ACL_DEBUG_BONE].z);
		}
#else
		print_error_fp(k_values_64, k_num_segment_values, dequantized_values_fp, i, out_errors);
#endif
	}

	printf("\n");
}

void test_arithmetic()
{
	Vector4_64 error_64[NUM_BIT_RATES][k_num_segment_values];
	Vector4_64 error_32[NUM_BIT_RATES][k_num_segment_values];
	Vector4_64 error_fp[NUM_BIT_RATES][k_num_segment_values];

	measure_error_64(false, error_64);
	measure_error_32(false, error_32);
	measure_error_fp(false, error_fp);

	uint32_t num_total_wins_64 = 0;
	uint32_t num_total_wins_32 = 0;
	uint32_t num_total_wins_fp = 0;
	for (uint8_t bit_rate = 1; bit_rate < NUM_BIT_RATES - 1; ++bit_rate)
	{
		uint32_t num_wins_64 = 0;
		uint32_t num_wins_32 = 0;
		uint32_t num_wins_fp = 0;
		for (size_t i = 0; i < k_num_segment_values; ++i)
		{
			if (k_enable_float64)
			{
				if ((!k_enable_float32 || vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i])))
					num_wins_64++;

				if ((!k_enable_float32 || vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i])))
					num_wins_64++;

				if ((!k_enable_float32 || vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i])))
					num_wins_64++;
			}

			if (k_enable_float32)
			{
				if ((!k_enable_float64 || vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i])))
					num_wins_32++;

				if ((!k_enable_float64 || vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i])))
					num_wins_32++;

				if ((!k_enable_float64 || vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i])))
					num_wins_32++;
			}

			if (k_enable_fp)
			{
				if ((!k_enable_float64 || vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_32[bit_rate][i])))
					num_wins_fp++;

				if ((!k_enable_float64 || vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_32[bit_rate][i])))
					num_wins_fp++;

				if ((!k_enable_float64 || vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_32[bit_rate][i])))
					num_wins_fp++;
			}
		}

		const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		printf("Bit rate %u (%u, %u, %u) wins: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_wins_64, num_wins_32, num_wins_fp);

		num_total_wins_64 += num_wins_64;
		num_total_wins_32 += num_wins_32;
		num_total_wins_fp += num_wins_fp;
	}

	printf("No segmenting wins: 64 [%u] 32 [%u] fp [%u]\n", num_total_wins_64, num_total_wins_32, num_total_wins_fp);

	measure_error_64(true, error_64);
	measure_error_32(true, error_32);
	measure_error_fp(true, error_fp);

	num_total_wins_64 = 0;
	num_total_wins_32 = 0;
	num_total_wins_fp = 0;
	for (uint8_t bit_rate = 1; bit_rate < NUM_BIT_RATES - 1; ++bit_rate)
	{
		uint32_t num_wins_64 = 0;
		uint32_t num_wins_32 = 0;
		uint32_t num_wins_fp = 0;
		for (size_t i = 0; i < k_num_segment_values; ++i)
		{
			if (k_enable_float64)
			{
				if ((!k_enable_float32 || vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i])))
					num_wins_64++;

				if ((!k_enable_float32 || vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i])))
					num_wins_64++;

				if ((!k_enable_float32 || vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]))
					&& (!k_enable_fp || vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i])))
					num_wins_64++;
			}

			if (k_enable_float32)
			{
				if ((!k_enable_float64 || vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i])))
					num_wins_32++;

				if ((!k_enable_float64 || vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i])))
					num_wins_32++;

				if ((!k_enable_float64 || vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]))
					&& (!k_enable_fp || vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i])))
					num_wins_32++;
			}

			if (k_enable_fp)
			{
				if ((!k_enable_float64 || vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_32[bit_rate][i])))
					num_wins_fp++;

				if ((!k_enable_float64 || vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_32[bit_rate][i])))
					num_wins_fp++;

				if ((!k_enable_float64 || vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]))
					&& (!k_enable_float32 || vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_32[bit_rate][i])))
					num_wins_fp++;
			}
		}

		const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
		printf("Bit rate %u (%u, %u, %u) wins: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_wins_64, num_wins_32, num_wins_fp);

		num_total_wins_64 += num_wins_64;
		num_total_wins_32 += num_wins_32;
		num_total_wins_fp += num_wins_fp;
	}

	printf("Segmenting wins: 64 [%u] 32 [%u] fp [%u]\n", num_total_wins_64, num_total_wins_32, num_total_wins_fp);
}

static int main_impl(int argc, char** argv)
{
	test_arithmetic();
	return 0;
}

int main(int argc, char** argv)
{
	int result = main_impl(argc, argv);

	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}

	return result;
}
