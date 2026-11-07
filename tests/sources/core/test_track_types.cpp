////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2026 Nicholas Frechette & Animation Compression Library contributors
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

#include "catch2.impl.h"

#include <acl/core/track_types.h>

#include <cstring>

using namespace acl;

TEST_CASE("track type names", "[core][track_types]")
{
	CHECK(std::strcmp(get_track_type_name(track_type8::float1f), "float1f") == 0);
	CHECK(std::strcmp(get_track_type_name(track_type8::vector4f), "vector4f") == 0);
	CHECK(std::strcmp(get_track_type_name(track_type8::vqvf), "vqvf") == 0);

	track_type8 type = track_type8::float1f;
	CHECK(get_track_type("vqvf", type) == true);
	CHECK(type == track_type8::vqvf);

	// The reader must still accept the legacy name
	type = track_type8::float1f;
	CHECK(get_track_type("qvvf", type) == true);
	CHECK(type == track_type8::vqvf);

	type = track_type8::vqvf;
	CHECK(get_track_type("float3f", type) == true);
	CHECK(type == track_type8::float3f);

	CHECK(get_track_type("not_a_type", type) == false);

	// Each name must convert back to its track type
	const track_type8 types[] = { track_type8::float1f, track_type8::float2f, track_type8::float3f, track_type8::float4f, track_type8::vector4f, track_type8::vqvf };
	for (const track_type8 expected_type : types)
	{
		CHECK(get_track_type(get_track_type_name(expected_type), type) == true);
		CHECK(type == expected_type);
	}
}
