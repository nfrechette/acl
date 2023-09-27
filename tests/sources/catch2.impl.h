#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2023 Nicholas Frechette & Animation Compression Library contributors
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

// We disable various informational warnings with using /Wall with MSVC caused by Catch2

#if defined(_MSC_VER) && !defined(__clang__)
	#pragma warning(push)
	#pragma warning(disable : 4365)	// Signed/unsigned mismatch
	#pragma warning(disable : 4388)	// Signed/unsigned comparison
	#pragma warning(disable : 4583)	// Destructor is not implicitly called
	#pragma warning(disable : 4623)	// Default constructor implicitly deleted
	#pragma warning(disable : 4625)	// Copy constructor implicitly deleted
	#pragma warning(disable : 4626)	// Copy assignment operator implicitly deleted
	#pragma warning(disable : 4868)	// May not enforce left to right order in initializer
	#pragma warning(disable : 5026)	// Move constructor implicitly deleted
	#pragma warning(disable : 5027)	// Move assignment operator implicitly deleted
	#pragma warning(disable : 5039)	// Pointer to potentially throwing function passed to extern C
	#pragma warning(disable : 5204)	// Class has virtual functions but no virtual destructor
	#pragma warning(disable : 5219)	// Implicit conversion, possible loss of data
	#pragma warning(disable : 5267)	// Implicit copy constructor deprecated due to user destructor
#endif

#include <catch2/catch.hpp>

#if defined(_MSC_VER) && !defined(__clang__)
	#pragma warning(pop)
#endif
