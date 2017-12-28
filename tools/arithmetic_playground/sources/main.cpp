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
#include "acl/core/scope_profiler.h"
#include "acl/math/vector4_32.h"
#include "acl/math/vector4_64.h"

#include <vector>
#include <thread>
#include <atomic>
#include <random>

#ifdef _WIN32
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

#else	// _WIN32
static constexpr int _kbhit() { return 0; }
static constexpr bool IsDebuggerPresent() { return false; }
#endif	// _WIN32

#define ACL_DEBUG_ARITHMETIC					0
#define ACL_DEBUG_BIT_RATE						14
#define ACL_DEBUG_BONE							0
#define ACL_MEASURE_COMP_WINS					1
#define ACL_MEASURE_COMP_LOSS					0
#define ACL_MEASURE_VEC3_WINS					1
#define ACL_MEASURE_VEC3_LOSS					0

// 0: FixedPoint -> cvt float32
// 1: FixedPoint -> cast float32 -> normalize
// 2: FixedPoint -> cast float32 -> normalize with delayed remap signed
#define ACL_HACK_COERCION_F32					1

// 0: FixedPoint -> Cvt float64 -> Cvt float32 -> remap signed
// 1: FixedPoint -> Cast float64 -> normalize -> cvt float32 -> remap signed
// 2: FixedPoint -> Cast float64 -> cvt float32 -> normalize & remap signed
// 3: FixedPoint -> Cast float32 -> normalize & remap signed
// 4: FixedPoint -> Cvt float32 -> remap signed
#define ACL_HACK_COERCION_FP_32					0

//#define ACL_DEBUG_INLINE						__declspec(noinline)
#define ACL_DEBUG_INLINE

#define ACL_VOLATILE_ volatile
//#define ACL_VOLATILE_

#ifdef _WIN32
#define ACL_FORCE_NO_INLINE						__declspec(noinline)
#define ACL_VECTOR_CALL							__vectorcall
#else
#define ACL_FORCE_NO_INLINE
#define ACL_VECTOR_CALL
#endif

static const size_t k_num_segment_values = 18;
static const bool k_remap_fp_range = false;
static const bool k_enable_float64 = false;
static const bool k_enable_float32 = true;
static const bool k_enable_fp = true;
static const bool k_dump_error = false;
static const bool k_dump_bit_rate_wins = false;
static const bool k_validate_sse_results = false;
static const bool k_exhaustive_accuracy_test = false;

using namespace acl;

alignas(16) static const uint64_t k_raw_data[]
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

alignas(16) static const uint64_t k_clip_range[]
{
	uint64_t(0xbfc24b48b8f03ffcull), uint64_t(0xbfe6a8ad6e48cee9ull), uint64_t(0xbfe240f2dfd93c0cull), uint64_t(0x3fda86e7a8f45a4eull),
	uint64_t(0xbfa34f65bf0e40d9ull), uint64_t(0xbfc1115cc7c50094ull), uint64_t(0xbfb17e488a5ce18dull), uint64_t(0x3fef4e743f849140ull),
};

alignas(16) static const uint64_t k_segment_range[]
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
	return (uint64_t)min(symmetric_round(input * double(uint64_t(1ull) << num_bits)), double((uint64_t(1ull) << num_bits) - 1));
}

static double scalar_from_fp_64(uint64_t input, uint8_t num_bits, bool is_unsigned)
{
	ACL_ENSURE(input <= ((uint64_t(1) << num_bits) - 1), "Invalid input!");

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

static float scalar_from_fp_32(uint64_t input, uint8_t num_bits, bool is_unsigned)
{
	ACL_ENSURE(input <= ((uint64_t(1) << num_bits) - 1), "Invalid input!");

#if ACL_HACK_COERCION_FP_32 == 0
	// 0: FixedPoint -> Cvt float64 -> Cvt float32 -> remap signed
	uint64_t max_value = uint64_t(1ull) << num_bits;
	double value_dbl = safe_to_double(input) / safe_to_double(max_value);
	float value_flt = float(value_dbl);
	// See comment above as to why we remap the range
	// The scale factor (1 << num_bits) / ((1 << num_bits) - 1) is larger than 1.0
	float scale = float(uint64_t(1ull) << num_bits) / float((uint64_t(1ull) << num_bits) - 1);
	if (k_remap_fp_range)
		value_flt *= scale;
	if (!is_unsigned)
		value_flt = (value_flt * 2.0f) - 1.0f;
	return value_flt;
#elif ACL_HACK_COERCION_FP_32 == 1
	// 1: FixedPoint -> Cast float64 -> normalize -> cvt float32 -> remap signed
	uint64_t value_u64 = (uint64_t(0x3ff) << 52) | (input << (52 - 32));
	double value_dbl = *reinterpret_cast<double*>(&value_u64) - 1.0;
	float value_flt = float(value_dbl);
	if (!is_unsigned)
		value_flt = (value_flt * 2.0f) - 1.0f;
	return value_flt;
#elif ACL_HACK_COERCION_FP_32 == 2
	// 2: FixedPoint -> Cast float64 -> cvt float32 -> normalize & remap signed
	uint64_t value_u64 = (uint64_t(0x3ff) << 52) | (input << (52 - 32));
	double value_dbl = *reinterpret_cast<double*>(&value_u64);
	float value_flt = float(value_dbl);
	if (!is_unsigned)
		value_flt = (value_flt * 2.0f) - 3.0f;
	else
		value_flt -= 1.0f;
	return value_flt;
#elif ACL_HACK_COERCION_FP_32 == 3
	// 3: FixedPoint -> Cast float32 -> normalize & remap signed
	uint32_t mantissa = uint32_t(input >> (num_bits - 23));
	uint32_t exponent = 0x3f800000;
	uint32_t value_u32 = mantissa | exponent;
	float value_flt = *reinterpret_cast<float*>(&value_u32);
	if (!is_unsigned)
		value_flt = (value_flt * 2.0f) - 3.0f;
	else
		value_flt -= 1.0f;
	return value_flt;
#elif ACL_HACK_COERCION_FP_32 == 4
	// 4: FixedPoint -> Cvt float32 -> remap signed
	uint32_t max_value = 1 << 19;
	// No rounding, we truncate
	float value_flt = safe_to_float(input >> (32 - 19)) / safe_to_float(max_value);
	if (!is_unsigned)
		value_flt = (value_flt * 2.0f) - 1.0f;
	return value_flt;
#endif
}

static Vector4_FP vector_to_fp(const Vector4_64& input, uint8_t num_bits, bool is_unsigned)
{
	return Vector4_FP{ scalar_to_fp(vector_get_x(input), num_bits, is_unsigned), scalar_to_fp(vector_get_y(input), num_bits, is_unsigned), scalar_to_fp(vector_get_z(input), num_bits, is_unsigned), scalar_to_fp(vector_get_w(input), num_bits, is_unsigned) };
}

static Vector4_FP vector_to_fp(const Vector4_32& input, uint8_t num_bits, bool is_unsigned)
{
	return vector_to_fp(vector_cast(input), num_bits, is_unsigned);
}

static Vector4_64 vector_from_fp_64(const Vector4_FP& input, uint8_t num_bits, bool is_unsigned)
{
	return vector_set(scalar_from_fp_64(input.x, num_bits, is_unsigned), scalar_from_fp_64(input.y, num_bits, is_unsigned), scalar_from_fp_64(input.z, num_bits, is_unsigned), scalar_from_fp_64(input.w, num_bits, is_unsigned));
}

static Vector4_32 vector_from_fp_32(const Vector4_FP& input, uint8_t num_bits, bool is_unsigned)
{
	return vector_set(scalar_from_fp_32(input.x, num_bits, is_unsigned), scalar_from_fp_32(input.y, num_bits, is_unsigned), scalar_from_fp_32(input.z, num_bits, is_unsigned), scalar_from_fp_32(input.w, num_bits, is_unsigned));
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
		return vector_min(vector_shift_right(vector_add(input, bias), num_truncated_bits), vector_set((uint64_t(1ull) << to_bits) - 1));
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
		normalized_value = vector_min(normalized_value, vector_set((uint64_t(1ull) << 32) - 1));
		out_normalized_values[i] = normalized_value;
	}
}

static void normalize_segment_fp(const Vector4_FP* values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_normalized_values)
{
	// Range min/max are 0.8
	Vector4_FP range_extent = vector_sub(range_max, range_min);
	// We cannot represent 1.0, increment the range extent by 1
	range_extent = vector_add(range_extent, vector_set(uint64_t(1)));

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& value = values[i];
		Vector4_FP offset = vector_sub(value, range_min);
		Vector4_FP normalized_value;
		normalized_value.x = range_extent.x != 0 ? (offset.x / range_extent.x) : 0;
		normalized_value.y = range_extent.y != 0 ? (offset.y / range_extent.y) : 0;
		normalized_value.z = range_extent.z != 0 ? (offset.z / range_extent.z) : 0;
		normalized_value.w = range_extent.w != 0 ? (offset.w / range_extent.w) : 0;
		normalized_value = vector_min(normalized_value, vector_set((uint64_t(1ull) << 24) - 1));
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

static uint32_t pack_scalar_unsigned_32_(float input, uint8_t num_bits)
{
	ACL_ENSURE(input >= 0.0f && input < 1.0f, "Invalue input value: 0.0 <= %f < 1.0", input);
	uint32_t max_value = (1 << num_bits);
	return std::min<uint32_t>(static_cast<uint32_t>(symmetric_round(input * safe_to_float(max_value))), (1 << num_bits) - 1);
}

static void unpack_scalar_unsigned_32(size_t input, size_t num_bits, float& out_result)
{
	size_t max_value = (1 << num_bits) - 1;
	ACL_ENSURE(input <= max_value, "Invalue input value: %ull <= 1.0", input);
	out_result = safe_to_float(input) / safe_to_float(max_value);
}

static Vector4_32 vector_from_range32(Vector4_32 input)
{
#if ACL_HACK_COERCION_F32 == 0
	// 0: FixedPoint -> cvt float32
	size_t vector_x = pack_scalar_unsigned_32(vector_get_x(input), 8);
	size_t vector_y = pack_scalar_unsigned_32(vector_get_y(input), 8);
	size_t vector_z = pack_scalar_unsigned_32(vector_get_z(input), 8);
	return vector_set(float(vector_x) / 255.0f, float(vector_y) / 255.0f, float(vector_z) / 255.0f, 0.0f);
#elif ACL_HACK_COERCION_F32 == 1
	// 1: FixedPoint -> cast float32 -> normalize
	float scale = float(1 << 8) / float((1 << 8) - 1);
	float inv_scale = float((1 << 8) - 1) / float(1 << 8);
	input = vector_mul(input, inv_scale);
	uint32_t vector_x = pack_scalar_unsigned_32_(vector_get_x(input), 8);
	uint32_t vector_y = pack_scalar_unsigned_32_(vector_get_y(input), 8);
	uint32_t vector_z = pack_scalar_unsigned_32_(vector_get_z(input), 8);
	uint32_t exponent = 0x3f800000;
	uint32_t value_x_u32 = uint32_t(vector_x << (23 - 8)) | exponent;
	uint32_t value_y_u32 = uint32_t(vector_y << (23 - 8)) | exponent;
	uint32_t value_z_u32 = uint32_t(vector_z << (23 - 8)) | exponent;
	float value_x_flt = *reinterpret_cast<float*>(&value_x_u32) - 1.0f;
	float value_y_flt = *reinterpret_cast<float*>(&value_y_u32) - 1.0f;
	float value_z_flt = *reinterpret_cast<float*>(&value_z_u32) - 1.0f;
	return vector_mul(vector_set(value_x_flt, value_y_flt, value_z_flt, 0.0f), scale);
#elif ACL_HACK_COERCION_F32 == 2
	// 2: FixedPoint -> cast float32 -> normalize with delayed remap signed
#endif
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
	Vector4_FP clamped_range_min = vector_min(vector_shift_right(range_min, 24), vector_set(uint64_t(0xFF)));
	Vector4_FP clamped_range_max = vector_min(vector_shift_right(vector_add(range_max, vector_set(uint64_t(0x80))), 24), vector_set(uint64_t(0xFF)));
	//Vector4_FP clamped_range_max = vector_min(vector_add(vector_shift_right(vector_add(range_max, vector_set(uint64_t(0x80))), 24), vector_set(uint64_t(1))), vector_set(uint64_t(0xFF)));

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

static void pack_vector3_n(Vector4_32 vector, uint8_t XBits, uint8_t YBits, uint8_t ZBits, uint8_t* out_vector_data)
{
#if ACL_HACK_COERCION_F32 == 0
	// 0: FixedPoint -> cvt float32
	size_t vector_x = pack_scalar_unsigned_32(vector_get_x(vector), XBits);
	size_t vector_y = pack_scalar_unsigned_32(vector_get_y(vector), YBits);
	size_t vector_z = pack_scalar_unsigned_32(vector_get_z(vector), ZBits);

	uint64_t vector_u64 = safe_static_cast<uint64_t>((vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z);
#elif ACL_HACK_COERCION_F32 == 1
	// 1: FixedPoint -> cast float32 -> normalize
	float inv_scale = float((1 << XBits) - 1) / float(1 << XBits);
	vector = vector_mul(vector, inv_scale);
	size_t vector_x = pack_scalar_unsigned_32_(vector_get_x(vector), XBits);
	size_t vector_y = pack_scalar_unsigned_32_(vector_get_y(vector), YBits);
	size_t vector_z = pack_scalar_unsigned_32_(vector_get_z(vector), ZBits);

	uint64_t vector_u64 = safe_static_cast<uint64_t>((vector_x << (YBits + ZBits)) | (vector_y << ZBits) | vector_z);
#endif

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
#if ACL_HACK_COERCION_F32 == 0
	// 0: FixedPoint -> cvt float32
	float x, y, z;
	unpack_scalar_unsigned_32(x64, XBits, x);
	unpack_scalar_unsigned_32(y64, YBits, y);
	unpack_scalar_unsigned_32(z64, ZBits, z);
	return vector_set(x, y, z);
#elif ACL_HACK_COERCION_F32 == 1
	// 1: FixedPoint -> cast float32 -> normalize
	float scale = float(1 << XBits) / float((1 << XBits) - 1);
	uint32_t exponent = 0x3f800000;
	uint32_t value_x_u32 = uint32_t(x64 << (23 - XBits)) | exponent;
	uint32_t value_y_u32 = uint32_t(y64 << (23 - YBits)) | exponent;
	uint32_t value_z_u32 = uint32_t(z64 << (23 - ZBits)) | exponent;
	float value_x_flt = *reinterpret_cast<float*>(&value_x_u32) - 1.0f;
	float value_y_flt = *reinterpret_cast<float*>(&value_y_u32) - 1.0f;
	float value_z_flt = *reinterpret_cast<float*>(&value_z_u32) - 1.0f;
	return vector_mul(vector_set(value_x_flt, value_y_flt, value_z_flt, 0.0f), scale);
#endif
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

static void denormalize_64(const Vector4_64* normalized_values, size_t num_values, const Vector4_64& range_min, const Vector4_64& range_max, Vector4_32* out_values)
{
	const Vector4_64 range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& normalized_value = normalized_values[i];
		out_values[i] = vector_cast(vector_mul_add(normalized_value, range_extent, range_min));
	}
}

static void denormalize_clip_32(const Vector4_32* normalized_values, size_t num_values, const Vector4_32& range_min, const Vector4_32& range_max, Vector4_32* out_values)
{
	const Vector4_32 range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_32& normalized_value = normalized_values[i];
		out_values[i] = vector_mul_add(normalized_value, range_extent, range_min);
	}
}

static void denormalize_segment_32(const Vector4_32* normalized_values, size_t num_values, const Vector4_32& range_min, const Vector4_32& range_max, Vector4_32* out_values)
{
	const Vector4_32 range_extent = vector_sub(range_max, range_min);
	const Vector4_32 range_extent_ = vector_from_range32(range_extent);
	const Vector4_32 range_min_ = vector_from_range32(range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_32& normalized_value = normalized_values[i];
		out_values[i] = vector_mul_add(normalized_value, range_extent_, range_min_);
	}
}

static void denormalize_clip_fp(const Vector4_FP* normalized_values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_32* out_values)
{
	const Vector4_FP range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& normalized_value = normalized_values[i];
		Vector4_FP result = vector_mul(normalized_value, range_extent);
		result = vector_shift_right(result, 32);	// Truncate
		result = vector_add(result, range_min);
		out_values[i] = vector_from_fp_32(result, 32, false);
	}
}

static void denormalize_clip_fp(const Vector4_FP* normalized_values, size_t num_values, const Vector4_32& range_min, const Vector4_32& range_max, Vector4_32* out_values)
{
	const Vector4_32 range_extent = vector_sub(range_max, range_min);

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& normalized_value = normalized_values[i];
		Vector4_32 normalized_value32 = vector_from_fp_32(normalized_value, 32, true);
		Vector4_32 result = vector_mul(normalized_value32, range_extent);
		result = vector_add(result, range_min);
		out_values[i] = result;
	}
}

static void denormalize_segment_fp(const Vector4_FP* normalized_values, size_t num_values, const Vector4_FP& range_min, const Vector4_FP& range_max, Vector4_FP* out_values)
{
	Vector4_FP range_extent = vector_sub(range_max, range_min);
	// We cannot represent 1.0, increment the range extent by 1
	range_extent = vector_add(range_extent, vector_set(uint64_t(1)));

	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_FP& normalized_value = normalized_values[i];
		Vector4_FP result = vector_mul(normalized_value, range_extent);
		result = vector_add(result, range_min);
		out_values[i] = result;
	}
}

static void print_error_64(const Vector4_64* raw_values, size_t num_values, const Vector4_32* lossy_values, uint8_t bit_rate, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
	if (k_dump_error)
		printf("Bit rate: %u (%u, %u, %u)\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate);
	for (size_t i = 0; i < num_values; ++i)
	{
		const Vector4_64& raw_value = raw_values[i];
		const Vector4_64& lossy_value = vector_cast(lossy_values[i]);
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

static void print_error_fp(const Vector4_64* raw_values, size_t num_values, const Vector4_32* lossy_values, uint8_t bit_rate, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
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
	Vector4_32 dequantized_values_64[k_num_segment_values];
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

	if (k_dump_error)
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
			denormalize_segment_32(dequantized_segment_normalized_values_32, k_num_segment_values, segment_min_32, segment_max_32, dequantized_clip_normalized_values_32);
		else
			memcpy(dequantized_clip_normalized_values_32, dequantized_segment_normalized_values_32, sizeof(dequantized_segment_normalized_values_32));

		denormalize_clip_32(dequantized_clip_normalized_values_32, k_num_segment_values, clip_min_32, clip_max_32, dequantized_values_32);

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

	if (k_dump_error)
		printf("\n");
}

static __m128i k_exponent_bits_xyzw32 = _mm_set1_epi32(0x3f800000);
static __m128i k_exponent_bits_xyzw64 = _mm_set1_epi64x((uint64_t(0x3ff)) << 52);
static __m128d k_dbl_offset = _mm_set1_pd(1.0);
static __m128  k_one = vector_set(1.0f);
static __m128  k_two = vector_set(2.0f);
static __m128  k_three = vector_set(3.0f);
static __m128  k_max_8bit_value = vector_set(255.0f);
static __m128  k_max_16bit_value = vector_set(65535.0f);
static __m128  k_8bit_scale = vector_set(256.0f / 255.0f);
static __m128  k_16bit_scale = vector_set(65536.0f / 65535.0f);

static const int32_t k_num_segment_value_bits = 8;
static const int32_t k_one_float_as_i32 = 0x3f800000;
static const int32_t k_two_float_as_i32 = 0x40000000;

static const float VALUE_BITS_MAX[] =
{
	float((1 << 0) - 1), float((1 << 1) - 1), float((1 << 2) - 1), float((1 << 3) - 1),
	float((1 << 4) - 1), float((1 << 5) - 1), float((1 << 6) - 1), float((1 << 7) - 1),
	float((1 << 8) - 1), float((1 << 9) - 1), float((1 << 10) - 1), float((1 << 11) - 1),
	float((1 << 12) - 1), float((1 << 13) - 1), float((1 << 14) - 1), float((1 << 15) - 1),
	float((1 << 16) - 1),
};

static const float SEGMENT_BITS_MAX = float((1 << k_num_segment_value_bits) - 1);
static const int32_t SEGMENT_SHIFT_AMOUNT = 23 - k_num_segment_value_bits;
static const int32_t EXPONENT_BITS = 0x3f800000;

static const float SAMPLE_SCALE_FLT[] =
{
	float(1 << 0) / float((1 << 0) - 1), float(1 << 1) / float((1 << 1) - 1), float(1 << 2) / float((1 << 2) - 1), float(1 << 3) / float((1 << 3) - 1),
	float(1 << 4) / float((1 << 4) - 1), float(1 << 5) / float((1 << 5) - 1), float(1 << 6) / float((1 << 6) - 1), float(1 << 7) / float((1 << 7) - 1),
	float(1 << 8) / float((1 << 8) - 1), float(1 << 9) / float((1 << 9) - 1), float(1 << 10) / float((1 << 10) - 1), float(1 << 11) / float((1 << 11) - 1),
	float(1 << 12) / float((1 << 12) - 1), float(1 << 13) / float((1 << 13) - 1), float(1 << 14) / float((1 << 14) - 1), float(1 << 15) / float((1 << 15) - 1),
	float(1 << 16) / float((1 << 16) - 1),
};

// (1.0 << (N + 16)) / N.0 = 17.0 | 1.16
static const uint32_t SAMPLE_SCALE_I17[] =
{
	0, uint32_t(((uint64_t(1) << 1) << 16) / ((uint64_t(1) << 1) - 1)),
	uint32_t(((uint64_t(1) << 2) << 16) / ((uint64_t(1) << 2) - 1)), uint32_t(((uint64_t(1) << 3) << 16) / ((uint64_t(1) << 3) - 1)),
	uint32_t(((uint64_t(1) << 4) << 16) / ((uint64_t(1) << 4) - 1)), uint32_t(((uint64_t(1) << 5) << 16) / ((uint64_t(1) << 5) - 1)),
	uint32_t(((uint64_t(1) << 6) << 16) / ((uint64_t(1) << 6) - 1)), uint32_t(((uint64_t(1) << 7) << 16) / ((uint64_t(1) << 7) - 1)),
	uint32_t(((uint64_t(1) << 8) << 16) / ((uint64_t(1) << 8) - 1)), uint32_t(((uint64_t(1) << 9) << 16) / ((uint64_t(1) << 9) - 1)),
	uint32_t(((uint64_t(1) << 10) << 16) / ((uint64_t(1) << 10) - 1)), uint32_t(((uint64_t(1) << 11) << 16) / ((uint64_t(1) << 11) - 1)),
	uint32_t(((uint64_t(1) << 12) << 16) / ((uint64_t(1) << 12) - 1)), uint32_t(((uint64_t(1) << 13) << 16) / ((uint64_t(1) << 13) - 1)),
	uint32_t(((uint64_t(1) << 14) << 16) / ((uint64_t(1) << 14) - 1)), uint32_t(((uint64_t(1) << 15) << 16) / ((uint64_t(1) << 15) - 1)),
	uint32_t(((uint64_t(1) << 16) << 16) / ((uint64_t(1) << 16) - 1)),
};

// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
static const uint32_t SAMPLE_SCALE_I32[] =
{
	0, uint32_t(((uint64_t(1) << 1) << 31) / ((uint64_t(1) << 1) - 1)),
	uint32_t(((uint64_t(1) << 2) << 31) / ((uint64_t(1) << 2) - 1)), uint32_t(((uint64_t(1) << 3) << 31) / ((uint64_t(1) << 3) - 1)),
	uint32_t(((uint64_t(1) << 4) << 31) / ((uint64_t(1) << 4) - 1)), uint32_t(((uint64_t(1) << 5) << 31) / ((uint64_t(1) << 5) - 1)),
	uint32_t(((uint64_t(1) << 6) << 31) / ((uint64_t(1) << 6) - 1)), uint32_t(((uint64_t(1) << 7) << 31) / ((uint64_t(1) << 7) - 1)),
	uint32_t(((uint64_t(1) << 8) << 31) / ((uint64_t(1) << 8) - 1)), uint32_t(((uint64_t(1) << 9) << 31) / ((uint64_t(1) << 9) - 1)),
	uint32_t(((uint64_t(1) << 10) << 31) / ((uint64_t(1) << 10) - 1)), uint32_t(((uint64_t(1) << 11) << 31) / ((uint64_t(1) << 11) - 1)),
	uint32_t(((uint64_t(1) << 12) << 31) / ((uint64_t(1) << 12) - 1)), uint32_t(((uint64_t(1) << 13) << 31) / ((uint64_t(1) << 13) - 1)),
	uint32_t(((uint64_t(1) << 14) << 31) / ((uint64_t(1) << 14) - 1)), uint32_t(((uint64_t(1) << 15) << 31) / ((uint64_t(1) << 15) - 1)),
	uint32_t(((uint64_t(1) << 16) << 31) / ((uint64_t(1) << 16) - 1)),
};

static const uint64_t SAMPLE_SHIFT_AMOUNT_23[] =
{
	23 - 0, 23 - 1, 23 - 2, 23 - 3,
	23 - 4, 23 - 5, 23 - 6, 23 - 7,
	23 - 8, 23 - 9, 23 - 10, 23 - 11,
	23 - 12, 23 - 13, 23 - 14, 23 - 15,
	23 - 16,
};

static const uint64_t SAMPLE_SHIFT_AMOUNT_16[] =
{
	16 - 0, 16 - 1, 16 - 2, 16 - 3,
	16 - 4, 16 - 5, 16 - 6, 16 - 7,
	16 - 8, 16 - 9, 16 - 10, 16 - 11,
	16 - 12, 16 - 13, 16 - 14, 16 - 15,
	16 - 16,
};

static const float SEGMENT_SCALE_FLT = float(1 << k_num_segment_value_bits) / float((1 << k_num_segment_value_bits) - 1);
static const uint32_t SEGMENT_SCALE_I9 = ((1 << k_num_segment_value_bits) << 8) / ((1 << k_num_segment_value_bits) - 1);
static const uint32_t SEGMENT_SCALE_I25 = uint32_t(((uint64_t(1) << k_num_segment_value_bits) << 24) / ((uint64_t(1) << k_num_segment_value_bits) - 1));
static const float ONE = 1.0f;
static const float TWO = 2.0f;

// (1.0 << (32 + 31)) / 32.0 = 32.0 | 1.31
static const uint32_t CLIP_SCALE_I32 = uint32_t((uint64_t(1) << 63) / ((uint64_t(1) << 32) - 1));

#define _mm_shuffle_epi32_ab(a, b, mask) _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), mask))
#define _mm_broadcast_epi32(ptr) _mm_castps_si128(_mm_load1_ps((float*)(ptr)))
#define _mm_load_epi32(ptr) _mm_castps_si128(_mm_load_ss((float*)(ptr)))

// Float32 classic conversion
ACL_FORCE_NO_INLINE __m128 ACL_VECTOR_CALL decompress_f32_0(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw)
{
	__m128i quant_xyzw = _mm_loadu_si128(quantized_value);
	__m128 segment_normalized_xyzw = _mm_div_ps(_mm_cvtepi32_ps(quant_xyzw), k_max_16bit_value);
	__m128 segment_range_extent_xyzw32 = _mm_div_ps(_mm_cvtepi32_ps(segment_range_extent_xyzw), k_max_8bit_value);
	__m128 segment_range_min_xyzw32 = _mm_div_ps(_mm_cvtepi32_ps(segment_range_min_xyzw), k_max_8bit_value);
	__m128 clip_normalized_xyzw = _mm_add_ps(_mm_mul_ps(segment_normalized_xyzw, segment_range_extent_xyzw32), segment_range_min_xyzw32);
	return _mm_add_ps(_mm_mul_ps(clip_normalized_xyzw, *clip_range_extent_xyzw), *clip_range_min_xyzw);
}

// Float32 hack conversion
ACL_FORCE_NO_INLINE __m128 ACL_VECTOR_CALL decompress_f32_1(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw)
{
	__m128i quant_xyzw = _mm_loadu_si128(quantized_value);
	__m128 segment_normalized_xyzw = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(_mm_or_si128(_mm_sll_epi32(quant_xyzw, _mm_set1_epi32(23 - num_bits_at_bit_rate)), k_exponent_bits_xyzw32)), k_one), k_16bit_scale);
	__m128 segment_range_extent_xyzw32 = _mm_sub_ps(_mm_castsi128_ps(_mm_or_si128(_mm_slli_epi32(segment_range_extent_xyzw, 23 - 8), k_exponent_bits_xyzw32)), k_one);
	__m128 segment_range_min_xyzw32 = _mm_sub_ps(_mm_castsi128_ps(_mm_or_si128(_mm_slli_epi32(segment_range_min_xyzw, 23 - 8), k_exponent_bits_xyzw32)), k_one);
	__m128 clip_normalized_xyzw = _mm_add_ps(_mm_mul_ps(segment_normalized_xyzw, segment_range_extent_xyzw32), segment_range_min_xyzw32);
	clip_normalized_xyzw = _mm_mul_ps(clip_normalized_xyzw, k_8bit_scale);
	return _mm_add_ps(_mm_mul_ps(clip_normalized_xyzw, *clip_range_extent_xyzw), *clip_range_min_xyzw);
}

// 1: FixedPoint -> Cast float64 -> normalize -> cvt float32 -> remap signed
ACL_FORCE_NO_INLINE __m128 ACL_VECTOR_CALL decompress_1(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xzyw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value, __m128i* clip_range_extent2, __m128i* clip_range_min2)
{
	__m128i quant_xzyw = _mm_loadu_si128(quantized_value);

	__m128i shift = _mm_set1_epi64x(24 - num_bits_at_bit_rate);
	__m128i segment_normalized_xzyw = _mm_sll_epi32(quant_xzyw, shift);

	__m128i clip_normalized_xzyw = _mm_add_epi32(_mm_mullo_epi32(segment_normalized_xzyw, segment_range_extent_xzyw), segment_range_min_xzyw);
	__m128i clip_normalized_x_y_2 = clip_normalized_xzyw;
	__m128i clip_normalized_z_w_2 = _mm_srli_si128(clip_normalized_xzyw, 4);

	__m128i clip_range_extent_xzyw = _mm_loadu_si128(clip_range_extent2);
	__m128i clip_range_extent_x_y_2 = clip_range_extent_xzyw;
	__m128i clip_range_extent_z_w_2 = _mm_srli_si128(clip_range_extent_xzyw, 4);

	__m128i lossy_fp__x_y2 = _mm_mul_epu32(clip_normalized_x_y_2, clip_range_extent_x_y_2);
	__m128i lossy_fp__z_w2 = _mm_mul_epu32(clip_normalized_z_w_2, clip_range_extent_z_w_2);

	// Coercion to float64 then float32
	__m128i lossy_fp_x_y_ = _mm_srli_epi64(lossy_fp__x_y2, 32);
	__m128i lossy_fp_z_w_ = _mm_srli_epi64(lossy_fp__z_w2, 32);
	__m128i clip_range_min_xzyw = _mm_loadu_si128(clip_range_min2);
	__m128  zero = _mm_setzero_ps();
	__m128i clip_range_min_x_y_ = _mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(clip_range_min_xzyw), zero, 0xA));
	__m128i clip_range_min_z_w_ = _mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(_mm_srli_si128(clip_range_min_xzyw, 4)), zero, 0xA));
	__m128i lossy_x_y_ = _mm_add_epi32(lossy_fp_x_y_, clip_range_min_x_y_);
	__m128i lossy_z_w_ = _mm_add_epi32(lossy_fp_z_w_, clip_range_min_z_w_);
	__m128i lossy_x_y_2 = _mm_or_si128(_mm_slli_epi64(lossy_x_y_, 52 - 32), k_exponent_bits_xyzw64);
	__m128i lossy_z_w_2 = _mm_or_si128(_mm_slli_epi64(lossy_z_w_, 52 - 32), k_exponent_bits_xyzw64);
	__m128d lossy_x_y_64_ = _mm_castsi128_pd(lossy_x_y_2);
	__m128d lossy_z_w_64_ = _mm_castsi128_pd(lossy_z_w_2);

	// Normalize with float64, convert to float32, remap to signed range
	__m128d lossy_x_y_64 = _mm_sub_pd(lossy_x_y_64_, k_dbl_offset);
	__m128d lossy_z_w_64 = _mm_sub_pd(lossy_z_w_64_, k_dbl_offset);
	__m128  lossy_xy__32 = _mm_cvtpd_ps(lossy_x_y_64);
	__m128  lossy_zw__32 = _mm_cvtpd_ps(lossy_z_w_64);
	__m128  lossy_xyzw2 = _mm_shuffle_ps(lossy_xy__32, lossy_zw__32, _MM_SHUFFLE(1, 0, 1, 0));
	return _mm_sub_ps(_mm_mul_ps(lossy_xyzw2, k_two), k_one);
}

// 2: FixedPoint -> Cast float64 -> cvt float32 -> normalize & remap signed
ACL_FORCE_NO_INLINE __m128 ACL_VECTOR_CALL decompress_2(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xzyw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value, __m128i* clip_range_extent2, __m128i* clip_range_min2)
{
	__m128i quant_xzyw = _mm_loadu_si128(quantized_value);

	__m128i shift = _mm_set1_epi64x(24 - num_bits_at_bit_rate);
	__m128i segment_normalized_xzyw = _mm_sll_epi32(quant_xzyw, shift);

	__m128i clip_normalized_xzyw = _mm_add_epi32(_mm_mullo_epi32(segment_normalized_xzyw, segment_range_extent_xzyw), segment_range_min_xzyw);
	__m128i clip_normalized_x_y_2 = clip_normalized_xzyw;
	__m128i clip_normalized_z_w_2 = _mm_srli_si128(clip_normalized_xzyw, 4);

	__m128i clip_range_extent_xzyw = _mm_loadu_si128(clip_range_extent2);
	__m128i clip_range_extent_x_y_2 = clip_range_extent_xzyw;
	__m128i clip_range_extent_z_w_2 = _mm_srli_si128(clip_range_extent_xzyw, 4);

	__m128i lossy_fp__x_y2 = _mm_mul_epu32(clip_normalized_x_y_2, clip_range_extent_x_y_2);
	__m128i lossy_fp__z_w2 = _mm_mul_epu32(clip_normalized_z_w_2, clip_range_extent_z_w_2);

	// Coercion to float64 then float32
	__m128i lossy_fp_x_y_ = _mm_srli_epi64(lossy_fp__x_y2, 32);
	__m128i lossy_fp_z_w_ = _mm_srli_epi64(lossy_fp__z_w2, 32);
	__m128i clip_range_min_xzyw = _mm_loadu_si128(clip_range_min2);
	__m128  zero = _mm_setzero_ps();
	__m128i clip_range_min_x_y_ = _mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(clip_range_min_xzyw), zero, 0xA));
	__m128i clip_range_min_z_w_ = _mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(_mm_srli_si128(clip_range_min_xzyw, 4)), zero, 0xA));
	__m128i lossy_x_y_ = _mm_add_epi32(lossy_fp_x_y_, clip_range_min_x_y_);
	__m128i lossy_z_w_ = _mm_add_epi32(lossy_fp_z_w_, clip_range_min_z_w_);
	__m128i lossy_x_y_2 = _mm_or_si128(_mm_slli_epi64(lossy_x_y_, 52 - 32), k_exponent_bits_xyzw64);
	__m128i lossy_z_w_2 = _mm_or_si128(_mm_slli_epi64(lossy_z_w_, 52 - 32), k_exponent_bits_xyzw64);
	__m128d lossy_x_y_64_ = _mm_castsi128_pd(lossy_x_y_2);
	__m128d lossy_z_w_64_ = _mm_castsi128_pd(lossy_z_w_2);

	// Convert to float32, normalize and remap to signed range
	__m128  lossy_xy__32_ = _mm_cvtpd_ps(lossy_x_y_64_);
	__m128  lossy_zw__32_ = _mm_cvtpd_ps(lossy_z_w_64_);
	__m128  lossy_xyzw2_ = _mm_shuffle_ps(lossy_xy__32_, lossy_zw__32_, _MM_SHUFFLE(1, 0, 1, 0));
	return _mm_sub_ps(_mm_mul_ps(lossy_xyzw2_, k_two), k_three);
}

// 3: FixedPoint -> Cast float32 -> normalize & remap signed
ACL_FORCE_NO_INLINE __m128 ACL_VECTOR_CALL decompress_3(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xzyw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value, __m128i* clip_range_extent2, __m128i* clip_range_min)
{
	__m128i quant_xzyw = _mm_loadu_si128(quantized_value);

	__m128i shift = _mm_set1_epi64x(24 - num_bits_at_bit_rate);
	__m128i segment_normalized_xzyw = _mm_sll_epi32(quant_xzyw, shift);

	__m128i clip_normalized_xzyw = _mm_add_epi32(_mm_mullo_epi32(segment_normalized_xzyw, segment_range_extent_xzyw), segment_range_min_xzyw);
	__m128i clip_normalized_x_y_2 = clip_normalized_xzyw;
	__m128i clip_normalized_z_w_2 = _mm_srli_si128(clip_normalized_xzyw, 4);

	__m128i clip_range_extent_xzyw = _mm_loadu_si128(clip_range_extent2);
	__m128i clip_range_extent_x_y_2 = clip_range_extent_xzyw;
	__m128i clip_range_extent_z_w_2 = _mm_srli_si128(clip_range_extent_xzyw, 4);

	__m128i lossy_fp__x_y2 = _mm_mul_epu32(clip_normalized_x_y_2, clip_range_extent_x_y_2);
	__m128i lossy_fp__z_w2 = _mm_mul_epu32(clip_normalized_z_w_2, clip_range_extent_z_w_2);

	// Hack coercion to float32
	__m128i lossy_fp_xyzw2 = _mm_shuffle_epi32_ab(lossy_fp__x_y2, lossy_fp__z_w2, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i lossy_xyzw = _mm_add_epi32(lossy_fp_xyzw2, *clip_range_min);
	__m128i mantissa_fp_xyzw = _mm_srli_epi32(lossy_xyzw, 32 - 23);	// no rounding, we truncate
	return _mm_sub_ps(_mm_mul_ps(_mm_castsi128_ps(_mm_or_si128(mantissa_fp_xyzw, k_exponent_bits_xyzw32)), k_two), k_three);
}

// In order of most accurate segment only:
//     legacy, hack 4, hack 1, hack 3, hack 6, hack 7, hack 5, hack 2, hack 8

// In order or most accurate (100k samples up to 10 bits):
//     legacy, hack 4, hack 1, hack 6, hack 3, hack 7, hack 5, hack 2, hack 8
// Legacy and hack 4 have equivalent max error

// This is the true value calculated with float64 arithmetic
static float calculate_f32_truth(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, double clip_extent_value, double clip_min_value)
{
	double sample_dbl = double(sample_value) / double((1 << num_value_bits) - 1);
	double segment_extent_dbl = double(segment_extent_value) / double((1 << k_num_segment_value_bits) - 1);
	double segment_min_dbl = double(segment_min_value) / double((1 << k_num_segment_value_bits) - 1);
	double clip_normalized = (sample_dbl * segment_extent_dbl) + segment_min_dbl;
	return float((clip_normalized * clip_extent_value) + clip_min_value);
}

// This is the current legacy implementation
ACL_DEBUG_INLINE static float calculate_f32_legacy(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	float sample_flt = float(sample_value) / VALUE_BITS_MAX[num_value_bits];
	float segment_extent_flt = float(segment_extent_value) / SEGMENT_BITS_MAX;
	float segment_min_flt = float(segment_min_value) / SEGMENT_BITS_MAX;
	float clip_normalized = (sample_flt * segment_extent_flt) + segment_min_flt;
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_legacy_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128 value_bits_max = _mm_load1_ps(&VALUE_BITS_MAX[num_value_bits]);
	__m128 sample_flt = _mm_div_ps(_mm_cvtepi32_ps(sample_value_), value_bits_max);

	__m128 segment_bits_max = _mm_load1_ps(&SEGMENT_BITS_MAX);
	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128 segment_extent_flt = _mm_div_ps(_mm_cvtepi32_ps(segment_extent_value_), segment_bits_max);
	__m128 segment_min_flt = _mm_div_ps(_mm_cvtepi32_ps(segment_min_value_), segment_bits_max);

	__m128 clip_normalized = _mm_add_ps(_mm_mul_ps(sample_flt, segment_extent_flt), segment_min_flt);
	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_legacy_sse_ps(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xyzw = _mm_loadu_si128(quantized_value);
	__m128 value_bits_max = _mm_load1_ps(&VALUE_BITS_MAX[num_bits_at_bit_rate]);
	__m128 sample_flt_xyzw = _mm_div_ps(_mm_cvtepi32_ps(sample_value_xyzw), value_bits_max);

	__m128 segment_bits_max = _mm_load1_ps(&SEGMENT_BITS_MAX);
	__m128 segment_extent_flt_xyzw = _mm_div_ps(_mm_cvtepi32_ps(segment_range_extent_xyzw), segment_bits_max);
	__m128 segment_min_flt_xyzw = _mm_div_ps(_mm_cvtepi32_ps(segment_range_min_xyzw), segment_bits_max);

	__m128 clip_normalized_xyzw = _mm_add_ps(_mm_mul_ps(sample_flt_xyzw, segment_extent_flt_xyzw), segment_min_flt_xyzw);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized_xyzw, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses fast coercion for the sample and segment values and float32 arithmetic to combine everything
ACL_DEBUG_INLINE static float calculate_f32_hack1(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	int32_t sample_i32 = (sample_value << SAMPLE_SHIFT_AMOUNT_23[num_value_bits]) | EXPONENT_BITS;
	int32_t segment_extent_i32 = (segment_extent_value << SEGMENT_SHIFT_AMOUNT) | EXPONENT_BITS;
	int32_t segment_min_i32 = (segment_min_value << SEGMENT_SHIFT_AMOUNT) | EXPONENT_BITS;
	float sample_scale = SAMPLE_SCALE_FLT[num_value_bits];
	float sample_flt = (*reinterpret_cast<float*>(&sample_i32) - 1.0f) * sample_scale;
	// TODO: Maybe use mul/sub with the segment scale? ext = (exti32 * scale) - scale
	float segment_extent_flt = *reinterpret_cast<float*>(&segment_extent_i32) - 1.0f;
	float segment_min_flt = *reinterpret_cast<float*>(&segment_min_i32) - 1.0f;
	float clip_normalized = ((sample_flt * segment_extent_flt) + segment_min_flt) * SEGMENT_SCALE_FLT;
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_hack1_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_23[num_value_bits]);
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i sample_i32 = _mm_or_si128(_mm_sll_epi32(sample_value_, sample_shift_amount), exponent);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i segment_extent_i32 = _mm_or_si128(_mm_slli_epi32(segment_extent_value_, SEGMENT_SHIFT_AMOUNT), exponent);
	__m128i segment_min_i32 = _mm_or_si128(_mm_slli_epi32(segment_min_value_, SEGMENT_SHIFT_AMOUNT), exponent);

	__m128 sample_scale = _mm_load1_ps(&SAMPLE_SCALE_FLT[num_value_bits]);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 one = _mm_load1_ps(&ONE);

	__m128 sample_flt = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(sample_i32), one), sample_scale);
	__m128 segment_extent_flt = _mm_sub_ps(_mm_castsi128_ps(segment_extent_i32), one);
	__m128 segment_min_flt = _mm_sub_ps(_mm_castsi128_ps(segment_min_i32), one);

	__m128 clip_normalized = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(sample_flt, segment_extent_flt), segment_min_flt), segment_scale);
	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack1_sse_ps(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xyzw = _mm_loadu_si128(quantized_value);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_23[num_bits_at_bit_rate]);
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i sample_i32 = _mm_or_si128(_mm_sll_epi32(sample_value_xyzw, sample_shift_amount), exponent);

	__m128i segment_extent_i32 = _mm_or_si128(_mm_slli_epi32(segment_range_extent_xyzw, SEGMENT_SHIFT_AMOUNT), exponent);
	__m128i segment_min_i32 = _mm_or_si128(_mm_slli_epi32(segment_range_min_xyzw, SEGMENT_SHIFT_AMOUNT), exponent);

	__m128 sample_scale = _mm_load1_ps(&SAMPLE_SCALE_FLT[num_bits_at_bit_rate]);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 one = _mm_load1_ps(&ONE);

	__m128 sample_flt = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(sample_i32), one), sample_scale);
	__m128 segment_extent_flt = _mm_sub_ps(_mm_castsi128_ps(segment_extent_i32), one);
	__m128 segment_min_flt = _mm_sub_ps(_mm_castsi128_ps(segment_min_i32), one);

	__m128 clip_normalized = _mm_mul_ps(_mm_add_ps(_mm_mul_ps(sample_flt, segment_extent_flt), segment_min_flt), segment_scale);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses 32 bit fixed point arithmetic to perform segment range expansion and float32 arithmetic for clip range expansion
ACL_DEBUG_INLINE static float calculate_f32_hack2(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	// Due to rounding, some integral parts are never used and always 0, re-use those bits!
	// (1.0 << (N + 16)) / N.0 = 17.0 | 1.16
	uint32_t sample_scale_i32 = SAMPLE_SCALE_I17[num_value_bits];
	ACL_ENSURE(sample_scale_i32 > (1 << 16), "Must be >= 1.0!");
	uint32_t scaled_sample_i32 = (sample_value << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i32;	// 0.16 * 1.16 = 0.32	(integral part always 0)
	ACL_ENSURE((((uint64_t(sample_value) << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i32) & (uint64_t(1) << 32)) == 0, "Integer bit used!");

	uint32_t scaled_range_i32 = (scaled_sample_i32 >> 8) * segment_extent_value;				// 0.24 * 0.8 = 0.32
	uint32_t unnormalized_i32 = scaled_range_i32 + (segment_min_value << 24);					// 0.32 + 0.32 = 0.32

																								// (1.0 << (8 + 8)) / 8.0 = 9.0 | 1.8
	uint32_t segment_scale_i32 = SEGMENT_SCALE_I9;
	ACL_ENSURE(segment_scale_i32 > (1 << 8), "Must be >= 1.0!");
	uint32_t normalized_i32 = (unnormalized_i32 >> 8) * segment_scale_i32;						// 0.24 * 1.8 = 0.32	(integral part always 0)
	ACL_ENSURE((((uint64_t(unnormalized_i32) >> 8) * segment_scale_i32) & (uint64_t(1) << 32)) == 0, "Integer bit used!");

	uint32_t result_mantissa_i32 = normalized_i32 >> 9;											// 0.32 >> 9 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float clip_normalized = (*reinterpret_cast<float*>(&result_i32) - 1.0f);
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_hack2_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I17[num_value_bits]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_value_bits]);
	__m128i scaled_sample_i32 = _mm_mullo_epi32(_mm_sll_epi32(sample_value_, sample_shift_amount), sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i scaled_range_i32 = _mm_mullo_epi32(_mm_srli_epi32(scaled_sample_i32, 8), segment_extent_value_);
	__m128i unnormalized_i32 = _mm_add_epi32(scaled_range_i32, _mm_slli_epi32(segment_min_value_, 24));

	__m128i segment_scale_i32 = _mm_broadcast_epi32(&SEGMENT_SCALE_I9);
	__m128i normalized_i32 = _mm_mullo_epi32(_mm_srli_epi32(unnormalized_i32, 8), segment_scale_i32);

	__m128i clip_normalized_mantissa_i32 = _mm_srli_epi32(normalized_i32, 9);
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);

	__m128 one = _mm_load1_ps(&ONE);
	__m128 clip_normalized = _mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one);
	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack2_sse_ps(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xyzw = _mm_loadu_si128(quantized_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I17[num_bits_at_bit_rate]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_bits_at_bit_rate]);
	__m128i scaled_sample_i32 = _mm_mullo_epi32(_mm_sll_epi32(sample_value_xyzw, sample_shift_amount), sample_scale_i32);

	__m128i scaled_range_i32 = _mm_mullo_epi32(_mm_srli_epi32(scaled_sample_i32, 8), segment_range_extent_xyzw);
	__m128i unnormalized_i32 = _mm_add_epi32(scaled_range_i32, _mm_slli_epi32(segment_range_min_xyzw, 24));

	__m128i segment_scale_i32 = _mm_broadcast_epi32(&SEGMENT_SCALE_I9);
	__m128i normalized_i32 = _mm_mullo_epi32(_mm_srli_epi32(unnormalized_i32, 8), segment_scale_i32);

	__m128i clip_normalized_mantissa_i32 = _mm_srli_epi32(normalized_i32, 9);
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);

	__m128 one = _mm_load1_ps(&ONE);
	__m128 clip_normalized = _mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses a mix of 64 and 32 bit fixed point arithmetic to perform segment range expansion and float32 arithmetic for clip range expansion
ACL_DEBUG_INLINE static float calculate_f32_hack3(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	// Due to rounding, some integral parts are never used and always 0, re-use those bits!
	// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
	uint64_t sample_scale_i64 = ((uint64_t(1) << num_value_bits) << 31) / ((uint64_t(1) << num_value_bits) - 1);
	ACL_ENSURE(sample_scale_i64 > (uint64_t(1) << 31), "Must be >= 1.0!");
	uint64_t scaled_sample_i64 = (sample_value << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i64;	// 0.16 * 1.31 = 0.47	(integral part always 0)
	ACL_ENSURE((scaled_sample_i64 & (uint64_t(1) << 47)) == 0, "Integer bit used!");

	// (1.0 << (8 + 24)) / 8.0 = 24.0 | 1.24
	uint32_t segment_scale_i32 = SEGMENT_SCALE_I25;
	ACL_ENSURE(segment_scale_i32 > (1 << 24), "Must be >= 1.0!");
	uint64_t scaled_extent_i64 = segment_extent_value * segment_scale_i32;						// 0.8 * 1.24 = 0.32	(integral part always 0)
	uint32_t scaled_min_i32 = segment_min_value * segment_scale_i32;							// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_extent_i64 & (uint64_t(1) << 32)) == 0, "Integer bit used!");
	ACL_ENSURE(((uint64_t(segment_min_value) * segment_scale_i32) & (uint64_t(1) << 32)) == 0, "Integer bit used!");

	uint64_t scaled_range_i64 = (scaled_sample_i64 >> 15) * scaled_extent_i64;					// 0.32 * 0.32 = 0.64
	uint32_t result_mantissa_i32 = uint32_t(scaled_range_i64 >> 41) + (scaled_min_i32 >> 9);	// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float clip_normalized = (*reinterpret_cast<float*>(&result_i32) - 1.0f);
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_hack3_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I32[num_value_bits]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_value_bits]);
	__m128i shifted_sample_value = _mm_sll_epi32(sample_value_, sample_shift_amount);
	__m128i scaled_sample_i64 = _mm_mul_epu32(shifted_sample_value, sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i segment_scale_i32 = _mm_broadcast_epi32(&SEGMENT_SCALE_I25);
	__m128i scaled_extent_i32 = _mm_mullo_epi32(segment_extent_value_, segment_scale_i32);
	__m128i scaled_min_i32 = _mm_mullo_epi32(segment_min_value_, segment_scale_i32);

	__m128i scaled_range_i64 = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_i64, 15), scaled_extent_i32);
	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(_mm_srli_epi64(scaled_range_i64, 41), _mm_srli_epi32(scaled_min_i32, 9));
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);

	__m128 one = _mm_load1_ps(&ONE);
	__m128 clip_normalized = _mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one);
	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack3_sse_ps(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xzyw = _mm_loadu_si128(quantized_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I32[num_bits_at_bit_rate]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_bits_at_bit_rate]);
	__m128i shifted_sample_value_xzyw = _mm_sll_epi32(sample_value_xzyw, sample_shift_amount);

	__m128i shifted_sample_value_x_y_ = shifted_sample_value_xzyw;
	__m128i shifted_sample_value_z_w_ = _mm_shuffle_epi32_ab(shifted_sample_value_xzyw, shifted_sample_value_xzyw, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_sample_xlohi_ylohi = _mm_mul_epu32(shifted_sample_value_x_y_, sample_scale_i32);
	__m128i scaled_sample_zlohi_wlohi = _mm_mul_epu32(shifted_sample_value_z_w_, sample_scale_i32);

	__m128i segment_scale_i32 = _mm_broadcast_epi32(&SEGMENT_SCALE_I25);
	__m128i scaled_extent_xzyw = _mm_mullo_epi32(segment_range_extent_xzyw, segment_scale_i32);
	__m128i scaled_extent_x_y_ = scaled_extent_xzyw;
	__m128i scaled_extent_z_w_ = _mm_shuffle_epi32_ab(scaled_extent_xzyw, scaled_extent_xzyw, _MM_SHUFFLE(3, 1, 3, 1));;
	__m128i scaled_min_xyzw = _mm_mullo_epi32(segment_range_min_xyzw, segment_scale_i32);

	__m128i scaled_range_xlohi_ylohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_xlohi_ylohi, 15), scaled_extent_x_y_);
	__m128i scaled_range_zlohi_wlohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_zlohi_wlohi, 15), scaled_extent_z_w_);
	__m128i scaled_range_x_y_ = _mm_srli_epi64(scaled_range_xlohi_ylohi, 41);
	__m128i scaled_range_z_w_ = _mm_srli_epi64(scaled_range_zlohi_wlohi, 41);
	__m128i scaled_range_xyzw = _mm_shuffle_epi32_ab(scaled_range_x_y_, scaled_range_z_w_, _MM_SHUFFLE(2, 0, 2, 0));

	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(scaled_range_xyzw, _mm_srli_epi32(scaled_min_xyzw, 9));
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);

	__m128 one = _mm_load1_ps(&ONE);
	__m128 clip_normalized = _mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses a mix of 64 and 32 bit fixed point arithmetic to perform segment range expansion but applies the normalization scale with float32 arithmetic and uses float32 for clip range expansion
ACL_DEBUG_INLINE static float calculate_f32_hack4(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	// Due to rounding, some integral parts are never used and always 0, re-use those bits!
	// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
	uint64_t sample_scale_i64 = ((uint64_t(1) << num_value_bits) << 31) / ((uint64_t(1) << num_value_bits) - 1);
	ACL_ENSURE(sample_scale_i64 > (uint64_t(1) << 31), "Must be >= 1.0!");
	uint64_t scaled_sample_i64 = (sample_value << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i64;		// 0.16 * 1.31 = 0.47	(integral part always 0)
	ACL_ENSURE((scaled_sample_i64 & (uint64_t(1) << 47)) == 0, "Integer bit used!");

	uint64_t scaled_range_i64 = (scaled_sample_i64 >> 15) * segment_extent_value;					// 0.32 * 0.8 = 0.40
	uint32_t result_mantissa_i32 = uint32_t(scaled_range_i64 >> 17) + (segment_min_value << 15);	// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float clip_normalized = (*reinterpret_cast<float*>(&result_i32) - 1.0f) * SEGMENT_SCALE_FLT;
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_hack4_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I32[num_value_bits]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_value_bits]);
	__m128i shifted_sample_value = _mm_sll_epi32(sample_value_, sample_shift_amount);
	__m128i scaled_sample_i64 = _mm_mul_epu32(shifted_sample_value, sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i scaled_range_i64 = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_i64, 15), segment_extent_value_);

	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(_mm_srli_epi64(scaled_range_i64, 17), _mm_slli_epi32(segment_min_value_, 15));
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 clip_normalized = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one), segment_scale);

	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack4_sse_ps(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xzyw = _mm_loadu_si128(quantized_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I32[num_bits_at_bit_rate]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_bits_at_bit_rate]);
	__m128i shifted_sample_value_xzyw = _mm_sll_epi32(sample_value_xzyw, sample_shift_amount);

	__m128i shifted_sample_value_x_y_ = shifted_sample_value_xzyw;
	__m128i shifted_sample_value_z_w_ = _mm_shuffle_epi32_ab(shifted_sample_value_xzyw, shifted_sample_value_xzyw, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_sample_xlohi_ylohi = _mm_mul_epu32(shifted_sample_value_x_y_, sample_scale_i32);
	__m128i scaled_sample_zlohi_wlohi = _mm_mul_epu32(shifted_sample_value_z_w_, sample_scale_i32);

	__m128i segment_range_extent_x_y_ = segment_range_extent_xzyw;
	__m128i segment_range_extent_z_w_ = _mm_shuffle_epi32_ab(segment_range_extent_xzyw, segment_range_extent_xzyw, _MM_SHUFFLE(3, 1, 3, 1));;

	__m128i scaled_range_xlohi_ylohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_xlohi_ylohi, 15), segment_range_extent_x_y_);
	__m128i scaled_range_zlohi_wlohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_zlohi_wlohi, 15), segment_range_extent_z_w_);
	__m128i scaled_range_x_y_ = _mm_srli_epi64(scaled_range_xlohi_ylohi, 17);
	__m128i scaled_range_z_w_ = _mm_srli_epi64(scaled_range_zlohi_wlohi, 17);
	__m128i scaled_range_xyzw = _mm_shuffle_epi32_ab(scaled_range_x_y_, scaled_range_z_w_, _MM_SHUFFLE(2, 0, 2, 0));

	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(scaled_range_xyzw, _mm_slli_epi32(segment_range_min_xyzw, 15));
	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 clip_normalized = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one), segment_scale);

	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses 32 bit fixed point arithmetic to perform segment range expansion but applies the normalization scale with float32 arithmetic and uses float32 for clip range expansion
ACL_DEBUG_INLINE static float calculate_f32_hack5(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	// Due to rounding, some integral parts are never used and always 0, re-use those bits!
	// (1.0 << (N + 16)) / N.0 = 17.0 | 1.16
	uint32_t sample_scale_i32 = SAMPLE_SCALE_I17[num_value_bits];
	ACL_ENSURE(sample_scale_i32 > (1 << 16), "Must be >= 1.0!");
	uint32_t scaled_sample_i32 = (sample_value << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i32;		// 0.16 * 1.16 = 0.32	(integral part always 0)
	ACL_ENSURE((((uint64_t(sample_value) << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i32) & (uint64_t(1) << 32)) == 0, "Integer bit used!");

	uint32_t scaled_range_i32 = (scaled_sample_i32 >> 8) * segment_extent_value;					// 0.24 * 0.8 = 0.32
	uint32_t result_mantissa_i32 = (scaled_range_i32 >> 9) + (segment_min_value << 15);				// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float clip_normalized = (*reinterpret_cast<float*>(&result_i32) - 1.0f) * SEGMENT_SCALE_FLT;
	return (clip_normalized * clip_extent_value) + clip_min_value;
}

ACL_DEBUG_INLINE static float calculate_f32_hack5_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, float clip_extent_value, float clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I17[num_value_bits]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_value_bits]);
	__m128i scaled_sample_i32 = _mm_mullo_epi32(_mm_sll_epi32(sample_value_, sample_shift_amount), sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i scaled_range_i32 = _mm_mullo_epi32(_mm_srli_epi32(scaled_sample_i32, 8), segment_extent_value_);
	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(_mm_srli_epi32(scaled_range_i32, 9), _mm_slli_epi32(segment_min_value_, 15));

	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 clip_normalized = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one), segment_scale);

	__m128 clip_extent_value_ = _mm_set1_ps(clip_extent_value);
	__m128 clip_min_value_ = _mm_set1_ps(clip_min_value);
	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, clip_extent_value_), clip_min_value_);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack5_sse_ps(__m128i segment_range_extent_xyzw, __m128i segment_range_min_xyzw, __m128* clip_range_extent_xyzw, __m128* clip_range_min_xyzw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xyzw = _mm_loadu_si128(quantized_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I17[num_bits_at_bit_rate]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_bits_at_bit_rate]);
	__m128i scaled_sample_i32 = _mm_mullo_epi32(_mm_sll_epi32(sample_value_xyzw, sample_shift_amount), sample_scale_i32);

	__m128i scaled_range_i32 = _mm_mullo_epi32(_mm_srli_epi32(scaled_sample_i32, 8), segment_range_extent_xyzw);
	__m128i clip_normalized_mantissa_i32 = _mm_add_epi32(_mm_srli_epi32(scaled_range_i32, 9), _mm_slli_epi32(segment_range_min_xyzw, 15));

	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i clip_normalized_i32 = _mm_or_si128(clip_normalized_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 segment_scale = _mm_load1_ps(&SEGMENT_SCALE_FLT);
	__m128 clip_normalized = _mm_mul_ps(_mm_sub_ps(_mm_castsi128_ps(clip_normalized_i32), one), segment_scale);

	__m128 result = _mm_add_ps(_mm_mul_ps(clip_normalized, *clip_range_extent_xyzw), *clip_range_min_xyzw);
	return result;
}

// This uses a mix of 64 and 32 bit fixed point arithmetic to perform segment and clip range expansion, clip range on 32 bit
ACL_DEBUG_INLINE static float calculate_f32_hack6(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, uint32_t clip_extent_value, uint32_t clip_min_value)
{
	// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
	uint64_t sample_scale_i64 = ((uint64_t(1) << num_value_bits) << 31) / ((uint64_t(1) << num_value_bits) - 1);
	ACL_ENSURE(sample_scale_i64 > (uint64_t(1) << 31), "Must be >= 1.0!");
	uint64_t scaled_sample_i64 = (sample_value << SAMPLE_SHIFT_AMOUNT_16[num_value_bits]) * sample_scale_i64;						// 0.16 * 1.31 = 0.47	(integral part always 0)
	ACL_ENSURE((scaled_sample_i64 & (uint64_t(1) << 47)) == 0, "Integer bit used!");

	// (1.0 << (8 + 24)) / 8.0 = 25.0 | 1.24
	uint32_t segment_scale_i32 = SEGMENT_SCALE_I25;
	ACL_ENSURE(segment_scale_i32 > (1 << 24), "Must be >= 1.0!");
	uint64_t scaled_segment_extent_i64 = segment_extent_value * segment_scale_i32;									// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_extent_i64 & (uint64_t(1) << 32)) == 0, "Integer bit used!");
	uint64_t scaled_segment_min_i64 = segment_min_value * segment_scale_i32;										// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_min_i64 & (uint64_t(1) << 32)) == 0, "Integer bit used!");

	uint64_t scaled_segment_range_i64 = (scaled_sample_i64 >> 15) * scaled_segment_extent_i64;						// 0.32 * 0.32 = 0.64
	uint64_t clip_normalized_i64 = scaled_segment_range_i64 + (scaled_segment_min_i64 << 32);						// 0.64

	// (1.0 << (32 + 31)) / 32.0 = 32.0 | 1.31
	uint64_t clip_scale_i64 = (uint64_t(1) << 63) / ((uint64_t(1) << 32) - 1);										// 1.31
	ACL_ENSURE(clip_scale_i64 == (uint64_t(1) << 31), "Must be == 1.0!");	// :( not necessary, cannot scale higher
	uint64_t scaled_clip_extent_i64 = clip_extent_value * clip_scale_i64;											// 0.32 * 1.31 = 0.63	(integral part always 0)
	ACL_ENSURE((scaled_clip_extent_i64 & (1ull << 63)) == 0, "Integer bit used!");
	uint64_t scaled_clip_min_i64 = clip_min_value * clip_scale_i64;													// 0.32 * 1.31 = 0.63	(integral part always 0)
	ACL_ENSURE((scaled_clip_min_i64 & (1ull << 63)) == 0, "Integer bit used!");

	uint64_t scaled_clip_range_i64 = (clip_normalized_i64 >> 32) * (scaled_clip_extent_i64 >> 31);					// 0.32 * 0.32 = 0.64
	uint32_t result_mantissa_i32 = uint32_t(scaled_clip_range_i64 >> 41) + uint32_t(scaled_clip_min_i64 >> 40);		// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float result_remapped = (*reinterpret_cast<float*>(&result_i32) - 1.0f);
	return (result_remapped * 2.0f) - 1.0f;
}

ACL_DEBUG_INLINE static float calculate_f32_hack6_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, uint32_t clip_extent_value, uint32_t clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_set1_epi32(SAMPLE_SCALE_I32[num_value_bits]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_value_bits]);
	__m128i shifted_sample_value = _mm_sll_epi32(sample_value_, sample_shift_amount);
	__m128i scaled_sample_i64 = _mm_mul_epu32(shifted_sample_value, sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i segment_scale_i32 = _mm_set1_epi32(SEGMENT_SCALE_I25);
	__m128i scaled_segment_extent_i64 = _mm_mullo_epi32(segment_extent_value_, segment_scale_i32);
	__m128i scaled_segment_min_i64 = _mm_mullo_epi32(segment_min_value_, segment_scale_i32);

	__m128i scaled_segment_range_i64 = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_i64, 15), scaled_segment_extent_i64);
	__m128i clip_normalized_i64 = _mm_add_epi64(scaled_segment_range_i64, _mm_slli_epi64(scaled_segment_min_i64, 32));

	__m128i clip_scale_i32 = _mm_set1_epi32(CLIP_SCALE_I32);
	__m128i clip_extent_value_ = _mm_set1_epi32(clip_extent_value);
	__m128i clip_min_value_ = _mm_set1_epi32(clip_min_value);
	__m128i scaled_clip_extent_i64 = _mm_mul_epu32(clip_extent_value_, clip_scale_i32);
	__m128i scaled_clip_min_i64 = _mm_mul_epu32(clip_min_value_, clip_scale_i32);

	__m128i scaled_clip_range_i64 = _mm_mul_epu32(_mm_srli_epi64(clip_normalized_i64, 32), _mm_srli_epi64(scaled_clip_extent_i64, 31));
	__m128i result_mantissa_i32 = _mm_add_epi32(_mm_srli_epi64(scaled_clip_range_i64, 41), _mm_srli_epi64(scaled_clip_min_i64, 40));

	__m128i exponent = _mm_set1_epi32(EXPONENT_BITS);
	__m128i result_i32 = _mm_or_si128(result_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 two = _mm_load1_ps(&TWO);
	__m128 result_remapped = _mm_sub_ps(_mm_castsi128_ps(result_i32), one);
	__m128 result = _mm_sub_ps(_mm_mul_ps(result_remapped, two), one);
	return _mm_cvtss_f32(result);
}

ACL_FORCE_NO_INLINE static __m128 ACL_VECTOR_CALL calculate_f32_hack6_sse_ps(__m128i segment_range_extent_xzyw, __m128i segment_range_min_xzyw, __m128i* clip_range_extent_xzyw, __m128i* clip_range_min_xzyw, uint8_t num_bits_at_bit_rate, __m128i* quantized_value)
{
	__m128i sample_value_xzyw = _mm_loadu_si128(quantized_value);
	__m128i sample_scale_i32 = _mm_broadcast_epi32(&SAMPLE_SCALE_I32[num_bits_at_bit_rate]);
	__m128i sample_shift_amount = _mm_load_epi32(&SAMPLE_SHIFT_AMOUNT_16[num_bits_at_bit_rate]);
	__m128i shifted_sample_value_xzyw = _mm_sll_epi32(sample_value_xzyw, sample_shift_amount);

	__m128i shifted_sample_value_x_y_ = shifted_sample_value_xzyw;
	__m128i shifted_sample_value_z_w_ = _mm_shuffle_epi32_ab(shifted_sample_value_xzyw, shifted_sample_value_xzyw, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_sample_xlohi_ylohi = _mm_mul_epu32(shifted_sample_value_x_y_, sample_scale_i32);
	__m128i scaled_sample_zlohi_wlohi = _mm_mul_epu32(shifted_sample_value_z_w_, sample_scale_i32);

	__m128i segment_scale_i32 = _mm_broadcast_epi32(&SEGMENT_SCALE_I25);
	__m128i scaled_segment_extent_xzyw = _mm_mullo_epi32(segment_range_extent_xzyw, segment_scale_i32);
	__m128i scaled_segment_extent_x_y_ = scaled_segment_extent_xzyw;
	__m128i scaled_segment_extent_z_w_ = _mm_shuffle_epi32_ab(scaled_segment_extent_xzyw, scaled_segment_extent_xzyw, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_segment_min_xzyw = _mm_mullo_epi32(segment_range_min_xzyw, segment_scale_i32);
	__m128i scaled_segment_min_x_y_ = scaled_segment_min_xzyw;
	__m128i scaled_segment_min_z_w_ = _mm_shuffle_epi32_ab(scaled_segment_min_xzyw, scaled_segment_min_xzyw, _MM_SHUFFLE(3, 1, 3, 1));

	__m128i scaled_segment_range_xlohi_ylohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_xlohi_ylohi, 15), scaled_segment_extent_x_y_);
	__m128i scaled_segment_range_zlohi_wlohi = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_zlohi_wlohi, 15), scaled_segment_extent_z_w_);
	__m128i clip_normalized_xlohi_ylohi = _mm_add_epi64(scaled_segment_range_xlohi_ylohi, _mm_slli_epi64(scaled_segment_min_x_y_, 32));
	__m128i clip_normalized_zlohi_wlohi = _mm_add_epi64(scaled_segment_range_zlohi_wlohi, _mm_slli_epi64(scaled_segment_min_z_w_, 32));

	__m128i clip_scale_i32 = _mm_broadcast_epi32(&CLIP_SCALE_I32);
	__m128i clip_range_extent_x_y_ = *clip_range_extent_xzyw;
	__m128i clip_range_extent_z_w_ = _mm_shuffle_epi32_ab(clip_range_extent_x_y_, clip_range_extent_x_y_, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_clip_extent_xlohi_ylohi = _mm_mul_epu32(clip_range_extent_x_y_, clip_scale_i32);
	__m128i scaled_clip_extent_zlohi_wlohi = _mm_mul_epu32(clip_range_extent_z_w_, clip_scale_i32);
	__m128i clip_range_min_x_y_ = *clip_range_min_xzyw;
	__m128i clip_range_min_z_w_ = _mm_shuffle_epi32_ab(clip_range_min_x_y_, clip_range_min_x_y_, _MM_SHUFFLE(3, 1, 3, 1));
	__m128i scaled_clip_min_xlohi_ylohi = _mm_mul_epu32(clip_range_min_x_y_, clip_scale_i32);
	__m128i scaled_clip_min_zlohi_wlohi = _mm_mul_epu32(clip_range_min_z_w_, clip_scale_i32);

	__m128i scaled_clip_range_xlohi_ylohi = _mm_mul_epu32(_mm_srli_epi64(clip_normalized_xlohi_ylohi, 32), _mm_srli_epi64(scaled_clip_extent_xlohi_ylohi, 31));
	__m128i scaled_clip_range_zlohi_wlohi = _mm_mul_epu32(_mm_srli_epi64(clip_normalized_zlohi_wlohi, 32), _mm_srli_epi64(scaled_clip_extent_zlohi_wlohi, 31));
	__m128i scaled_clip_range_x_y_ = _mm_srli_epi64(scaled_clip_range_xlohi_ylohi, 41);
	__m128i scaled_clip_range_z_w_ = _mm_srli_epi64(scaled_clip_range_zlohi_wlohi, 41);
	__m128i scaled_clip_min_x_y_ = _mm_srli_epi64(scaled_clip_min_xlohi_ylohi, 40);
	__m128i scaled_clip_min_z_w_ = _mm_srli_epi64(scaled_clip_min_zlohi_wlohi, 40);
	__m128i scaled_clip_range_xyzw = _mm_shuffle_epi32_ab(scaled_clip_range_x_y_, scaled_clip_range_z_w_, _MM_SHUFFLE(2, 0, 2, 0));
	__m128i scaled_clip_min_xyzw = _mm_shuffle_epi32_ab(scaled_clip_min_x_y_, scaled_clip_min_z_w_, _MM_SHUFFLE(2, 0, 2, 0));
	__m128i result_mantissa_i32 = _mm_add_epi32(scaled_clip_range_xyzw, scaled_clip_min_xyzw);

	__m128i exponent = _mm_broadcast_epi32(&EXPONENT_BITS);
	__m128i result_i32 = _mm_or_si128(result_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 two = _mm_load1_ps(&TWO);
	__m128 result_remapped = _mm_sub_ps(_mm_castsi128_ps(result_i32), one);
	__m128 result = _mm_sub_ps(_mm_mul_ps(result_remapped, two), one);
	return result;
}

// This uses a mix of 64 and 32 bit fixed point arithmetic to perform segment and clip range expansion, clip range on 24 bit
ACL_DEBUG_INLINE static float calculate_f32_hack7(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, uint32_t clip_extent_value, uint32_t clip_min_value)
{
	// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
	uint64_t sample_scale_i64 = ((uint64_t(1) << num_value_bits) << 31) / ((uint64_t(1) << num_value_bits) - 1);
	ACL_ENSURE(sample_scale_i64 > (uint64_t(1) << 31), "Must be >= 1.0!");
	uint64_t scaled_sample_i64 = (sample_value << (16 - num_value_bits)) * sample_scale_i64;						// 0.16 * 1.31 = 0.47	(integral part always 0)
	ACL_ENSURE((scaled_sample_i64 & (1ull << 47)) == 0, "Integer bit used!");

	// (1.0 << (8 + 24)) / 8.0 = 25.0 | 1.24
	uint32_t segment_scale_i32 = SEGMENT_SCALE_I25;
	ACL_ENSURE(segment_scale_i32 > (1 << 24), "Must be >= 1.0!");
	uint64_t scaled_segment_extent_i64 = segment_extent_value * segment_scale_i32;									// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_extent_i64 & (1ull << 32)) == 0, "Integer bit used!");
	uint64_t scaled_segment_min_i64 = segment_min_value * segment_scale_i32;										// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_min_i64 & (1ull << 32)) == 0, "Integer bit used!");

	uint64_t scaled_segment_range_i64 = (scaled_sample_i64 >> 15) * scaled_segment_extent_i64;						// 0.32 * 0.32 = 0.64
	uint64_t clip_normalized_i64 = scaled_segment_range_i64 + (scaled_segment_min_i64 << 32);						// 0.64

																													// (1.0 << (32 + 31)) / 32.0 = 32.0 | 1.31
	uint64_t clip_scale_i64 = (uint64_t(1) << 63) / ((uint64_t(1) << 32) - 1);										// 1.31
	ACL_ENSURE(clip_scale_i64 == (uint64_t(1) << 31), "Must be == 1.0!");	// :( not necessary, cannot scale higher
	uint64_t scaled_clip_extent_i64 = clip_extent_value * clip_scale_i64;											// 0.24 * 1.31 = 0.55	(integral part always 0)
	ACL_ENSURE((scaled_clip_extent_i64 & (1ull << 55)) == 0, "Integer bit used!");
	uint64_t scaled_clip_min_i64 = clip_min_value * clip_scale_i64;													// 0.24 * 1.31 = 0.55	(integral part always 0)
	ACL_ENSURE((scaled_clip_min_i64 & (1ull << 55)) == 0, "Integer bit used!");

	uint64_t scaled_clip_range_i64 = (clip_normalized_i64 >> 32) * (scaled_clip_extent_i64 >> 23);					// 0.32 * 0.32 = 0.64
	uint32_t result_mantissa_i32 = uint32_t(scaled_clip_range_i64 >> 41) + uint32_t(scaled_clip_min_i64 >> 32);		// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float result_remapped = (*reinterpret_cast<float*>(&result_i32) - 1.0f);
	return (result_remapped * 2.0f) - 1.0f;
}

ACL_DEBUG_INLINE static float calculate_f32_hack7_sse_ss(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, uint32_t clip_extent_value, uint32_t clip_min_value)
{
	__m128i sample_value_ = _mm_set1_epi32(sample_value);
	__m128i sample_scale_i32 = _mm_set1_epi32(SAMPLE_SCALE_I32[num_value_bits]);
	__m128i sample_shift_amount = _mm_set1_epi64x(16 - num_value_bits);
	__m128i shifted_sample_value = _mm_sll_epi32(sample_value_, sample_shift_amount);
	__m128i scaled_sample_i64 = _mm_mul_epu32(shifted_sample_value, sample_scale_i32);

	__m128i segment_extent_value_ = _mm_set1_epi32(segment_extent_value);
	__m128i segment_min_value_ = _mm_set1_epi32(segment_min_value);
	__m128i segment_scale_i32 = _mm_set1_epi32(SEGMENT_SCALE_I25);
	__m128i scaled_segment_extent_i64 = _mm_mullo_epi32(segment_extent_value_, segment_scale_i32);
	__m128i scaled_segment_min_i64 = _mm_mullo_epi32(segment_min_value_, segment_scale_i32);

	__m128i scaled_segment_range_i64 = _mm_mul_epu32(_mm_srli_epi64(scaled_sample_i64, 15), scaled_segment_extent_i64);
	__m128i clip_normalized_i64 = _mm_add_epi64(scaled_segment_range_i64, _mm_slli_epi64(scaled_segment_min_i64, 32));

	__m128i clip_scale_i64 = _mm_set1_epi32(CLIP_SCALE_I32);
	__m128i clip_extent_value_ = _mm_set1_epi32(clip_extent_value);
	__m128i clip_min_value_ = _mm_set1_epi32(clip_min_value);
	__m128i scaled_clip_extent_i64 = _mm_mul_epu32(clip_extent_value_, clip_scale_i64);
	__m128i scaled_clip_min_i64 = _mm_mul_epu32(clip_min_value_, clip_scale_i64);

	__m128i scaled_clip_range_i64 = _mm_mul_epu32(_mm_srli_epi64(clip_normalized_i64, 32), _mm_srli_epi64(scaled_clip_extent_i64, 23));
	__m128i result_mantissa_i32 = _mm_add_epi32(_mm_srli_epi64(scaled_clip_range_i64, 41), _mm_srli_epi64(scaled_clip_min_i64, 32));

	__m128i exponent = _mm_set1_epi32(EXPONENT_BITS);
	__m128i result_i32 = _mm_or_si128(result_mantissa_i32, exponent);
	__m128 one = _mm_load1_ps(&ONE);
	__m128 two = _mm_load1_ps(&TWO);
	__m128 result_remapped = _mm_sub_ps(_mm_castsi128_ps(result_i32), one);
	__m128 result = _mm_sub_ps(_mm_mul_ps(result_remapped, two), one);
	return _mm_cvtss_f32(result);
}

// This uses a mix of 64 and 32 bit fixed point arithmetic to perform segment and clip range expansion, clip range min on 8 bit, clip range extent on 24 bit
ACL_DEBUG_INLINE static float calculate_f32_hack8(uint32_t sample_value, uint32_t num_value_bits, uint32_t segment_extent_value, uint32_t segment_min_value, uint32_t clip_extent_value, uint32_t clip_min_value)
{
	// (1.0 << (N + 31)) / N.0 = 32.0 | 1.31
	uint64_t sample_scale_i64 = ((uint64_t(1) << num_value_bits) << 31) / ((uint64_t(1) << num_value_bits) - 1);
	ACL_ENSURE(sample_scale_i64 > (uint64_t(1) << 31), "Must be >= 1.0!");
	uint64_t scaled_sample_i64 = (sample_value << (16 - num_value_bits)) * sample_scale_i64;						// 0.16 * 1.31 = 0.47	(integral part always 0)
	ACL_ENSURE((scaled_sample_i64 & (1ull << 47)) == 0, "Integer bit used!");

	// (1.0 << (8 + 24)) / 8.0 = 25.0 | 1.24
	uint32_t segment_scale_i32 = uint32_t(((uint64_t(1) << k_num_segment_value_bits) << 24) / ((uint64_t(1) << k_num_segment_value_bits) - 1));
	ACL_ENSURE(segment_scale_i32 > (1 << 24), "Must be >= 1.0!");
	uint64_t scaled_segment_extent_i64 = segment_extent_value * segment_scale_i32;									// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_extent_i64 & (1ull << 32)) == 0, "Integer bit used!");
	uint64_t scaled_segment_min_i64 = segment_min_value * segment_scale_i32;										// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_segment_min_i64 & (1ull << 32)) == 0, "Integer bit used!");

	uint64_t scaled_segment_range_i64 = (scaled_sample_i64 >> 15) * scaled_segment_extent_i64;						// 0.32 * 0.32 = 0.64
	uint64_t clip_normalized_i64 = scaled_segment_range_i64 + (scaled_segment_min_i64 << 32);						// 0.64

																													// (1.0 << (32 + 31)) / 32.0 = 32.0 | 1.31
	uint64_t clip_extent_scale_i64 = (uint64_t(1) << 63) / ((uint64_t(1) << 32) - 1);								// 1.31
	ACL_ENSURE(clip_extent_scale_i64 == (uint64_t(1) << 31), "Must be == 1.0!");	// :( not necessary, cannot scale higher
	uint64_t scaled_clip_extent_i64 = clip_extent_value * clip_extent_scale_i64;									// 0.24 * 1.31 = 0.55	(integral part always 0)
	ACL_ENSURE((scaled_clip_extent_i64 & (1ull << 55)) == 0, "Integer bit used!");

	// (1.0 << (8 + 24)) / 8.0 = 25.0 | 1.24
	uint64_t clip_min_scale_i64 = (((uint64_t(1) << 8) << 24) / ((uint64_t(1) << 8) - 1));							// 1.24
	ACL_ENSURE(clip_min_scale_i64 > (uint64_t(1) << 24), "Must be >= 1.0!");
	uint64_t scaled_clip_min_i64 = clip_min_value * clip_min_scale_i64;												// 0.8 * 1.24 = 0.32	(integral part always 0)
	ACL_ENSURE((scaled_clip_min_i64 & (1ull << 32)) == 0, "Integer bit used!");

	uint64_t scaled_clip_range_i64 = (clip_normalized_i64 >> 32) * (scaled_clip_extent_i64 >> 23);					// 0.32 * 0.32 = 0.64
	uint32_t result_mantissa_i32 = uint32_t(scaled_clip_range_i64 >> 41) + uint32_t(scaled_clip_min_i64 >> 9);		// 0.23 + 0.23 = 0.23
	ACL_ENSURE((result_mantissa_i32 & (1 << 23)) == 0, "Integer bit used!");
	// Due to rounding, the integral part is never used and always 0, we can safely OR the bits with the exponent
	uint32_t exponent = 0x3f800000;
	uint32_t result_i32 = result_mantissa_i32 | exponent;
	float result_remapped = (*reinterpret_cast<float*>(&result_i32) - 1.0f);
	return (result_remapped * 2.0f) - 1.0f;
}

static void measure_error_fp(bool use_segment_range_reduction, bool use_fixed_point_clip_range_reduction, Vector4_64 out_errors[NUM_BIT_RATES][k_num_segment_values])
{
	if (k_dump_error)
		printf("Error for arithmetic: fixed point\n");
	if (k_dump_error && use_segment_range_reduction)
		printf("With segment range reduction\n");

	Vector4_32 values_32[k_num_values];
	for (size_t i = 0; i < k_num_values; ++i)
		values_32[i] = vector_cast(k_values_64[i]);

	Vector4_FP values_fp[k_num_values];
	for (size_t i = 0; i < k_num_values; ++i)
		values_fp[i] = vector_to_fp(k_values_64[i], 32, false);

	Vector4_FP clip_min_fp;	// 0.32
	Vector4_FP clip_max_fp;	// 0.32
	calculate_range_fp(values_fp, k_num_values, clip_min_fp, clip_max_fp);

	Vector4_32 clip_min_32;
	Vector4_32 clip_max_32;
	calculate_range_32(values_32, k_num_values, clip_min_32, clip_max_32);

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
		Vector4_64 segment_min_64 = vector_from_fp_64(segment_min_fp, 32, true);
		Vector4_64 segment_max_64 = vector_from_fp_64(segment_max_fp, 32, true);
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
	Vector4_32 dequantized_values_32[k_num_segment_values];
	for (uint8_t i = 1; i < NUM_BIT_RATES - 1; ++i)
	{
		quantize_fp(segment_normalized_values_fp, k_num_segment_values, i, use_segment_range_reduction, quantized_values_fp);
		dequantize_fp(quantized_values_fp, k_num_segment_values, i, use_segment_range_reduction, dequantized_segment_normalized_values_fp);

		if (use_segment_range_reduction)
			denormalize_segment_fp(dequantized_segment_normalized_values_fp, k_num_segment_values, segment_min_fp, segment_max_fp, dequantized_clip_normalized_values_fp);
		else
			memcpy(dequantized_clip_normalized_values_fp, dequantized_segment_normalized_values_fp, sizeof(dequantized_segment_normalized_values_fp));

		if (use_fixed_point_clip_range_reduction)
			denormalize_clip_fp(dequantized_clip_normalized_values_fp, k_num_segment_values, clip_min_fp, clip_max_fp, dequantized_values_32);
		else
			denormalize_clip_fp(dequantized_clip_normalized_values_fp, k_num_segment_values, clip_min_32, clip_max_32, dequantized_values_32);

#if ACL_DEBUG_ARITHMETIC
		if (i == ACL_DEBUG_BIT_RATE)
		{
			printf("Quantized value %u: { %16X, %16X, %16X }\n", ACL_DEBUG_BONE, ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[0], ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[1], ((uint32_t*)&quantized_values_fp[ACL_DEBUG_BONE])[2]);
			Vector4_64 dequantized_clip_normalized_value0_64 = vector_from_fp_64(dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE], 32, true);
			printf("Clip norm value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_clip_normalized_value0_64), vector_get_y(dequantized_clip_normalized_value0_64), vector_get_z(dequantized_clip_normalized_value0_64));
			printf("Clip norm value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].x, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].y, dequantized_clip_normalized_values_fp[ACL_DEBUG_BONE].z);
			Vector4_64 dequantized_value0_64 = vector_cast(dequantized_values_32[ACL_DEBUG_BONE]);
			Vector4_FP dequantized_value0_fp = vector_to_fp(dequantized_values_32[ACL_DEBUG_BONE], 32, false);
			printf("Lossy value %u: { %.10f, %.10f, %.10f }\n", ACL_DEBUG_BONE, vector_get_x(dequantized_value0_64), vector_get_y(dequantized_value0_64), vector_get_z(dequantized_value0_64));
			printf("Lossy value %u: { %16I64X, %16I64X, %16I64X }\n", ACL_DEBUG_BONE, dequantized_value0_fp.x, dequantized_value0_fp.y, dequantized_value0_fp.z);
		}
#else
		print_error_fp(k_values_64, k_num_segment_values, dequantized_values_32, i, out_errors);
#endif

		if (use_segment_range_reduction && !use_fixed_point_clip_range_reduction && i == ACL_DEBUG_BIT_RATE)
		{
			uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(i);

			__m128i sample_value_xyzw = _mm_castps_si128(quantized_values_fp[0]);
			__m128i sample_value_xzyw = _mm_shuffle_epi32_ab(sample_value_xyzw, sample_value_xyzw, _MM_SHUFFLE(3, 1, 2, 0));

			Vector4_FP segment_range_extent = vector_sub(segment_max_fp, segment_min_fp);
			__m128i segment_range_extent_xyzw = _mm_set_epi32(int32_t(segment_range_extent.w), int32_t(segment_range_extent.z), int32_t(segment_range_extent.y), int32_t(segment_range_extent.x));
			__m128i segment_range_extent_xzyw = _mm_set_epi32(int32_t(segment_range_extent.w), int32_t(segment_range_extent.y), int32_t(segment_range_extent.z), int32_t(segment_range_extent.x));
			__m128i segment_range_min_xzyw = _mm_set_epi32(int32_t(segment_min_fp.w), int32_t(segment_min_fp.y), int32_t(segment_min_fp.z), int32_t(segment_min_fp.x));
			__m128i segment_range_min_xyzw = _mm_set_epi32(int32_t(segment_min_fp.w), int32_t(segment_min_fp.z), int32_t(segment_min_fp.y), int32_t(segment_min_fp.x));

			Vector4_32 clip_range_extent_32 = vector_sub(clip_max_32, clip_min_32);
			Vector4_FP clip_range_extent_fp = vector_sub(clip_max_fp, clip_min_fp);
			__m128i clip_range_extent_xzyw = _mm_set_epi32(int32_t(clip_range_extent_fp.w), int32_t(clip_range_extent_fp.y), int32_t(clip_range_extent_fp.z), int32_t(clip_range_extent_fp.x));
			__m128i clip_range_min_xzyw = _mm_set_epi32(int32_t(clip_min_fp.w), int32_t(clip_min_fp.y), int32_t(clip_min_fp.z), int32_t(clip_min_fp.x));

			if (k_validate_sse_results)
			{
				for (int32_t comp_index = 0; comp_index < 3; ++comp_index)
				{
					uint32_t sample_value_ = ((uint32_t*)&sample_value_xyzw)[comp_index];
					uint32_t segment_range_extent_ = uint32_t(((uint64_t*)&segment_range_extent.x)[comp_index]);
					uint32_t segment_range_min_ = uint32_t(((uint64_t*)&segment_min_fp.x)[comp_index]);
					float clip_range_extent_ = vector_as_float_ptr(clip_range_extent_32)[comp_index];
					float clip_range_min_ = vector_as_float_ptr(clip_min_32)[comp_index];
					uint32_t clip_range_extent_i32 = uint32_t(((uint64_t*)&clip_range_extent_fp.x)[comp_index]);
					uint32_t clip_range_min_i32 = uint32_t(((uint64_t*)&clip_min_fp.x)[comp_index]);

					__m128 value_legacy_ps = calculate_f32_legacy_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					float value_legacy_ss = calculate_f32_legacy_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_legacy_ss == vector_as_float_ptr(value_legacy_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack1_ps = calculate_f32_hack1_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					float value_hack1_ss = calculate_f32_hack1_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_hack1_ss == vector_as_float_ptr(value_hack1_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack2_ps = calculate_f32_hack2_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					float value_hack2_ss = calculate_f32_hack2_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_hack2_ss == vector_as_float_ptr(value_hack2_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack3_ps = calculate_f32_hack3_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
					float value_hack3_ss = calculate_f32_hack3_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_hack3_ss == vector_as_float_ptr(value_hack3_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack4_ps = calculate_f32_hack4_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
					float value_hack4_ss = calculate_f32_hack4_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_hack4_ss == vector_as_float_ptr(value_hack4_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack5_ps = calculate_f32_hack5_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					float value_hack5_ss = calculate_f32_hack5_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_, clip_range_min_);
					ACL_ENSURE(value_hack5_ss == vector_as_float_ptr(value_hack5_ps)[comp_index], "SSE implementations differ!");

					__m128 value_hack6_ps = calculate_f32_hack6_sse_ps(segment_range_extent_xzyw, segment_range_min_xzyw, &clip_range_extent_xzyw, &clip_range_min_xzyw, num_bits_at_bit_rate, &sample_value_xzyw);
					float value_hack6_ss = calculate_f32_hack6_sse_ss(sample_value_, num_bits_at_bit_rate, segment_range_extent_, segment_range_min_, clip_range_extent_i32, clip_range_min_i32);
					ACL_ENSURE(value_hack6_ss == vector_as_float_ptr(value_hack6_ps)[comp_index], "SSE implementations differ!");
				}
			}

			// Warm up
			for (int64_t iter = 0; iter < 1000000000; ++iter)
			{
				ACL_VOLATILE_ __m128 value_legacy = calculate_f32_legacy_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
				ACL_VOLATILE_ __m128 value_hack1 = calculate_f32_hack1_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
				ACL_VOLATILE_ __m128 value_hack2 = calculate_f32_hack2_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
				ACL_VOLATILE_ __m128 value_hack3 = calculate_f32_hack3_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
				ACL_VOLATILE_ __m128 value_hack4 = calculate_f32_hack4_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
				ACL_VOLATILE_ __m128 value_hack5 = calculate_f32_hack5_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
				ACL_VOLATILE_ __m128 value_hack6 = calculate_f32_hack6_sse_ps(segment_range_extent_xzyw, segment_range_min_xzyw, &clip_range_extent_xzyw, &clip_range_min_xzyw, num_bits_at_bit_rate, &sample_value_xzyw);
			}

			const int32_t num_iter = 10000000;

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_legacy_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Legacy: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack1_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack1: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack2_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack2: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack3_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack3: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack4_sse_ps(segment_range_extent_xzyw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xzyw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack4: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack5_sse_ps(segment_range_extent_xyzw, segment_range_min_xyzw, &clip_range_extent_32, &clip_min_32, num_bits_at_bit_rate, &sample_value_xyzw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack5: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = calculate_f32_hack6_sse_ps(segment_range_extent_xzyw, segment_range_min_xzyw, &clip_range_extent_xzyw, &clip_range_min_xzyw, num_bits_at_bit_rate, &sample_value_xzyw);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("Hack6: %f ms\n", prof.get_elapsed_milliseconds());
			}
		}

		if (use_segment_range_reduction && use_fixed_point_clip_range_reduction && i == ACL_DEBUG_BIT_RATE && 0)
		{
			uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(i);

			Vector4_FP segment_range_extent = vector_sub(segment_max_fp, segment_min_fp);
			__m128i segment_range_extent_xyzw = _mm_set_epi32(int32_t(segment_range_extent.w), int32_t(segment_range_extent.z), int32_t(segment_range_extent.y), int32_t(segment_range_extent.x));
			segment_range_extent_xyzw = _mm_add_epi32(segment_range_extent_xyzw, _mm_set1_epi32(1));
			__m128i segment_range_extent_xzyw = _mm_set_epi32(int32_t(segment_range_extent.w), int32_t(segment_range_extent.y), int32_t(segment_range_extent.z), int32_t(segment_range_extent.x));
			segment_range_extent_xzyw = _mm_add_epi32(segment_range_extent_xzyw, _mm_set1_epi32(1));
			__m128i segment_range_min_xzyw = _mm_set_epi32(int32_t(segment_min_fp.w), int32_t(segment_min_fp.y), int32_t(segment_min_fp.z), int32_t(segment_min_fp.x));
			__m128i segment_range_min_xyzw = _mm_set_epi32(int32_t(segment_min_fp.w), int32_t(segment_min_fp.z), int32_t(segment_min_fp.y), int32_t(segment_min_fp.x));

			Vector4_32 clip_range_extent_32 = vector_sub(clip_max_32, clip_min_32);
			Vector4_FP clip_range_extent_fp = vector_sub(clip_max_fp, clip_min_fp);
			__m128i clip_range_extent_xyzw = _mm_set_epi32(int32_t(clip_range_extent_fp.w), int32_t(clip_range_extent_fp.z), int32_t(clip_range_extent_fp.y), int32_t(clip_range_extent_fp.x));
			__m128i clip_range_extent_xzyw = _mm_set_epi32(int32_t(clip_range_extent_fp.w), int32_t(clip_range_extent_fp.y), int32_t(clip_range_extent_fp.z), int32_t(clip_range_extent_fp.x));
			__m128i clip_range_min_xyzw = _mm_set_epi32(int32_t(clip_min_fp.w), int32_t(clip_min_fp.z), int32_t(clip_min_fp.y), int32_t(clip_min_fp.x));
			__m128i clip_range_min_xzyw = _mm_set_epi32(int32_t(clip_min_fp.w), int32_t(clip_min_fp.y), int32_t(clip_min_fp.z), int32_t(clip_min_fp.x));
			//printf("0: %f\n", vector_get_x(dequantized_values_32[0]));

			int32_t num_iter = 10000000;
			//num_iter = 1;

			// Warm up
			for (int64_t iter = 0; iter < 1000000000; ++iter)
			{
				ACL_VOLATILE_ __m128 value0 = decompress_f32_0(segment_range_extent_xyzw, segment_range_min_xyzw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_32, &clip_min_32);
				ACL_VOLATILE_ __m128 value1 = decompress_f32_1(segment_range_extent_xyzw, segment_range_min_xyzw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_32, &clip_min_32);
				ACL_VOLATILE_ __m128 value2 = decompress_1(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xzyw);
				ACL_VOLATILE_ __m128 value3 = decompress_2(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xzyw);
				ACL_VOLATILE_ __m128 value4 = decompress_3(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xyzw);
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = decompress_f32_0(segment_range_extent_xyzw, segment_range_min_xyzw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_32, &clip_min_32);
					//printf("F32 0: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("F32 0: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = decompress_f32_1(segment_range_extent_xyzw, segment_range_min_xyzw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_32, &clip_min_32);
					//printf("F32 1: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("F32 1: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = decompress_1(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xzyw);
					//printf("1: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("1: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = decompress_2(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xzyw);
					//printf("2: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("2: %f ms\n", prof.get_elapsed_milliseconds());
			}

			{
				ScopeProfiler prof;
				for (int32_t iter = 0; iter < num_iter; ++iter)
				{
					ACL_VOLATILE_ __m128 value = decompress_3(segment_range_extent_xzyw, segment_range_min_xzyw, num_bits_at_bit_rate, (__m128i*)&quantized_values_fp[0], &clip_range_extent_xzyw, &clip_range_min_xyzw);
					//printf("3: %f\n", _mm_cvtss_f32(value));
				}
				prof.stop();
				printf("3: %f ms\n", prof.get_elapsed_milliseconds());
			}
		}
	}

	if (k_dump_error)
		printf("\n");
}

static void print_wins(const char* label, Vector4_64 error_64[NUM_BIT_RATES][k_num_segment_values], Vector4_64 error_32[NUM_BIT_RATES][k_num_segment_values], Vector4_64 error_fp[NUM_BIT_RATES][k_num_segment_values])
{
	uint32_t num_total_comp_wins_64 = 0;
	uint32_t num_total_comp_wins_32 = 0;
	uint32_t num_total_comp_wins_fp = 0;
	uint32_t num_total_vec_wins_64 = 0;
	uint32_t num_total_vec_wins_32 = 0;
	uint32_t num_total_vec_wins_fp = 0;
	uint32_t num_total_comp_loss_64 = 0;
	uint32_t num_total_comp_loss_32 = 0;
	uint32_t num_total_comp_loss_fp = 0;
	uint32_t num_total_vec_loss_64 = 0;
	uint32_t num_total_vec_loss_32 = 0;
	uint32_t num_total_vec_loss_fp = 0;
	for (uint8_t bit_rate = 1; bit_rate < NUM_BIT_RATES - 1; ++bit_rate)
	{
		uint32_t num_comp_wins_64 = 0;
		uint32_t num_comp_wins_32 = 0;
		uint32_t num_comp_wins_fp = 0;
		uint32_t num_vec_wins_64 = 0;
		uint32_t num_vec_wins_32 = 0;
		uint32_t num_vec_wins_fp = 0;
		uint32_t num_comp_loss_64 = 0;
		uint32_t num_comp_loss_32 = 0;
		uint32_t num_comp_loss_fp = 0;
		uint32_t num_vec_loss_64 = 0;
		uint32_t num_vec_loss_32 = 0;
		uint32_t num_vec_loss_fp = 0;
		for (size_t i = 0; i < k_num_segment_values; ++i)
		{
#if ACL_MEASURE_COMP_WINS
			if (k_enable_float64)
			{
				if (k_enable_float32)
				{
					bool x_64_less_than_32 = vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]);
					bool y_64_less_than_32 = vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]);
					bool z_64_less_than_32 = vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]);

					if (x_64_less_than_32)
						num_comp_wins_64++;

					if (y_64_less_than_32)
						num_comp_wins_64++;

					if (z_64_less_than_32)
						num_comp_wins_64++;
				}

				if (k_enable_fp)
				{
					bool x_64_less_than_fp = vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i]);
					bool y_64_less_than_fp = vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i]);
					bool z_64_less_than_fp = vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i]);

					if (x_64_less_than_fp)
						num_comp_wins_64++;

					if (y_64_less_than_fp)
						num_comp_wins_64++;

					if (z_64_less_than_fp)
						num_comp_wins_64++;
				}
			}

			if (k_enable_float32)
			{
				if (k_enable_float64)
				{
					bool x_32_less_than_64 = vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]);
					bool y_32_less_than_64 = vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]);
					bool z_32_less_than_64 = vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]);

					if (x_32_less_than_64)
						num_comp_wins_32++;

					if (y_32_less_than_64)
						num_comp_wins_32++;

					if (z_32_less_than_64)
						num_comp_wins_32++;
				}

				if (k_enable_fp)
				{
					bool x_32_less_than_fp = vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i]);
					bool y_32_less_than_fp = vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i]);
					bool z_32_less_than_fp = vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i]);

					if (x_32_less_than_fp)
						num_comp_wins_32++;

					if (y_32_less_than_fp)
						num_comp_wins_32++;

					if (z_32_less_than_fp)
						num_comp_wins_32++;
				}
			}

			if (k_enable_fp)
			{
				if (k_enable_float64)
				{
					bool x_fp_less_than_64 = vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]);
					bool y_fp_less_than_64 = vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]);
					bool z_fp_less_than_64 = vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]);

					if (x_fp_less_than_64)
						num_comp_wins_fp++;

					if (y_fp_less_than_64)
						num_comp_wins_fp++;

					if (z_fp_less_than_64)
						num_comp_wins_fp++;
				}

				if (k_enable_float32)
				{
					bool x_fp_less_than_32 = vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]);
					bool y_fp_less_than_32 = vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]);
					bool z_fp_less_than_32 = vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]);

					if (x_fp_less_than_32)
						num_comp_wins_fp++;

					if (y_fp_less_than_32)
						num_comp_wins_fp++;

					if (z_fp_less_than_32)
						num_comp_wins_fp++;
				}
			}
#endif

#if ACL_MEASURE_COMP_LOSS
			if (k_enable_float64)
			{
				if (k_enable_float32)
				{
					bool x_64_greater_than_32 = vector_get_x(error_64[bit_rate][i]) > vector_get_x(error_32[bit_rate][i]);
					bool y_64_greater_than_32 = vector_get_y(error_64[bit_rate][i]) > vector_get_y(error_32[bit_rate][i]);
					bool z_64_greater_than_32 = vector_get_z(error_64[bit_rate][i]) > vector_get_z(error_32[bit_rate][i]);

					if (x_64_greater_than_32)
						num_comp_loss_64++;

					if (y_64_greater_than_32)
						num_comp_loss_64++;

					if (z_64_greater_than_32)
						num_comp_loss_64++;
				}

				if (k_enable_fp)
				{
					bool x_64_greater_than_fp = vector_get_x(error_64[bit_rate][i]) > vector_get_x(error_fp[bit_rate][i]);
					bool y_64_greater_than_fp = vector_get_y(error_64[bit_rate][i]) > vector_get_y(error_fp[bit_rate][i]);
					bool z_64_greater_than_fp = vector_get_z(error_64[bit_rate][i]) > vector_get_z(error_fp[bit_rate][i]);

					if (x_64_greater_than_fp)
						num_comp_loss_64++;

					if (y_64_greater_than_fp)
						num_comp_loss_64++;

					if (z_64_greater_than_fp)
						num_comp_loss_64++;
				}
			}

			if (k_enable_float32)
			{
				if (k_enable_float64)
				{
					bool x_32_greater_than_64 = vector_get_x(error_32[bit_rate][i]) > vector_get_x(error_64[bit_rate][i]);
					bool y_32_greater_than_64 = vector_get_y(error_32[bit_rate][i]) > vector_get_y(error_64[bit_rate][i]);
					bool z_32_greater_than_64 = vector_get_z(error_32[bit_rate][i]) > vector_get_z(error_64[bit_rate][i]);

					if (x_32_greater_than_64)
						num_comp_loss_32++;

					if (y_32_greater_than_64)
						num_comp_loss_32++;

					if (z_32_greater_than_64)
						num_comp_loss_32++;
				}

				if (k_enable_fp)
				{
					bool x_32_greater_than_fp = vector_get_x(error_32[bit_rate][i]) > vector_get_x(error_fp[bit_rate][i]);
					bool y_32_greater_than_fp = vector_get_y(error_32[bit_rate][i]) > vector_get_y(error_fp[bit_rate][i]);
					bool z_32_greater_than_fp = vector_get_z(error_32[bit_rate][i]) > vector_get_z(error_fp[bit_rate][i]);

					if (x_32_greater_than_fp)
						num_comp_loss_32++;

					if (y_32_greater_than_fp)
						num_comp_loss_32++;

					if (z_32_greater_than_fp)
						num_comp_loss_32++;
				}
			}

			if (k_enable_fp)
			{
				if (k_enable_float64)
				{
					bool x_fp_greater_than_64 = vector_get_x(error_fp[bit_rate][i]) > vector_get_x(error_64[bit_rate][i]);
					bool y_fp_greater_than_64 = vector_get_y(error_fp[bit_rate][i]) > vector_get_y(error_64[bit_rate][i]);
					bool z_fp_greater_than_64 = vector_get_z(error_fp[bit_rate][i]) > vector_get_z(error_64[bit_rate][i]);

					if (x_fp_greater_than_64)
						num_comp_loss_fp++;

					if (y_fp_greater_than_64)
						num_comp_loss_fp++;

					if (z_fp_greater_than_64)
						num_comp_loss_fp++;
				}

				if (k_enable_float32)
				{
					bool x_fp_greater_than_32 = vector_get_x(error_fp[bit_rate][i]) > vector_get_x(error_32[bit_rate][i]);
					bool y_fp_greater_than_32 = vector_get_y(error_fp[bit_rate][i]) > vector_get_y(error_32[bit_rate][i]);
					bool z_fp_greater_than_32 = vector_get_z(error_fp[bit_rate][i]) > vector_get_z(error_32[bit_rate][i]);

					if (x_fp_greater_than_32)
						num_comp_loss_fp++;

					if (y_fp_greater_than_32)
						num_comp_loss_fp++;

					if (z_fp_greater_than_32)
						num_comp_loss_fp++;
				}
			}
#endif

#if ACL_MEASURE_VEC3_WINS
			if (k_enable_float64)
			{
				if (k_enable_float32)
				{
					bool x_64_less_than_32 = vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]);
					bool y_64_less_than_32 = vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]);
					bool z_64_less_than_32 = vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]);
					bool xyz_64_less_than_32 = x_64_less_than_32 && y_64_less_than_32 && z_64_less_than_32;

					if (xyz_64_less_than_32)
						num_vec_wins_64++;
				}

				if (k_enable_fp)
				{
					bool x_64_less_than_fp = vector_get_x(error_64[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i]);
					bool y_64_less_than_fp = vector_get_y(error_64[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i]);
					bool z_64_less_than_fp = vector_get_z(error_64[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i]);
					bool xyz_64_less_than_fp = x_64_less_than_fp && y_64_less_than_fp && z_64_less_than_fp;

					if (xyz_64_less_than_fp)
						num_vec_wins_64++;
				}
			}

			if (k_enable_float32)
			{
				if (k_enable_float64)
				{
					bool x_32_less_than_64 = vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]);
					bool y_32_less_than_64 = vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]);
					bool z_32_less_than_64 = vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]);
					bool xyz_32_less_than_64 = x_32_less_than_64 && y_32_less_than_64 && z_32_less_than_64;

					if (xyz_32_less_than_64)
						num_vec_wins_32++;
				}

				if (k_enable_fp)
				{
					bool x_32_less_than_fp = vector_get_x(error_32[bit_rate][i]) < vector_get_x(error_fp[bit_rate][i]);
					bool y_32_less_than_fp = vector_get_y(error_32[bit_rate][i]) < vector_get_y(error_fp[bit_rate][i]);
					bool z_32_less_than_fp = vector_get_z(error_32[bit_rate][i]) < vector_get_z(error_fp[bit_rate][i]);
					bool xyz_32_less_than_fp = x_32_less_than_fp && y_32_less_than_fp && z_32_less_than_fp;

					if (xyz_32_less_than_fp)
						num_vec_wins_32++;
				}
			}

			if (k_enable_fp)
			{
				if (k_enable_float64)
				{
					bool x_fp_less_than_64 = vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_64[bit_rate][i]);
					bool y_fp_less_than_64 = vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_64[bit_rate][i]);
					bool z_fp_less_than_64 = vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_64[bit_rate][i]);
					bool xyz_fp_less_than_64 = x_fp_less_than_64 && y_fp_less_than_64 && z_fp_less_than_64;

					if (xyz_fp_less_than_64)
						num_vec_wins_fp++;
				}

				if (k_enable_float32)
				{
					bool x_fp_less_than_32 = vector_get_x(error_fp[bit_rate][i]) < vector_get_x(error_32[bit_rate][i]);
					bool y_fp_less_than_32 = vector_get_y(error_fp[bit_rate][i]) < vector_get_y(error_32[bit_rate][i]);
					bool z_fp_less_than_32 = vector_get_z(error_fp[bit_rate][i]) < vector_get_z(error_32[bit_rate][i]);
					bool xyz_fp_less_than_32 = x_fp_less_than_32 && y_fp_less_than_32 && z_fp_less_than_32;

					if (xyz_fp_less_than_32)
						num_vec_wins_fp++;
				}
			}
#endif

#if ACL_MEASURE_VEC3_LOSS
			if (k_enable_float64)
			{
				if (k_enable_float32)
				{
					bool x_64_greater_than_32 = vector_get_x(error_64[bit_rate][i]) > vector_get_x(error_32[bit_rate][i]);
					bool y_64_greater_than_32 = vector_get_y(error_64[bit_rate][i]) > vector_get_y(error_32[bit_rate][i]);
					bool z_64_greater_than_32 = vector_get_z(error_64[bit_rate][i]) > vector_get_z(error_32[bit_rate][i]);
					bool xyz_64_greater_than_32 = x_64_greater_than_32 && y_64_greater_than_32 && z_64_greater_than_32;

					if (xyz_64_greater_than_32)
						num_vec_loss_64++;
				}

				if (k_enable_fp)
				{
					bool x_64_greater_than_fp = vector_get_x(error_64[bit_rate][i]) > vector_get_x(error_fp[bit_rate][i]);
					bool y_64_greater_than_fp = vector_get_y(error_64[bit_rate][i]) > vector_get_y(error_fp[bit_rate][i]);
					bool z_64_greater_than_fp = vector_get_z(error_64[bit_rate][i]) > vector_get_z(error_fp[bit_rate][i]);
					bool xyz_64_greater_than_fp = x_64_greater_than_fp && y_64_greater_than_fp && z_64_greater_than_fp;

					if (xyz_64_greater_than_fp)
						num_vec_loss_64++;
				}
			}

			if (k_enable_float32)
			{
				if (k_enable_float64)
				{
					bool x_32_greater_than_64 = vector_get_x(error_32[bit_rate][i]) > vector_get_x(error_64[bit_rate][i]);
					bool y_32_greater_than_64 = vector_get_y(error_32[bit_rate][i]) > vector_get_y(error_64[bit_rate][i]);
					bool z_32_greater_than_64 = vector_get_z(error_32[bit_rate][i]) > vector_get_z(error_64[bit_rate][i]);
					bool xyz_32_greater_than_64 = x_32_greater_than_64 && y_32_greater_than_64 && z_32_greater_than_64;

					if (xyz_32_greater_than_64)
						num_vec_loss_32++;
				}

				if (k_enable_fp)
				{
					bool x_32_greater_than_fp = vector_get_x(error_32[bit_rate][i]) > vector_get_x(error_fp[bit_rate][i]);
					bool y_32_greater_than_fp = vector_get_y(error_32[bit_rate][i]) > vector_get_y(error_fp[bit_rate][i]);
					bool z_32_greater_than_fp = vector_get_z(error_32[bit_rate][i]) > vector_get_z(error_fp[bit_rate][i]);
					bool xyz_32_greater_than_fp = x_32_greater_than_fp && y_32_greater_than_fp && z_32_greater_than_fp;

					if (xyz_32_greater_than_fp)
						num_vec_loss_32++;
				}
			}

			if (k_enable_fp)
			{
				if (k_enable_float64)
				{
					bool x_fp_greater_than_64 = vector_get_x(error_fp[bit_rate][i]) > vector_get_x(error_64[bit_rate][i]);
					bool y_fp_greater_than_64 = vector_get_y(error_fp[bit_rate][i]) > vector_get_y(error_64[bit_rate][i]);
					bool z_fp_greater_than_64 = vector_get_z(error_fp[bit_rate][i]) > vector_get_z(error_64[bit_rate][i]);
					bool xyz_fp_greater_than_64 = x_fp_greater_than_64 && y_fp_greater_than_64 && z_fp_greater_than_64;

					if (xyz_fp_greater_than_64)
						num_vec_loss_fp++;
				}

				if (k_enable_float32)
				{
					bool x_fp_greater_than_32 = vector_get_x(error_fp[bit_rate][i]) > vector_get_x(error_32[bit_rate][i]);
					bool y_fp_greater_than_32 = vector_get_y(error_fp[bit_rate][i]) > vector_get_y(error_32[bit_rate][i]);
					bool z_fp_greater_than_32 = vector_get_z(error_fp[bit_rate][i]) > vector_get_z(error_32[bit_rate][i]);
					bool xyz_fp_greater_than_32 = x_fp_greater_than_32 && y_fp_greater_than_32 && z_fp_greater_than_32;

					if (xyz_fp_greater_than_32)
						num_vec_loss_fp++;
				}
			}
#endif
		}

		if (k_dump_bit_rate_wins)
		{
			const uint8_t num_bits_at_bit_rate = get_num_bits_at_bit_rate(bit_rate);
			printf("Bit rate %u (%u, %u, %u) comp wins: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_comp_wins_64, num_comp_wins_32, num_comp_wins_fp);
			printf("Bit rate %u (%u, %u, %u) vec3 wins: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_vec_wins_64, num_vec_wins_32, num_vec_wins_fp);
			//printf("Bit rate %u (%u, %u, %u) comp loss: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_comp_loss_64, num_comp_loss_32, num_comp_loss_fp);
			//printf("Bit rate %u (%u, %u, %u) vec3 loss: 64 [%u] 32 [%u] fp [%u]\n", bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_bits_at_bit_rate, num_vec_loss_64, num_vec_loss_32, num_vec_loss_fp);
		}

		num_total_comp_wins_64 += num_comp_wins_64;
		num_total_comp_wins_32 += num_comp_wins_32;
		num_total_comp_wins_fp += num_comp_wins_fp;
		num_total_vec_wins_64 += num_vec_wins_64;
		num_total_vec_wins_32 += num_vec_wins_32;
		num_total_vec_wins_fp += num_vec_wins_fp;
		num_total_comp_loss_64 += num_comp_loss_64;
		num_total_comp_loss_32 += num_comp_loss_32;
		num_total_comp_loss_fp += num_comp_loss_fp;
		num_total_vec_loss_64 += num_vec_loss_64;
		num_total_vec_loss_32 += num_vec_loss_32;
		num_total_vec_loss_fp += num_vec_loss_fp;
	}

	printf("%s comp wins: 64 [%u] 32 [%u] fp [%u]\n", label, num_total_comp_wins_64, num_total_comp_wins_32, num_total_comp_wins_fp);
	printf("%s vec3 wins: 64 [%u] 32 [%u] fp [%u]\n", label, num_total_vec_wins_64, num_total_vec_wins_32, num_total_vec_wins_fp);
	//printf("%s comp loss: 64 [%u] 32 [%u] fp [%u]\n", label, num_total_comp_loss_64, num_total_comp_loss_32, num_total_comp_loss_fp);
	//printf("%s vec3 loss: 64 [%u] 32 [%u] fp [%u]\n", label, num_total_vec_loss_64, num_total_vec_loss_32, num_total_vec_loss_fp);
}

void test_arithmetic()
{
	Vector4_64 error_64[NUM_BIT_RATES][k_num_segment_values];
	Vector4_64 error_32[NUM_BIT_RATES][k_num_segment_values];
	Vector4_64 error_fp[NUM_BIT_RATES][k_num_segment_values];

	measure_error_64(false, error_64);
	measure_error_32(false, error_32);
	measure_error_fp(false, true, error_fp);
	print_wins("No segmenting, fp range", error_64, error_32, error_fp);
	measure_error_fp(false, false, error_fp);
	print_wins("No segmenting, 32 range", error_64, error_32, error_fp);

	measure_error_64(true, error_64);
	measure_error_32(true, error_32);
	measure_error_fp(true, true, error_fp);
	print_wins("Segmenting, fp range", error_64, error_32, error_fp);
	measure_error_fp(true, false, error_fp);
	print_wins("Segmenting, 32 range", error_64, error_32, error_fp);
}

enum ResultType
{
	eF32_Truth,
	eF32_Legacy,
	eF32_Hack1,
	eF32_Hack2,
	eF32_Hack3,
	eF32_Hack4,
	eF32_Hack5,
	eF32_Hack6,
	eF32_Hack7,
	eF32_Hack8,
	eMax,
};

struct ExhaustiveSearchSlice
{
	uint8_t bit_rate;
	int32_t clip_min_value_i32;
	int32_t clip_max_value_start_i32;
	int32_t clip_max_value_end_i32;

	double total_bit_rate_error[eMax];
	double max_bit_rate_error[eMax];
	double num_bit_rate_samples;

	float worst_clip_extent_value[eMax];
	float worst_clip_min_value[eMax];

	int32_t worst_segment_min_value[eMax];
	int32_t worst_segment_extent_value[eMax];
	int32_t worst_sample_value[eMax];

	void merge(const ExhaustiveSearchSlice& other)
	{
		for (int32_t i = 0; i < eMax; ++i)
		{
			total_bit_rate_error[i] += other.total_bit_rate_error[i];
			num_bit_rate_samples += other.num_bit_rate_samples;

			if (other.max_bit_rate_error[i] > max_bit_rate_error[i])
			{
				max_bit_rate_error[i] = other.max_bit_rate_error[i];
				worst_clip_extent_value[i] = other.worst_clip_extent_value[i];
				worst_clip_min_value[i] = other.worst_clip_min_value[i];
				worst_segment_min_value[i] = other.worst_segment_min_value[i];
				worst_segment_extent_value[i] = other.worst_segment_extent_value[i];
				worst_sample_value[i] = other.worst_sample_value[i];
			}
		}
	}
};

static void exhaustive_search_with_inputs(uint8_t bit_rate, float clip_min_value, float clip_extent_value
	, double total_bit_rate_error[eMax], double max_bit_rate_error[eMax], double& num_bit_rate_samples
	, float worst_clip_min_value[eMax], float worst_clip_extent_value[eMax]
	, int32_t worst_segment_min_value[eMax], int32_t worst_segment_extent_value[eMax]
	, int32_t worst_sample_value[eMax])
{
	const int32_t num_value_bits = get_num_bits_at_bit_rate(bit_rate);

	double clip_min_value_dbl = double(clip_min_value);
	double clip_min_value_dbl_remapped = (clip_min_value_dbl * 0.5) + 0.5;
	uint32_t clip_min_value_i32 = uint32_t(uint64_t(clip_min_value_dbl_remapped * (double((1ull << 32) - 1) / double(1ull << 32)) * double((uint64_t(1) << 32) - 1)));
	uint32_t clip_min_value_i24 = uint32_t(uint64_t(clip_min_value_dbl_remapped * (double((1ull << 24) - 1) / double(1ull << 24)) * double((uint64_t(1) << 24) - 1)));
	uint32_t clip_min_value_i8 = uint32_t(uint64_t(clip_min_value_dbl_remapped * (double((1ull << 8) - 1) / double(1ull << 8)) * double((uint64_t(1) << 8) - 1)));

	double clip_extent_value_dbl = double(clip_extent_value);
	double clip_extent_value_dbl_remapped = clip_extent_value_dbl * 0.5;
	uint32_t clip_extent_value_i32 = uint32_t(uint64_t(clip_extent_value_dbl_remapped * (double((1ull << 32) - 1) / double(1ull << 32)) * double((uint64_t(1) << 32) - 1)));
	uint32_t clip_extent_value_i24 = uint32_t(uint64_t(clip_extent_value_dbl_remapped * (double((1ull << 24) - 1) / double(1ull << 24)) * double((uint64_t(1) << 24) - 1)));

	for (int32_t segment_min_value = 0; segment_min_value < (1 << k_num_segment_value_bits); ++segment_min_value)
	{
		//if (segment_min_value != 255) continue;
		for (int32_t segment_max_value = segment_min_value + 1; segment_max_value < (1 << k_num_segment_value_bits); ++segment_max_value)
		{
			int32_t segment_extent_value = segment_max_value - segment_min_value;
			//if (segment_min_value + segment_extent_value > 255) continue;
			//if (segment_extent_value != 255) continue;
			for (int32_t sample_value = 1; sample_value < (1 << num_value_bits); ++sample_value)
			{
				//if (sample_value != 15) continue;
				float results[eMax];

				results[eF32_Truth] = calculate_f32_truth(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_dbl, clip_min_value_dbl);
				results[eF32_Legacy] = calculate_f32_legacy_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack1] = calculate_f32_hack1_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack2] = calculate_f32_hack2_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack3] = calculate_f32_hack3_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack4] = calculate_f32_hack4_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack5] = calculate_f32_hack5_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
				results[eF32_Hack6] = calculate_f32_hack6_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i32, clip_min_value_i32);
				results[eF32_Hack7] = calculate_f32_hack7_sse_ss(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i24, clip_min_value_i24);
				results[eF32_Hack8] = calculate_f32_hack8(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i24, clip_min_value_i8);

				if (k_validate_sse_results)
				{
					float results_ref[eMax];

					results_ref[eF32_Truth] = calculate_f32_truth(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_dbl, clip_min_value_dbl);
					results_ref[eF32_Legacy] = calculate_f32_legacy(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack1] = calculate_f32_hack1(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack2] = calculate_f32_hack2(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack3] = calculate_f32_hack3(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack4] = calculate_f32_hack4(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack5] = calculate_f32_hack5(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value, clip_min_value);
					results_ref[eF32_Hack6] = calculate_f32_hack6(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i32, clip_min_value_i32);
					results_ref[eF32_Hack7] = calculate_f32_hack7(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i24, clip_min_value_i24);
					results_ref[eF32_Hack8] = calculate_f32_hack8(sample_value, num_value_bits, segment_extent_value, segment_min_value, clip_extent_value_i24, clip_min_value_i8);

					for (int32_t i = 0; i < eMax; ++i)
					{
						ACL_ENSURE(results[i] == results_ref[i], "SSE implementation is invalid!");
					}
				}

				//printf("[%4u, %4u] | %4u -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", segment_min_value, segment_extent_value, sample_value, results[eF32_Truth], results[eF32_Legacy], results[eF32_Hack1], results[eF32_Hack2], results[eF32_Hack3], results[eF32_Hack4], results[eF32_Hack5]);
				//printf("[%4u, %4u] | %4u -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", segment_min_value, segment_extent_value, sample_value, results[eF32_Truth], fabs(results[eF32_Legacy] - results[eF32_Truth]), fabs(results[eF32_Hack1] - results[eF32_Truth]), fabs(results[eF32_Hack2] - results[eF32_Truth]), fabs(results[eF32_Hack3] - results[eF32_Truth]), fabs(results[eF32_Hack4] - results[eF32_Truth]), fabs(results[eF32_Hack5] - results[eF32_Truth]));
				float err = fabs(results[eF32_Hack5] - results[eF32_Truth]);
				if (err > 0.2f && 1)
					printf("!!\n");

				for (int32_t i = 0; i < eMax; ++i)
				{
					double error = fabs(results[i] - results[eF32_Truth]);
					total_bit_rate_error[i] += error;
					if (error > max_bit_rate_error[i])
					{
						max_bit_rate_error[i] = error;
						worst_clip_min_value[i] = clip_min_value;
						worst_clip_extent_value[i] = clip_extent_value;
						worst_segment_min_value[i] = segment_min_value;
						worst_segment_extent_value[i] = segment_extent_value;
						worst_sample_value[i] = sample_value;
					}
				}
				num_bit_rate_samples += 1.0;
			}
		}
	}
}

static void exhaustive_search_slice(ExhaustiveSearchSlice& slice)
{
	const int32_t num_value_bits = get_num_bits_at_bit_rate(slice.bit_rate);
	float clip_min_value = *reinterpret_cast<float*>(&slice.clip_min_value_i32) - 1.0f;
	double clip_min_value_dbl = double(clip_min_value);
	uint32_t clip_min_value_i32 = uint32_t(uint64_t(clip_min_value_dbl * (double((1ull << 32) - 1) / double(1ull << 32)) * double((uint64_t(1) << 32) - 1)));
	uint32_t clip_min_value_i24 = uint32_t(uint64_t(clip_min_value_dbl * (double((1ull << 24) - 1) / double(1ull << 24)) * double((uint64_t(1) << 24) - 1)));
	uint32_t clip_min_value_i8 = uint32_t(uint64_t(clip_min_value_dbl * (double((1ull << 8) - 1) / double(1ull << 8)) * double((uint64_t(1) << 8) - 1)));

	for (int32_t clip_max_value_i32 = slice.clip_max_value_start_i32;;)
	{
		//if ((clip_max_value_i32 % 100) == 0)
			//printf("[%x] .. [%x] .. [%x]\n", slice.clip_max_value_start_i32, clip_max_value_i32, slice.clip_max_value_end_i32);
		//if (clip_max_value_i32 != one_float_as_i32) break;
		float clip_max_value = *reinterpret_cast<float*>(&clip_max_value_i32) - 1.0f;
		float clip_extent_value = clip_max_value - clip_min_value;
		double clip_extent_value_dbl = double(clip_max_value) - clip_min_value_dbl;
		uint32_t clip_extent_value_i32 = uint32_t(uint64_t(clip_extent_value_dbl * (double((1ull << 32) - 1) / double(1ull << 32)) * double((uint64_t(1) << 32) - 1)));
		uint32_t clip_extent_value_i24 = uint32_t(uint64_t(clip_extent_value_dbl * (double((1ull << 24) - 1) / double(1ull << 24)) * double((uint64_t(1) << 24) - 1)));

		if (slice.clip_min_value_i32 < 0)
		{
			clip_min_value = 0.0f;
			clip_min_value_dbl = 0.0;
			clip_min_value_i32 = 0;
			clip_min_value_i24 = 0;
			clip_min_value_i8 = 0;
			clip_extent_value = 1.0f;
			clip_extent_value_dbl = 1.0;
			clip_extent_value_i32 = 0xFFFFFFFF;
			clip_extent_value_i24 = 0x00FFFFFF;
		}

		exhaustive_search_with_inputs(slice.bit_rate, clip_min_value, clip_extent_value
			, slice.total_bit_rate_error, slice.max_bit_rate_error, slice.num_bit_rate_samples
			, slice.worst_clip_min_value, slice.worst_clip_extent_value, slice.worst_segment_min_value, slice.worst_segment_extent_value, slice.worst_sample_value);

		if (clip_max_value_i32 == slice.clip_max_value_end_i32)
			break;

		int32_t skip_offset = 10000;
		clip_max_value_i32 = std::min(clip_max_value_i32 + skip_offset, slice.clip_max_value_end_i32);
	}
}

void test_exhaustive()
{
	ExhaustiveSearchSlice total_result_slice;
	memset(&total_result_slice, 0, sizeof(total_result_slice));

	const int32_t slice_size = 1000000;
	const int32_t num_threads = 11;				// It is slightly faster if you saturate logical cores instead of physical cores
	const bool quick_test = false;
	const bool no_clip = false;
	const bool print_avg = no_clip;
	const bool use_random_sampling = true;
	const int32_t random_seed = 304;
	//const int32_t num_random_samples = quick_test ? 0 : 100000;
	const int32_t num_random_samples = quick_test ? 0 : 10;

	std::uniform_real_distribution<float> random_flt_distribution(0.1e-10f, std::nextafter(1.0f, std::numeric_limits<float>::max()));
	std::uniform_int_distribution<int32_t> random_sign_distribution(0, 2);
	std::default_random_engine re(random_seed);
	ScopeProfiler total_profiler;

	for (uint8_t bit_rate = 1; bit_rate < 15; ++bit_rate)
	{
		int32_t num_value_bits = get_num_bits_at_bit_rate(bit_rate);
		//if (num_value_bits != 16) continue;

		std::vector<ExhaustiveSearchSlice> slices;
		ScopeProfiler bit_rate_profiler;

		if (no_clip)
		{
			ExhaustiveSearchSlice slice;
			memset(&slice, 0, sizeof(slice));

			slice.bit_rate = bit_rate;
			slice.clip_min_value_i32 = -1;
			slice.clip_max_value_start_i32 = -1;
			slice.clip_max_value_end_i32 = -1;

			slices.push_back(slice);
		}
		else if (use_random_sampling)
		{
			struct ClipRange
			{
				float clip_min;
				float clip_extent;
			};

			// Test edge cases
			const ClipRange default_samples[] =
			{
				ClipRange{ -1.0f, 0.0f },
				ClipRange{ -1.0f, 1.0f },
				ClipRange{ -1.0f, 2.0f },
				ClipRange{ 0.0f, 1.0f },
				ClipRange{ 0.5f, 0.5f },
				ClipRange{ 1.0f, 0.0f },
				ClipRange{ 0.9999999999999f, 1.0f - 0.9999999999999f },
			};

			const int32_t num_default_samples = sizeof(default_samples) / sizeof(ClipRange);
			const int32_t num_samples = num_random_samples + num_default_samples;

			printf("\rCompleted %.2f %% ...", 0.0f);
			fflush(stdout);

			std::vector<std::thread> threads;
			std::atomic<int32_t> sample_index = 0;
			std::atomic<int32_t> num_completed = 0;
			std::atomic_flag lock = ATOMIC_FLAG_INIT;
			for (int32_t thread_index = 0; thread_index < num_threads; ++thread_index)
			{
				std::thread thread([&]()
				{
					ExhaustiveSearchSlice thread_slice;
					memset(&thread_slice, 0, sizeof(thread_slice));

					while (true)
					{
						int32_t thread_sample_index = sample_index.fetch_add(1, std::memory_order_relaxed);
						if (thread_sample_index >= num_samples)
							break;

						ClipRange clip_range;
						if (thread_sample_index < num_default_samples)
						{
							clip_range = default_samples[thread_sample_index];
						}
						else
						{
							while (lock.test_and_set(std::memory_order_acquire));

							int32_t sign_bias0 = random_sign_distribution(re);
							int32_t sign_bias1 = random_sign_distribution(re);

							float clip_value0_sign = sign_bias0 != 0 ? 1.0f : -1.0f;
							float clip_value1_sign = sign_bias1 != 0 ? 1.0f : -1.0f;

							float clip_range_value0 = random_flt_distribution(re) * clip_value0_sign;
							float clip_range_value1 = random_flt_distribution(re) * clip_value1_sign;

							lock.clear(std::memory_order_release);

							float clip_range_min = min(clip_range_value0, clip_range_value1);
							float clip_range_max = max(clip_range_value0, clip_range_value1);
							float clip_range_extent = clip_range_max - clip_range_min;

							clip_range.clip_min = clip_range_min;
							clip_range.clip_extent = clip_range_extent;
						}

						exhaustive_search_with_inputs(bit_rate, clip_range.clip_min, clip_range.clip_extent
							, thread_slice.total_bit_rate_error, thread_slice.max_bit_rate_error, thread_slice.num_bit_rate_samples
							, thread_slice.worst_clip_min_value, thread_slice.worst_clip_extent_value, thread_slice.worst_segment_min_value, thread_slice.worst_segment_extent_value, thread_slice.worst_sample_value);

						{
							while (lock.test_and_set(std::memory_order_acquire));

							size_t thread_num_completed = num_completed.fetch_add(1, std::memory_order_relaxed) + 1;
							float progress = (float(thread_num_completed) / float(num_samples)) * 100.0f;
							printf("\rCompleted %.2f %% ...", progress);
							fflush(stdout);

							lock.clear(std::memory_order_release);
						}
					}

					while (lock.test_and_set(std::memory_order_acquire));

					slices.push_back(thread_slice);

					lock.clear(std::memory_order_release);
				});

				threads.push_back(std::move(thread));
			}

			for (std::thread& thread : threads)
				thread.join();

			printf("\r                                      \n");
			fflush(stdout);
		}
		else
		{
			for (int32_t clip_min_value_i32 = k_one_float_as_i32;;)
			{
				//if (clip_min_value_i32 != one_float_as_i32) break;
				int32_t clip_max_value_i32 = clip_min_value_i32 + 1;
				for (; clip_max_value_i32 <= k_two_float_as_i32; clip_max_value_i32 += slice_size)
				{
					//if (clip_max_value_i32 != one_float_as_i32) break;
					ExhaustiveSearchSlice slice;
					memset(&slice, 0, sizeof(slice));

					slice.bit_rate = bit_rate;
					slice.clip_min_value_i32 = clip_min_value_i32;
					slice.clip_max_value_start_i32 = clip_max_value_i32;
					slice.clip_max_value_end_i32 = std::min(clip_max_value_i32 + slice_size, k_two_float_as_i32);

					slices.push_back(slice);

					if (quick_test && slices.size() > 4)
						break;
				}

				if (quick_test && slices.size() > 4)
					break;

				if (clip_min_value_i32 == k_two_float_as_i32)
					break;

				int32_t skip_offset = 10000;
				clip_min_value_i32 = std::min(clip_min_value_i32 + skip_offset, k_two_float_as_i32);
			}

			std::vector<std::thread> threads;
			std::atomic<size_t> slice_index = 0;
			std::atomic<int32_t> num_completed = 0;
			std::atomic_flag printf_lock = ATOMIC_FLAG_INIT;
			for (int32_t thread_index = 0; thread_index < num_threads; ++thread_index)
			{
				std::thread thread([&]()
				{
					while (true)
					{
						size_t thread_slice_index = slice_index.fetch_add(1, std::memory_order_relaxed);
						if (thread_slice_index >= slices.size())
							break;

						ExhaustiveSearchSlice& slice = slices[thread_slice_index];
						exhaustive_search_slice(slice);

						{
							while (printf_lock.test_and_set(std::memory_order_acquire));

							size_t thread_num_completed = num_completed.fetch_add(1, std::memory_order_relaxed) + 1;
							float progress = (float(thread_num_completed) / float(slices.size())) * 100.0f;
							printf("\rCompleted %.2f %% ...", progress);
							fflush(stdout);

							printf_lock.clear(std::memory_order_release);
						}
					}
				});

				threads.push_back(std::move(thread));
			}

			for (std::thread& thread : threads)
				thread.join();

			printf("\r                                      \n");
			fflush(stdout);
		}

		bit_rate_profiler.stop();

		ExhaustiveSearchSlice result_slice;
		memset(&result_slice, 0, sizeof(result_slice));
		for (int32_t i = 0; i < eMax; ++i)
		{
			for (ExhaustiveSearchSlice& slice : slices)
			{
				result_slice.merge(slice);
				total_result_slice.merge(slice);
			}
		}

		double avg_error[eMax] = { 0.0 };
		for (int32_t i = 0; i < eMax; ++i)
		{
			avg_error[i] = result_slice.total_bit_rate_error[i] / result_slice.num_bit_rate_samples;
		}

		printf("Bits: %2u       [Truth]      | Legacy     | Hack 1     | Hack 2     | Hack 3     | Hack 4     | Hack 5     | Hack 6     | Hack 7     | Hack 8\n", num_value_bits);
		if (print_avg)
			printf("Avg         -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", avg_error[eF32_Truth], avg_error[eF32_Legacy], avg_error[eF32_Hack1], avg_error[eF32_Hack2], avg_error[eF32_Hack3], avg_error[eF32_Hack4], avg_error[eF32_Hack5], avg_error[eF32_Hack6], avg_error[eF32_Hack7], avg_error[eF32_Hack8]);
		printf("Max         -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", result_slice.max_bit_rate_error[eF32_Truth], result_slice.max_bit_rate_error[eF32_Legacy], result_slice.max_bit_rate_error[eF32_Hack1], result_slice.max_bit_rate_error[eF32_Hack2], result_slice.max_bit_rate_error[eF32_Hack3], result_slice.max_bit_rate_error[eF32_Hack4], result_slice.max_bit_rate_error[eF32_Hack5], result_slice.max_bit_rate_error[eF32_Hack6], result_slice.max_bit_rate_error[eF32_Hack7], result_slice.max_bit_rate_error[eF32_Hack8]);
		printf("\n");

		printf("Worst Sample:               | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", result_slice.worst_sample_value[eF32_Legacy], result_slice.worst_sample_value[eF32_Hack1], result_slice.worst_sample_value[eF32_Hack2], result_slice.worst_sample_value[eF32_Hack3], result_slice.worst_sample_value[eF32_Hack4], result_slice.worst_sample_value[eF32_Hack5], result_slice.worst_sample_value[eF32_Hack6], result_slice.worst_sample_value[eF32_Hack7], result_slice.worst_sample_value[eF32_Hack8]);
		printf("Worst Segment Min:          | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", result_slice.worst_segment_min_value[eF32_Legacy], result_slice.worst_segment_min_value[eF32_Hack1], result_slice.worst_segment_min_value[eF32_Hack2], result_slice.worst_segment_min_value[eF32_Hack3], result_slice.worst_segment_min_value[eF32_Hack4], result_slice.worst_segment_min_value[eF32_Hack5], result_slice.worst_segment_min_value[eF32_Hack6], result_slice.worst_segment_min_value[eF32_Hack7], result_slice.worst_segment_min_value[eF32_Hack8]);
		printf("Worst Segment Extent:       | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", result_slice.worst_segment_extent_value[eF32_Legacy], result_slice.worst_segment_extent_value[eF32_Hack1], result_slice.worst_segment_extent_value[eF32_Hack2], result_slice.worst_segment_extent_value[eF32_Hack3], result_slice.worst_segment_extent_value[eF32_Hack4], result_slice.worst_segment_extent_value[eF32_Hack5], result_slice.worst_segment_extent_value[eF32_Hack6], result_slice.worst_segment_extent_value[eF32_Hack7], result_slice.worst_segment_extent_value[eF32_Hack8]);

		printf("Worst Clip Min:             | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", result_slice.worst_clip_min_value[eF32_Legacy], result_slice.worst_clip_min_value[eF32_Hack1], result_slice.worst_clip_min_value[eF32_Hack2], result_slice.worst_clip_min_value[eF32_Hack3], result_slice.worst_clip_min_value[eF32_Hack4], result_slice.worst_clip_min_value[eF32_Hack5], result_slice.worst_clip_min_value[eF32_Hack6], result_slice.worst_clip_min_value[eF32_Hack7], result_slice.worst_clip_min_value[eF32_Hack8]);
		printf("Worst Clip Min:             | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", *(int32_t*)&result_slice.worst_clip_min_value[eF32_Legacy], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack1], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack2], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack3], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack4], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack5], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack6], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack7], *(int32_t*)&result_slice.worst_clip_min_value[eF32_Hack8]);

		printf("Worst Clip Extent:          | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", result_slice.worst_clip_extent_value[eF32_Legacy], result_slice.worst_clip_extent_value[eF32_Hack1], result_slice.worst_clip_extent_value[eF32_Hack2], result_slice.worst_clip_extent_value[eF32_Hack3], result_slice.worst_clip_extent_value[eF32_Hack4], result_slice.worst_clip_extent_value[eF32_Hack5], result_slice.worst_clip_extent_value[eF32_Hack6], result_slice.worst_clip_extent_value[eF32_Hack7], result_slice.worst_clip_extent_value[eF32_Hack8]);
		printf("Worst Clip Extent:          | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Legacy], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack1], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack2], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack3], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack4], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack5], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack6], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack7], *(int32_t*)&result_slice.worst_clip_extent_value[eF32_Hack8]);

		{
			double elapsed_seconds = bit_rate_profiler.get_elapsed_seconds();
			int32_t elapsed_hours = int32_t(elapsed_seconds / (60.0 * 60.0));
			elapsed_seconds -= elapsed_hours * (60.0 * 60.0);
			int32_t elapsed_minutes = int32_t(elapsed_seconds / 60.0);
			elapsed_seconds -= elapsed_minutes * 60.0;
			printf("Completed in %uh %02um %.2fs\n", elapsed_hours, elapsed_minutes, elapsed_seconds);
		}

		printf("\n");
	}

	total_profiler.stop();

	{
		double avg_error[eMax] = { 0.0 };
		for (int32_t i = 0; i < eMax; ++i)
			avg_error[i] = total_result_slice.total_bit_rate_error[i] / total_result_slice.num_bit_rate_samples;

		printf("\n\n");
		printf("               [Truth]      | Legacy     | Hack 1     | Hack 2     | Hack 3     | Hack 4     | Hack 5     | Hack 6     | Hack 7     | Hack 8\n");
		if (print_avg)
			printf("Avg         -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", avg_error[eF32_Truth], avg_error[eF32_Legacy], avg_error[eF32_Hack1], avg_error[eF32_Hack2], avg_error[eF32_Hack3], avg_error[eF32_Hack4], avg_error[eF32_Hack5], avg_error[eF32_Hack6], avg_error[eF32_Hack7], avg_error[eF32_Hack8]);
		printf("Max         -> [%.8f] | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", total_result_slice.max_bit_rate_error[eF32_Truth], total_result_slice.max_bit_rate_error[eF32_Legacy], total_result_slice.max_bit_rate_error[eF32_Hack1], total_result_slice.max_bit_rate_error[eF32_Hack2], total_result_slice.max_bit_rate_error[eF32_Hack3], total_result_slice.max_bit_rate_error[eF32_Hack4], total_result_slice.max_bit_rate_error[eF32_Hack5], total_result_slice.max_bit_rate_error[eF32_Hack6], total_result_slice.max_bit_rate_error[eF32_Hack7], total_result_slice.max_bit_rate_error[eF32_Hack8]);
		printf("\n");

		printf("Worst Sample:               | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", total_result_slice.worst_sample_value[eF32_Legacy], total_result_slice.worst_sample_value[eF32_Hack1], total_result_slice.worst_sample_value[eF32_Hack2], total_result_slice.worst_sample_value[eF32_Hack3], total_result_slice.worst_sample_value[eF32_Hack4], total_result_slice.worst_sample_value[eF32_Hack5], total_result_slice.worst_sample_value[eF32_Hack6], total_result_slice.worst_sample_value[eF32_Hack7], total_result_slice.worst_sample_value[eF32_Hack8]);
		printf("Worst Segment Min:          | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", total_result_slice.worst_segment_min_value[eF32_Legacy], total_result_slice.worst_segment_min_value[eF32_Hack1], total_result_slice.worst_segment_min_value[eF32_Hack2], total_result_slice.worst_segment_min_value[eF32_Hack3], total_result_slice.worst_segment_min_value[eF32_Hack4], total_result_slice.worst_segment_min_value[eF32_Hack5], total_result_slice.worst_segment_min_value[eF32_Hack6], total_result_slice.worst_segment_min_value[eF32_Hack7], total_result_slice.worst_segment_min_value[eF32_Hack8]);
		printf("Worst Segment Extent:       | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", total_result_slice.worst_segment_extent_value[eF32_Legacy], total_result_slice.worst_segment_extent_value[eF32_Hack1], total_result_slice.worst_segment_extent_value[eF32_Hack2], total_result_slice.worst_segment_extent_value[eF32_Hack3], total_result_slice.worst_segment_extent_value[eF32_Hack4], total_result_slice.worst_segment_extent_value[eF32_Hack5], total_result_slice.worst_segment_extent_value[eF32_Hack6], total_result_slice.worst_segment_extent_value[eF32_Hack7], total_result_slice.worst_segment_extent_value[eF32_Hack8]);

		printf("Worst Clip Min:             | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", total_result_slice.worst_clip_min_value[eF32_Legacy], total_result_slice.worst_clip_min_value[eF32_Hack1], total_result_slice.worst_clip_min_value[eF32_Hack2], total_result_slice.worst_clip_min_value[eF32_Hack3], total_result_slice.worst_clip_min_value[eF32_Hack4], total_result_slice.worst_clip_min_value[eF32_Hack5], total_result_slice.worst_clip_min_value[eF32_Hack6], total_result_slice.worst_clip_min_value[eF32_Hack7], total_result_slice.worst_clip_min_value[eF32_Hack8]);
		printf("Worst Clip Min:             | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Legacy], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack1], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack2], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack3], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack4], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack5], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack6], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack7], *(int32_t*)&total_result_slice.worst_clip_min_value[eF32_Hack8]);

		printf("Worst Clip Extent:          | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f | %.8f\n", total_result_slice.worst_clip_extent_value[eF32_Legacy], total_result_slice.worst_clip_extent_value[eF32_Hack1], total_result_slice.worst_clip_extent_value[eF32_Hack2], total_result_slice.worst_clip_extent_value[eF32_Hack3], total_result_slice.worst_clip_extent_value[eF32_Hack4], total_result_slice.worst_clip_extent_value[eF32_Hack5], total_result_slice.worst_clip_extent_value[eF32_Hack6], total_result_slice.worst_clip_extent_value[eF32_Hack7], total_result_slice.worst_clip_extent_value[eF32_Hack8]);
		printf("Worst Clip Extent:          | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X | 0x%08X\n", *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Legacy], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack1], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack2], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack3], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack4], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack5], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack6], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack7], *(int32_t*)&total_result_slice.worst_clip_extent_value[eF32_Hack8]);

		{
			double elapsed_seconds = total_profiler.get_elapsed_seconds();
			int32_t elapsed_hours = int32_t(elapsed_seconds / (60.0 * 60.0));
			elapsed_seconds -= elapsed_hours * (60.0 * 60.0);
			int32_t elapsed_minutes = int32_t(elapsed_seconds / 60.0);
			elapsed_seconds -= elapsed_minutes * 60.0;
			printf("Completed in %uh %02um %.2fs\n", elapsed_hours, elapsed_minutes, elapsed_seconds);
		}
	}
}

static int main_impl(int argc, char** argv)
{
	if (k_exhaustive_accuracy_test)
		test_exhaustive();
	else
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
