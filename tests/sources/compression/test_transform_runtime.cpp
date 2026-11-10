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

#include <acl/compression/compression_settings.h>
#include <acl/compression/pre_process.h>
#include <acl/compression/track_array.h>
#include <acl/compression/transform_adapters.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/compression/transform_metrics.h>
#include <acl/compression/impl/transform_runtime.h>
#include <acl/core/ansi_allocator.h>

#include <rtm/scalarf.h>
#include <rtm/vector4f.h>
#include <rtm/vqmf.h>

#include <cstdint>

using namespace acl;
using namespace acl::acl_impl;

namespace
{
	// A custom adapter: VQM math through the interface
	struct test_custom_adapter final : public itransform_adapter
	{
		virtual const char* get_name() const override { return "custom_transform_adapter"; }
		virtual size_t get_transform_size() const override { return sizeof(rtm::vqmf); }
		virtual bool is_associative() const override { return true; }
		virtual void identity(void* out_transform) const override { *static_cast<rtm::vqmf*>(out_transform) = rtm::vqm_identity(); }

		virtual void mul(const void* lhs, const void* rhs, void* out_transform) const override
		{
			*static_cast<rtm::vqmf*>(out_transform) = rtm::vqm_mul(*static_cast<const rtm::vqmf*>(lhs), *static_cast<const rtm::vqmf*>(rhs));
		}

		virtual void RTM_SIMD_CALL mul_point3_x4(
			rtm::vector4f_arg0 point_xxxx, rtm::vector4f_arg1 point_yyyy, rtm::vector4f_arg2 point_zzzz,
			const void* transform,
			rtm::vector4f& out_result_xxxx, rtm::vector4f& out_result_yyyy, rtm::vector4f& out_result_zzzz) const override
		{
			acl_impl::vqm_mul_point3_x4(point_xxxx, point_yyyy, point_zzzz, *static_cast<const rtm::vqmf*>(transform), out_result_xxxx, out_result_yyyy, out_result_zzzz);
		}
	};

	// A custom metric that supports only custom adapters
	struct custom_only_transform_metric final : public itransform_metric
	{
		virtual const char* get_name() const override { return "custom_only_transform_metric"; }
		virtual bool is_supported(const itransform_adapter& adapter) const override { return adapter.get_type() == transform_runtime_type_t::custom; }
		virtual rtm::scalarf RTM_SIMD_CALL calculate_error_squared(const calculate_error_args& args) const override { (void)args; return rtm::scalar_set(0.0F); }
	};

	struct selection_result
	{
		bool is_valid;
		transform_runtime_type_t adapter_type;
		const itransform_metric* metric;
	};

	selection_result select(
		const transform_runtime_selector& selector,
		transform_runtime_type_t type, const itransform_adapter* custom_adapter, const itransform_metric* metric,
		track_type8 source_type, bool has_scale, bool has_non_uniform_base_scale)
	{
		transform_runtime_selection selection;
		const error_result result = selector.select(type, custom_adapter, metric, source_type, has_scale, has_non_uniform_base_scale, selection);

		selection_result output;
		output.is_valid = result.empty();
		output.adapter_type = selection.adapter != nullptr ? selection.adapter->get_type() : transform_runtime_type_t::automatic;
		output.metric = selection.metric;
		return output;
	}
}

TEST_CASE("transform runtime built-in types", "[compression][transform]")
{
	const transform_runtime_selector selector;

	const transform_runtime_type_t builtin_types[] =
	{
		transform_runtime_type_t::vqf,
		transform_runtime_type_t::vqsf,
		transform_runtime_type_t::vqvf,
		transform_runtime_type_t::vqmf,
		transform_runtime_type_t::matrix3x4f,
	};

	for (transform_runtime_type_t type : builtin_types)
	{
		const itransform_adapter* adapter = selector.get_builtin_adapter(type);
		REQUIRE(adapter != nullptr);
		CHECK(adapter->get_type() == type);

		// The default metric of each type supports it
		CHECK(selector.get_default_metric(type)->is_supported(*adapter));
	}

	CHECK(selector.get_builtin_adapter(transform_runtime_type_t::custom) == nullptr);
	CHECK(selector.get_builtin_adapter(transform_runtime_type_t::automatic) == nullptr);

	// The precise metric with VQ and VQS, the shell metric otherwise
	const precise_transform_metric precise_metric;
	const shell_transform_metric shell_metric;
	CHECK(selector.get_default_metric(transform_runtime_type_t::vqf)->get_hash() == precise_metric.get_hash());
	CHECK(selector.get_default_metric(transform_runtime_type_t::vqsf)->get_hash() == precise_metric.get_hash());
	CHECK(selector.get_default_metric(transform_runtime_type_t::vqvf)->get_hash() == shell_metric.get_hash());
	CHECK(selector.get_default_metric(transform_runtime_type_t::vqmf)->get_hash() == shell_metric.get_hash());
	CHECK(selector.get_default_metric(transform_runtime_type_t::matrix3x4f)->get_hash() == shell_metric.get_hash());
	CHECK(selector.get_default_metric(transform_runtime_type_t::custom)->get_hash() == shell_metric.get_hash());
}

TEST_CASE("transform runtime validation", "[compression][transform]")
{
	const transform_runtime_selector selector;
	test_custom_adapter custom_adapter;
	const precise_transform_metric precise_metric;
	const shell_transform_metric shell_metric;

	CHECK(selector.validate(transform_runtime_type_t::automatic, nullptr, nullptr).empty());
	CHECK(selector.validate(transform_runtime_type_t::vqvf, nullptr, nullptr).empty());
	CHECK(selector.validate(transform_runtime_type_t::custom, &custom_adapter, nullptr).empty());

	// A value that is not valid
	CHECK(selector.validate(transform_runtime_type_t(200), nullptr, nullptr).any());

	// The custom type and the custom adapter go together
	CHECK(selector.validate(transform_runtime_type_t::custom, nullptr, nullptr).any());
	CHECK(selector.validate(transform_runtime_type_t::vqmf, &custom_adapter, nullptr).any());
	CHECK(selector.validate(transform_runtime_type_t::automatic, &custom_adapter, nullptr).any());

	// The metric must support the type when we know the type
	CHECK(selector.validate(transform_runtime_type_t::vqsf, nullptr, &precise_metric).empty());
	CHECK(selector.validate(transform_runtime_type_t::vqmf, nullptr, &precise_metric).any());
	CHECK(selector.validate(transform_runtime_type_t::custom, &custom_adapter, &precise_metric).any());
	CHECK(selector.validate(transform_runtime_type_t::custom, &custom_adapter, &shell_metric).empty());
	CHECK(selector.validate(transform_runtime_type_t::automatic, nullptr, &precise_metric).empty());
}

TEST_CASE("transform runtime selection", "[compression][transform]")
{
	const transform_runtime_selector selector;
	test_custom_adapter custom_adapter;
	const custom_only_transform_metric custom_metric;
	const precise_transform_metric precise_metric;
	const shell_transform_metric shell_metric;

	// Without scale, we use VQ with the precise metric, for every runtime type
	{
		const transform_runtime_type_t types[] = { transform_runtime_type_t::automatic, transform_runtime_type_t::vqvf, transform_runtime_type_t::vqmf, transform_runtime_type_t::matrix3x4f };
		for (transform_runtime_type_t type : types)
		{
			const selection_result result = select(selector, type, nullptr, nullptr, track_type8::vqvf, false, false);
			CHECK(result.is_valid);
			CHECK(result.adapter_type == transform_runtime_type_t::vqf);
			CHECK(result.metric->get_hash() == precise_metric.get_hash());
		}

		const selection_result custom_result = select(selector, transform_runtime_type_t::custom, &custom_adapter, nullptr, track_type8::vqvf, false, false);
		CHECK(custom_result.is_valid);
		CHECK(custom_result.adapter_type == transform_runtime_type_t::vqf);

		// The metric that we select applies with VQ too
		const selection_result shell_result = select(selector, transform_runtime_type_t::vqmf, nullptr, &shell_metric, track_type8::vqvf, false, false);
		CHECK(shell_result.is_valid);
		CHECK(shell_result.adapter_type == transform_runtime_type_t::vqf);
		CHECK(shell_result.metric == &shell_metric);

		// A metric that does not support VQ keeps the custom adapter
		const selection_result custom_metric_result = select(selector, transform_runtime_type_t::custom, &custom_adapter, &custom_metric, track_type8::vqvf, false, false);
		CHECK(custom_metric_result.is_valid);
		CHECK(custom_metric_result.adapter_type == transform_runtime_type_t::custom);
		CHECK(custom_metric_result.metric == &custom_metric);
	}

	// With scale and 'automatic', we use VQS for VQS tracks and VQM otherwise
	{
		const selection_result vqs_result = select(selector, transform_runtime_type_t::automatic, nullptr, nullptr, track_type8::vqsf, true, false);
		CHECK(vqs_result.is_valid);
		CHECK(vqs_result.adapter_type == transform_runtime_type_t::vqsf);
		CHECK(vqs_result.metric->get_hash() == precise_metric.get_hash());

		// An additive base with a scale that is not uniform gives a scale that is not uniform
		const selection_result base_result = select(selector, transform_runtime_type_t::automatic, nullptr, nullptr, track_type8::vqsf, true, true);
		CHECK(base_result.is_valid);
		CHECK(base_result.adapter_type == transform_runtime_type_t::vqmf);
		CHECK(base_result.metric->get_hash() == shell_metric.get_hash());

		const selection_result vqv_result = select(selector, transform_runtime_type_t::automatic, nullptr, nullptr, track_type8::vqvf, true, false);
		CHECK(vqv_result.is_valid);
		CHECK(vqv_result.adapter_type == transform_runtime_type_t::vqmf);
		CHECK(vqv_result.metric->get_hash() == shell_metric.get_hash());

		// VQ tracks have a scale only with an additive base
		const selection_result vq_result = select(selector, transform_runtime_type_t::automatic, nullptr, nullptr, track_type8::vqf, true, false);
		CHECK(vq_result.is_valid);
		CHECK(vq_result.adapter_type == transform_runtime_type_t::vqmf);
	}

	// With scale and a type that we select
	{
		const selection_result vqv_result = select(selector, transform_runtime_type_t::vqvf, nullptr, nullptr, track_type8::vqvf, true, false);
		CHECK(vqv_result.is_valid);
		CHECK(vqv_result.adapter_type == transform_runtime_type_t::vqvf);
		CHECK(vqv_result.metric->get_hash() == shell_metric.get_hash());

		const selection_result custom_result = select(selector, transform_runtime_type_t::custom, &custom_adapter, nullptr, track_type8::vqvf, true, false);
		CHECK(custom_result.is_valid);
		CHECK(custom_result.adapter_type == transform_runtime_type_t::custom);
		CHECK(custom_result.metric->get_hash() == shell_metric.get_hash());

		// The shell metric with VQS
		const selection_result shell_result = select(selector, transform_runtime_type_t::vqsf, nullptr, &shell_metric, track_type8::vqsf, true, false);
		CHECK(shell_result.is_valid);
		CHECK(shell_result.adapter_type == transform_runtime_type_t::vqsf);
		CHECK(shell_result.metric == &shell_metric);

		// VQS needs VQS tracks and a uniform base scale
		CHECK(!select(selector, transform_runtime_type_t::vqsf, nullptr, nullptr, track_type8::vqvf, true, false).is_valid);
		CHECK(!select(selector, transform_runtime_type_t::vqsf, nullptr, nullptr, track_type8::vqsf, true, true).is_valid);

		// The precise metric does not support VQM
		CHECK(!select(selector, transform_runtime_type_t::automatic, nullptr, &precise_metric, track_type8::vqvf, true, false).is_valid);
	}
}

TEST_CASE("transform runtime settings", "[compression][transform]")
{
	test_custom_adapter custom_adapter;
	precise_transform_metric precise_metric;
	vqvf_transform_error_metric legacy_metric;

	{
		const compression_settings settings = get_default_compression_settings();
		CHECK(settings.transform_runtime_type == transform_runtime_type_t::automatic);
		CHECK(settings.custom_transform_adapter == nullptr);
		CHECK(settings.transform_metric == nullptr);
		CHECK(settings.is_valid().empty());
	}

	{
		compression_settings settings = get_default_compression_settings();
		settings.transform_runtime_type = transform_runtime_type_t::custom;
		CHECK(settings.is_valid().any());

		settings.custom_transform_adapter = &custom_adapter;
		CHECK(settings.is_valid().empty());
	}

	{
		compression_settings settings = get_default_compression_settings();
		settings.transform_runtime_type = transform_runtime_type_t::vqmf;
		settings.transform_metric = &precise_metric;
		CHECK(settings.is_valid().any());
	}

	// The legacy error metric does not go with the new settings
	{
		compression_settings settings = get_default_compression_settings();
		settings.error_metric = &legacy_metric;
		CHECK(settings.is_valid().empty());

		settings.transform_runtime_type = transform_runtime_type_t::vqvf;
		CHECK(settings.is_valid().any());
	}

	// The hash changes with the runtime type and the metric
	{
		const compression_settings settings0 = get_default_compression_settings();

		compression_settings settings1 = get_default_compression_settings();
		settings1.transform_runtime_type = transform_runtime_type_t::vqvf;

		compression_settings settings2 = get_default_compression_settings();
		settings2.transform_metric = &precise_metric;

		CHECK(settings0.get_hash() != settings1.get_hash());
		CHECK(settings0.get_hash() != settings2.get_hash());
	}

	// Pre-processing has the same rules
	{
		ansi_allocator allocator;
		track_array_vqvf track_list(allocator, 0);

		pre_process_settings_t settings;
		settings.precision_policy = pre_process_precision_policy::lossy;
		CHECK(pre_process_track_list(allocator, settings, track_list).empty());

		settings.transform_runtime_type = transform_runtime_type_t::custom;
		CHECK(pre_process_track_list(allocator, settings, track_list).any());

		settings.custom_transform_adapter = &custom_adapter;
		CHECK(pre_process_track_list(allocator, settings, track_list).empty());

		settings.error_metric = &legacy_metric;
		CHECK(pre_process_track_list(allocator, settings, track_list).any());
	}
}
