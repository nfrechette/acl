#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2022 Nicholas Frechette & Animation Compression Library contributors
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

////////////////////////////////////////////////////////////////////////////////
// Macros to detect the ACL version
////////////////////////////////////////////////////////////////////////////////

#define ACL_VERSION_MAJOR 2
#define ACL_VERSION_MINOR 0
#define ACL_VERSION_PATCH 2

////////////////////////////////////////////////////////////////////////////////
// In order to allow multiple versions of this library to coexist side by side
// within the same executable/library, the symbols have to be unique per version.
// We achieve this by using a versioned namespace that we optionally inline.
// To disable namespace inlining, define ACL_NO_INLINE_NAMESPACE before including
// any ACL header.
////////////////////////////////////////////////////////////////////////////////

// Name of the namespace, e.g. v20
#define ACL_IMPL_VERSION_NAMESPACE_NAME v ## ACL_VERSION_MAJOR ## ACL_VERSION_MINOR

#if defined(ACL_NO_INLINE_NAMESPACE)
    // Namespace won't be inlined, its usage will have to be qualified with the
    // full version everywhere
    #define ACL_IMPL_NAMESPACE acl::ACL_IMPL_VERSION_NAMESPACE_NAME

    #define ACL_IMPL_VERSION_NAMESPACE_BEGIN \
        namespace ACL_IMPL_VERSION_NAMESPACE_NAME \
        {
#else
    // Namespace is inlined, its usage does not need to be qualified with the
    // full version everywhere
    #define ACL_IMPL_NAMESPACE acl

    #define ACL_IMPL_VERSION_NAMESPACE_BEGIN \
        inline namespace ACL_IMPL_VERSION_NAMESPACE_NAME \
        {
#endif

#define ACL_IMPL_VERSION_NAMESPACE_END \
    }
