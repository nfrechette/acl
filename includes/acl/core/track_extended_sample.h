#pragma once

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

#include "acl/version.h"
#include "acl/core/error.h"
#include "acl/core/sample_interpolator.h"
#include "acl/core/impl/compiler_utils.h"

#include <cstdint>

ACL_IMPL_FILE_PRAGMA_PUSH

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	// A constant interpolator sample
	template<typename sample_type_t>
	struct sample_constant_data_t
	{
		sample_type_t value;
	};

	// A linear interpolator sample
	template<typename sample_type_t>
	struct sample_linear_data_t
	{
		sample_type_t value;
	};

	// A type to encapsulate an extended track sample.
	// Basic tracks always linearly interpolate while extended tracks can specify
	// the behavior per sample. Each track sample is uniformly distributed according
	// to its track sample rate.
	template<typename sample_type_t>
	struct track_extended_sample_t
	{
		// The size in bytes of a sample value
		static constexpr size_t sample_size = sizeof(sample_type_t);

		// The sample interpolator function
		sample_interpolator_t interpolator = sample_interpolator_t::linear;

		// A union of all interpolator sample types
		union sample_data_t
		{
			sample_constant_data_t<sample_type_t> constant;
			sample_linear_data_t<sample_type_t> linear;

			// TODO: Add bezier/hermite/etc support
		};

		// Our sample data
		sample_data_t data;

		// Utility helpers to get/set data safely

		void set_constant(const sample_constant_data_t<sample_type_t>& data);
		const sample_constant_data_t<sample_type_t>& get_constant() const;

		void set_linear(const sample_linear_data_t<sample_type_t>& data);
		const sample_linear_data_t<sample_type_t>& get_linear() const;

		sample_type_t get_value() const;
	};

	ACL_IMPL_VERSION_NAMESPACE_END
}

#include "acl/core/impl/track_extended_sample.impl.h"

ACL_IMPL_FILE_PRAGMA_POP
