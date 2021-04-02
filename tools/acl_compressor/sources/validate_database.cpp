////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2021 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl_compressor.h"

#include "acl/core/compressed_database.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/floating_point_exceptions.h"
#include "acl/core/iallocator.h"
#include "acl/compression/compress.h"
#include "acl/compression/track_array.h"
#include "acl/compression/track_error.h"
#include "acl/compression/transform_error_metrics.h"
#include "acl/decompression/decompress.h"
#include "acl/decompression/database/database.h"
#include "acl/decompression/database/null_database_streamer.h"	// Just to test compilation
#include "acl/decompression/database/impl/debug_database_streamer.h"

using namespace acl;

#if defined(ACL_USE_SJSON) && defined(ACL_HAS_ASSERT_CHECKS)
struct debug_transform_decompression_settings_with_db : public acl::debug_transform_decompression_settings
{
	using database_settings_type = acl::debug_database_settings;
};

static void stream_in_database_tier(database_context<debug_database_settings>& context, const debug_database_streamer& streamer, const compressed_database& db, quality_tier tier)
{
	const uint32_t num_chunks = db.get_num_chunks(tier);

	bool is_streamed_in = context.is_streamed_in(tier);
	ACL_ASSERT((num_chunks == 0 && is_streamed_in) || !is_streamed_in, "Tier should not be streamed in");
	ACL_ASSERT(streamer.get_bulk_data(tier) == nullptr, "Bulk data should not be allocated");

	acl::database_stream_request_result stream_in_result = context.stream_in(tier, 2);
	const uint8_t* streamer_bulk_data = streamer.get_bulk_data(tier);

	ACL_ASSERT((num_chunks == 0 && stream_in_result == database_stream_request_result::done) || stream_in_result == acl::database_stream_request_result::dispatched, "Failed to stream in tier");
	ACL_ASSERT(num_chunks == 0 || streamer.get_bulk_data(tier) != nullptr, "Bulk data should be allocated");

	is_streamed_in = context.is_streamed_in(tier);
	ACL_ASSERT((num_chunks <= 2 && is_streamed_in) || !is_streamed_in, "Failed to stream in tier (first 2 chunks)");

	stream_in_result = context.stream_in(tier);

	ACL_ASSERT((num_chunks <= 2 && stream_in_result == database_stream_request_result::done) || stream_in_result == acl::database_stream_request_result::dispatched, "Failed to stream in tier");
	ACL_ASSERT(num_chunks == 0 || streamer.get_bulk_data(tier) != nullptr, "Bulk data should be allocated");
	ACL_ASSERT(streamer.get_bulk_data(tier) == streamer_bulk_data, "Bulk data should not have been reallocated");

	is_streamed_in = context.is_streamed_in(tier);
	ACL_ASSERT(is_streamed_in, "Failed to stream in tier");
}

static void stream_out_database_tier(database_context<debug_database_settings>& context, const debug_database_streamer& streamer, const compressed_database& db, quality_tier tier)
{
	const uint8_t* streamer_bulk_data = streamer.get_bulk_data(tier);
	const uint32_t num_chunks = db.get_num_chunks(tier);

	const bool is_streamed_in = context.is_streamed_in(tier);
	ACL_ASSERT(is_streamed_in, "Tier should be streamed in");
	ACL_ASSERT(num_chunks == 0 || streamer.get_bulk_data(tier) != nullptr, "Bulk data should be allocated");

	acl::database_stream_request_result stream_out_result = context.stream_out(tier, 2);

	ACL_ASSERT((num_chunks == 0 && stream_out_result == database_stream_request_result::done) || stream_out_result == acl::database_stream_request_result::dispatched, "Failed to stream out tier 1");
	if (num_chunks <= 2)
	{
		ACL_ASSERT(streamer.get_bulk_data(tier) == nullptr, "Bulk data should not be allocated");
	}
	else
	{
		ACL_ASSERT(streamer.get_bulk_data(tier) != nullptr, "Bulk data should be allocated");
		ACL_ASSERT(streamer.get_bulk_data(tier) == streamer_bulk_data, "Bulk data should not have been reallocated");
	}

	bool is_streamed_out = !context.is_streamed_in(tier);
	ACL_ASSERT(num_chunks == 0 || is_streamed_out, "Failed to stream out tier 1 (first 2 chunks)");

	stream_out_result = context.stream_out(tier);

	ACL_ASSERT((num_chunks <= 2 && stream_out_result == database_stream_request_result::done) || stream_out_result == acl::database_stream_request_result::dispatched, "Failed to stream out tier 1");
	ACL_ASSERT(streamer.get_bulk_data(tier) == nullptr, "Bulk data should not be allocated");

	is_streamed_out = !context.is_streamed_in(tier);
	ACL_ASSERT(num_chunks == 0 || is_streamed_out, "Failed to stream out tier 1");
}

static void validate_db_streaming(iallocator& allocator, const track_array_qvvf& raw_tracks, const track_array_qvvf& additive_base_tracks, const itransform_error_metric& error_metric,
	const track_error& high_quality_tier_error_ref,
	const compressed_tracks& tracks0, const compressed_tracks& tracks1,
	const compressed_database& db, const uint8_t* db_bulk_data_medium, const uint8_t* db_bulk_data_low)
{
	// Our desired error threshold
	// When intrinsics aren't used with x86, the floating point arithmetic falls back to
	// using x87 instructions. When this happens, depending on how code is generated some
	// small inaccuracies can pop up because rounding happens when we store to memory.
	// The full pose decompression stores samples into the stack while working with them
	// while the single track decompression does not which causes the issue.
	// With SSE2 and NEON, there are no such rounding issues.
#if !defined(RTM_SSE2_INTRINSICS) && defined(RTM_ARCH_X86)
	const float threshold = 1.0E-3F;
#else
	const float threshold = 1.0E-4F;
#endif

	decompression_context<debug_transform_decompression_settings_with_db> context0;
	decompression_context<debug_transform_decompression_settings_with_db> context1;
	database_context<acl::debug_database_settings> db_context;
	debug_database_streamer db_medium_streamer(allocator, db_bulk_data_medium, db.get_bulk_data_size(quality_tier::medium_importance));
	debug_database_streamer db_low_streamer(allocator, db_bulk_data_low, db.get_bulk_data_size(quality_tier::lowest_importance));
	ACL_ASSERT(db_medium_streamer.get_bulk_data(acl::quality_tier::medium_importance) == nullptr, "Bulk data should not be allocated");
	ACL_ASSERT(db_low_streamer.get_bulk_data(acl::quality_tier::lowest_importance) == nullptr, "Bulk data should not be allocated");

	bool initialized = db_context.initialize(allocator, db, db_medium_streamer, db_low_streamer);
	initialized = initialized && context0.initialize(tracks0, db_context);
	initialized = initialized && context1.initialize(tracks1, db_context);
	ACL_ASSERT(initialized, "Failed to initialize decompression context");
	ACL_ASSERT(!db_context.is_streamed_in(quality_tier::medium_importance) || db.get_num_chunks(quality_tier::medium_importance) == 0, "Tier shouldn't be streamed in yet");
	ACL_ASSERT(!db_context.is_streamed_in(quality_tier::lowest_importance) || db.get_num_chunks(quality_tier::lowest_importance) == 0, "Tier shouldn't be streamed in yet");

	// Nothing is streamed in yet, we have low quality
	const track_error low_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
	ACL_ASSERT(low_quality_tier_error0.error >= high_quality_tier_error_ref.error, "Low quality tier split error should be higher or equal to high quality tier inline");
	const track_error low_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
	ACL_ASSERT(low_quality_tier_error1.error >= high_quality_tier_error_ref.error, "Low quality tier split error should be higher or equal to high quality tier inline");

	// Stream in our medium importance tier
	stream_in_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

	const track_error medium_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
	ACL_ASSERT(medium_quality_tier_error0.error >= high_quality_tier_error_ref.error, "Medium quality tier split error should be higher or equal to high quality tier inline");
	const track_error medium_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
	ACL_ASSERT(medium_quality_tier_error1.error >= high_quality_tier_error_ref.error, "Medium quality tier split error should be higher or equal to high quality tier inline");

	ACL_ASSERT(low_quality_tier_error0.error >= medium_quality_tier_error0.error, "Low quality tier split error should be higher or equal to medium quality tier split error");
	ACL_ASSERT(low_quality_tier_error1.error >= medium_quality_tier_error1.error, "Low quality tier split error should be higher or equal to medium quality tier split error");

	// Stream in our low importance tier, restoring the full high quality
	stream_in_database_tier(db_context, db_low_streamer, db, quality_tier::lowest_importance);

	{
		const track_error high_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(high_quality_tier_error0.error, high_quality_tier_error_ref.error, threshold), "High quality tier split error should be equal to high quality tier inline");
		const track_error high_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(high_quality_tier_error1.error, high_quality_tier_error_ref.error, threshold), "High quality tier split error should be equal to high quality tier inline");
	}

	// Stream out our medium importance tier, we'll have mixed quality
	stream_out_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

	{
		// Not guaranteed to always be the case due to linear interpolation
		//const track_error mixed_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		//const track_error mixed_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);

		//ACL_ASSERT(mixed_quality_tier_error0.error >= high_quality_tier_error_ref.error, "Mixed quality split error should be higher or equal to high quality tier inline");
		//ACL_ASSERT(mixed_quality_tier_error1.error >= high_quality_tier_error_ref.error, "Mixed quality split error should be higher or equal to high quality tier inline");

		//ACL_ASSERT(low_quality_tier_error0.error >= mixed_quality_tier_error0.error, "Low quality tier split error should be higher or equal to mixed quality split error");
		//ACL_ASSERT(low_quality_tier_error1.error >= mixed_quality_tier_error1.error, "Low quality tier split error should be higher or equal to mixed quality split error");
	}

	// Stream in our medium importance tier, restoring the full high quality
	stream_in_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

	{
		const track_error high_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(high_quality_tier_error0.error, high_quality_tier_error_ref.error, threshold), "High quality tier split error should be equal to high quality tier inline");
		const track_error high_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(high_quality_tier_error1.error, high_quality_tier_error_ref.error, threshold), "High quality tier split error should be equal to high quality tier inline");
	}

	// Stream out our low importance tier, restoring medium quality
	stream_out_database_tier(db_context, db_low_streamer, db, quality_tier::lowest_importance);

	{
		const track_error medium_quality_tier_error0_ = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		ACL_ASSERT(medium_quality_tier_error0_.error == medium_quality_tier_error0.error, "Medium quality should be restored");
		const track_error medium_quality_tier_error1_ = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
		ACL_ASSERT(medium_quality_tier_error1_.error == medium_quality_tier_error1.error, "Medium quality should be restored");
	}

	// Stream out our medium importance tier, restoring low quality
	stream_out_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

	{
		const track_error low_quality_tier_error0_ = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		ACL_ASSERT(low_quality_tier_error0_.error == low_quality_tier_error0.error, "Low quality should be restored");
		const track_error low_quality_tier_error1_ = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
		ACL_ASSERT(low_quality_tier_error1_.error == low_quality_tier_error1.error, "Low quality should be restored");
	}
}

static void validate_db_stripping(iallocator& allocator, const track_array_qvvf& raw_tracks, const track_array_qvvf& additive_base_tracks, const itransform_error_metric& error_metric,
	const compressed_tracks& tracks0, const compressed_tracks& tracks1,
	const compressed_database& db, const uint8_t* db_bulk_data_medium, const uint8_t* db_bulk_data_low)
{
	compressed_database* db_no_medium = nullptr;
	compressed_database* db_no_low = nullptr;
	compressed_database* db_neither0 = nullptr;
	compressed_database* db_neither1 = nullptr;

	track_error low_quality_tier_error_ref0;
	track_error low_quality_tier_error_ref1;
	track_error medium_quality_tier_error_ref0;
	track_error medium_quality_tier_error_ref1;
	track_error mixed_quality_tier_error_ref0;
	track_error mixed_quality_tier_error_ref1;

	// Grab our reference values before we strip anything
	{
		decompression_context<debug_transform_decompression_settings_with_db> context0;
		decompression_context<debug_transform_decompression_settings_with_db> context1;
		database_context<acl::debug_database_settings> db_context;
		debug_database_streamer db_medium_streamer(allocator, db_bulk_data_medium, db.get_bulk_data_size(quality_tier::medium_importance));
		debug_database_streamer db_low_streamer(allocator, db_bulk_data_low, db.get_bulk_data_size(quality_tier::lowest_importance));

		bool initialized = db_context.initialize(allocator, db, db_medium_streamer, db_low_streamer);
		initialized = initialized && context0.initialize(tracks0, db_context);
		initialized = initialized && context1.initialize(tracks1, db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		// Nothing is streamed in yet, we have low quality
		low_quality_tier_error_ref0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		low_quality_tier_error_ref1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);

		// Stream in our medium importance tier
		stream_in_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

		medium_quality_tier_error_ref0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		medium_quality_tier_error_ref1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);

		// Stream in our low importance tier, restoring the full high quality
		stream_in_database_tier(db_context, db_low_streamer, db, quality_tier::lowest_importance);

		// Stream out our medium importance tier, we'll have mixed quality
		stream_out_database_tier(db_context, db_medium_streamer, db, quality_tier::medium_importance);

		mixed_quality_tier_error_ref0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		mixed_quality_tier_error_ref1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
	}

	// Strip medium tier
	if (db.has_bulk_data(quality_tier::medium_importance))
	{
		const error_result result = strip_database_quality_tier(allocator, db, quality_tier::medium_importance, db_no_medium);
		ACL_ASSERT(result.empty(), result.c_str());

		decompression_context<debug_transform_decompression_settings_with_db> context0;
		decompression_context<debug_transform_decompression_settings_with_db> context1;
		database_context<acl::debug_database_settings> db_context;
		debug_database_streamer db_medium_streamer(allocator, db_bulk_data_medium, db_no_medium->get_bulk_data_size(quality_tier::medium_importance));
		debug_database_streamer db_low_streamer(allocator, db_bulk_data_low, db_no_medium->get_bulk_data_size(quality_tier::lowest_importance));

		bool initialized = db_context.initialize(allocator, *db_no_medium, db_medium_streamer, db_low_streamer);
		initialized = initialized && context0.initialize(tracks0, db_context);
		initialized = initialized && context1.initialize(tracks1, db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		// Nothing is streamed in yet, we have low quality
		{
			const track_error low_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Low quality tier stripped error should be equal to low quality tier whole");
			const track_error low_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Low quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream in our medium importance tier
		stream_in_database_tier(db_context, db_medium_streamer, *db_no_medium, quality_tier::medium_importance);

		{
			const track_error medium_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Medium quality tier stripped error should be equal to low quality tier whole");
			const track_error medium_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Medium quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream in our low importance tier, restoring the full high quality
		stream_in_database_tier(db_context, db_low_streamer, *db_no_medium, quality_tier::lowest_importance);

		{
			const track_error high_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error0.error == mixed_quality_tier_error_ref0.error, "High quality tier stripped error should be equal to mixed quality tier whole");
			const track_error high_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error1.error == mixed_quality_tier_error_ref1.error, "High quality tier stripped error should be equal to mixed quality tier whole");
		}

		// Stream out our medium importance tier, we'll have mixed quality
		stream_out_database_tier(db_context, db_medium_streamer, *db_no_medium, quality_tier::medium_importance);

		{
			const track_error mixed_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error0.error == mixed_quality_tier_error_ref0.error, "Mixed quality tier stripped error should be equal to mixed quality tier whole");
			const track_error mixed_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error1.error == mixed_quality_tier_error_ref1.error, "Mixed quality tier stripped error should be equal to mixed quality tier whole");
		}
	}

	// Strip lowest tier
	if (db.has_bulk_data(quality_tier::lowest_importance))
	{
		const error_result result = strip_database_quality_tier(allocator, db, quality_tier::lowest_importance, db_no_low);
		ACL_ASSERT(result.empty(), result.c_str());

		decompression_context<debug_transform_decompression_settings_with_db> context0;
		decompression_context<debug_transform_decompression_settings_with_db> context1;
		database_context<acl::debug_database_settings> db_context;
		debug_database_streamer db_medium_streamer(allocator, db_bulk_data_medium, db_no_low->get_bulk_data_size(quality_tier::medium_importance));
		debug_database_streamer db_low_streamer(allocator, db_bulk_data_low, db_no_low->get_bulk_data_size(quality_tier::lowest_importance));

		bool initialized = db_context.initialize(allocator, *db_no_low, db_medium_streamer, db_low_streamer);
		initialized = initialized && context0.initialize(tracks0, db_context);
		initialized = initialized && context1.initialize(tracks1, db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		// Nothing is streamed in yet, we have low quality
		{
			const track_error low_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Low quality tier stripped error should be equal to low quality tier whole");
			const track_error low_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Low quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream in our medium importance tier
		stream_in_database_tier(db_context, db_medium_streamer, *db_no_low, quality_tier::medium_importance);

		{
			const track_error medium_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error0.error == medium_quality_tier_error_ref0.error, "Medium quality tier stripped error should be equal to medium quality tier whole");
			const track_error medium_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error1.error == medium_quality_tier_error_ref1.error, "Medium quality tier stripped error should be equal to medium quality tier whole");
		}

		// Stream in our low importance tier, restoring the full high quality
		stream_in_database_tier(db_context, db_low_streamer, *db_no_low, quality_tier::lowest_importance);

		{
			const track_error high_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error0.error == medium_quality_tier_error_ref0.error, "High quality tier stripped error should be equal to medium quality tier whole");
			const track_error high_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error1.error == medium_quality_tier_error_ref1.error, "High quality tier stripped error should be equal to medium quality tier whole");
		}

		// Stream out our medium importance tier, we'll have mixed quality
		stream_out_database_tier(db_context, db_medium_streamer, *db_no_low, quality_tier::medium_importance);

		{
			const track_error mixed_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Mixed quality tier stripped error should be equal to low quality tier whole");
			const track_error mixed_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Mixed quality tier stripped error should be equal to low quality tier whole");
		}
	}

	// Strip medium and lowest tiers
	if (db.has_bulk_data(quality_tier::medium_importance) && db.has_bulk_data(quality_tier::lowest_importance))
	{
		ACL_ASSERT(db_no_medium != nullptr, "Expected a valid database");
		ACL_ASSERT(db_no_low != nullptr, "Expected a valid database");

		error_result result = strip_database_quality_tier(allocator, *db_no_medium, quality_tier::lowest_importance, db_neither0);
		ACL_ASSERT(result.empty(), result.c_str());
		result = strip_database_quality_tier(allocator, *db_no_low, quality_tier::medium_importance, db_neither1);
		ACL_ASSERT(result.empty(), result.c_str());
		ACL_ASSERT(db_neither0->get_hash() == db_neither1->get_hash(), "Stripping order should not matter");

		decompression_context<debug_transform_decompression_settings_with_db> context0;
		decompression_context<debug_transform_decompression_settings_with_db> context1;
		database_context<acl::debug_database_settings> db_context;
		debug_database_streamer db_medium_streamer(allocator, db_bulk_data_medium, db_neither0->get_bulk_data_size(quality_tier::medium_importance));
		debug_database_streamer db_low_streamer(allocator, db_bulk_data_low, db_neither0->get_bulk_data_size(quality_tier::lowest_importance));

		bool initialized = db_context.initialize(allocator, *db_neither0, db_medium_streamer, db_low_streamer);
		initialized = initialized && context0.initialize(tracks0, db_context);
		initialized = initialized && context1.initialize(tracks1, db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		// Nothing is streamed in yet, we have low quality
		{
			const track_error low_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Low quality tier stripped error should be equal to low quality tier whole");
			const track_error low_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(low_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Low quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream in our medium importance tier
		stream_in_database_tier(db_context, db_medium_streamer, *db_neither0, quality_tier::medium_importance);

		{
			const track_error medium_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Medium quality tier stripped error should be equal to low quality tier whole");
			const track_error medium_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(medium_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Medium quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream in our low importance tier, restoring the full high quality
		stream_in_database_tier(db_context, db_low_streamer, *db_neither0, quality_tier::lowest_importance);

		{
			const track_error high_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error0.error == low_quality_tier_error_ref0.error, "High quality tier stripped error should be equal to low quality tier whole");
			const track_error high_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(high_quality_tier_error1.error == low_quality_tier_error_ref1.error, "High quality tier stripped error should be equal to low quality tier whole");
		}

		// Stream out our medium importance tier, we'll have mixed quality
		stream_out_database_tier(db_context, db_medium_streamer, *db_neither0, quality_tier::medium_importance);

		{
			const track_error mixed_quality_tier_error0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error0.error == low_quality_tier_error_ref0.error, "Mixed quality tier stripped error should be equal to low quality tier whole");
			const track_error mixed_quality_tier_error1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
			ACL_ASSERT(mixed_quality_tier_error1.error == low_quality_tier_error_ref1.error, "Mixed quality tier stripped error should be equal to low quality tier whole");
		}
	}

	if (db_no_medium != nullptr)
		allocator.deallocate(db_no_medium, db_no_medium->get_size());
	if (db_no_low != nullptr)
		allocator.deallocate(db_no_low, db_no_low->get_size());
	if (db_neither0 != nullptr)
		allocator.deallocate(db_neither0, db_neither0->get_size());
	if (db_neither1 != nullptr)
		allocator.deallocate(db_neither1, db_neither1->get_size());
}

static bool has_zero_scale(const track_array_qvvf& raw_tracks)
{
	const float threshold = 1.0E-6F;
	const rtm::vector4f zero = rtm::vector_zero();
	const uint32_t num_samples_per_track = raw_tracks.get_num_samples_per_track();

	for (const track_qvvf& raw_track : raw_tracks)
	{
		for (uint32_t sample_index = 0; sample_index < num_samples_per_track; ++sample_index)
		{
			const rtm::qvvf& sample = raw_track[sample_index];
			if (rtm::vector_any_near_equal3(sample.scale, zero, threshold))
				return true;
		}
	}

	return false;
}

void validate_db(iallocator& allocator, const track_array_qvvf& raw_tracks, const track_array_qvvf& additive_base_tracks,
	const compression_database_settings& settings, const itransform_error_metric& error_metric,
	const compressed_tracks& compressed_tracks0, const compressed_tracks& compressed_tracks1)
{
	using namespace acl_impl;

	// We do not support building empty databases
	if (compressed_tracks0.get_num_tracks() == 0 || compressed_tracks0.get_num_samples_per_track() == 0)
		return;

	// When we have a database, we are forced to linearly interpolate our keys which means
	// we can end up with slightly different values than we otherwise would.
	// When this happens, zero scale ends up being non-zero and the results can end up vastly different.
	// Don't validate our database if we have zero scale.
	if (has_zero_scale(raw_tracks))
		return;

	// Disable floating point exceptions since decompression assumes it
	scope_disable_fp_exceptions fp_off;

	// Our desired error threshold
	// When intrinsics aren't used with x86, the floating point arithmetic falls back to
	// using x87 instructions. When this happens, depending on how code is generated some
	// small inaccuracies can pop up because rounding happens when we store to memory.
	// The full pose decompression stores samples into the stack while working with them
	// while the single track decompression does not which causes the issue.
	// With SSE2 and NEON, there are no such rounding issues.
#if !defined(RTM_SSE2_INTRINSICS) && defined(RTM_ARCH_X86)
	const float threshold = 1.0E-3F;
#else
	const float threshold = 1.0E-4F;
#endif

	// Build our databases
	const compressed_tracks* input_tracks[2] = { &compressed_tracks0, &compressed_tracks1 };

	compressed_tracks* db_tracks0[1] = { nullptr };
	compressed_tracks* db_tracks1[1] = { nullptr };
	compressed_tracks* db_tracks01[2] = { nullptr, nullptr };
	compressed_database* db0 = nullptr;
	compressed_database* db1 = nullptr;
	compressed_database* db01 = nullptr;

	{
		error_result db_result = build_database(allocator, settings, &input_tracks[0], 1, db_tracks0, db0);
		ACL_ASSERT(db_result.empty(), db_result.c_str());

		db_result = build_database(allocator, settings, &input_tracks[1], 1, db_tracks1, db1);
		ACL_ASSERT(db_result.empty(), db_result.c_str());

		db_result = build_database(allocator, settings, &input_tracks[0], 2, db_tracks01, db01);
		ACL_ASSERT(db_result.empty(), db_result.c_str());

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C6011: Dereferencing NULL pointer '...'.
	// Crashing is fine since this is used for regression testing
	#pragma warning(disable : 6011)
#endif

		ACL_ASSERT(db0->contains(*db_tracks0[0]), "Database should contain our clip");
		ACL_ASSERT(db1->contains(*db_tracks1[0]), "Database should contain our clip");
		ACL_ASSERT(db01->contains(*db_tracks01[0]), "Database should contain our clip");
		ACL_ASSERT(db01->contains(*db_tracks01[1]), "Database should contain our clip");

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif
	}

	// Reference error without the database with everything highest quality
	track_error high_quality_tier_error_ref;
	{
		acl::decompression_context<debug_transform_decompression_settings_with_db> context;

		const bool initialized = context.initialize(compressed_tracks0);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		high_quality_tier_error_ref = calculate_compression_error(allocator, raw_tracks, context, error_metric, additive_base_tracks);
	}

	// Make sure the databases agree with our reference
	{
		acl::decompression_context<debug_transform_decompression_settings_with_db> context;
		acl::database_context<acl::debug_database_settings> db_context;

		bool initialized = db_context.initialize(allocator, *db0);
		initialized = initialized && context.initialize(*db_tracks0[0], db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		const track_error error_tier0 = calculate_compression_error(allocator, raw_tracks, context, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(error_tier0.error, high_quality_tier_error_ref.error, threshold), "Database 0 should have the same error");
	}

	{
		acl::decompression_context<debug_transform_decompression_settings_with_db> context;
		acl::database_context<acl::debug_database_settings> db_context;

		bool initialized = db_context.initialize(allocator, *db1);
		initialized = initialized && context.initialize(*db_tracks1[0], db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		const track_error error_tier1 = calculate_compression_error(allocator, raw_tracks, context, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(error_tier1.error, high_quality_tier_error_ref.error, threshold), "Database 1 should have the same error");
	}

	{
		acl::decompression_context<debug_transform_decompression_settings_with_db> context0;
		acl::decompression_context<debug_transform_decompression_settings_with_db> context1;
		acl::database_context<acl::debug_database_settings> db_context;

		bool initialized = db_context.initialize(allocator, *db01);
		initialized = initialized && context0.initialize(*db_tracks01[0], db_context);
		initialized = initialized && context1.initialize(*db_tracks01[1], db_context);
		ACL_ASSERT(initialized, "Failed to initialize decompression context");

		const track_error error_tier0 = calculate_compression_error(allocator, raw_tracks, context0, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(error_tier0.error, high_quality_tier_error_ref.error, threshold), "Database 01 should have the same error");

		const track_error error_tier1 = calculate_compression_error(allocator, raw_tracks, context1, error_metric, additive_base_tracks);
		ACL_ASSERT(rtm::scalar_near_equal(error_tier1.error, high_quality_tier_error_ref.error, threshold), "Database 01 should have the same error");
	}

	// Split the database bulk data out
	compressed_database* split_db = nullptr;
	uint8_t* split_db_bulk_data_medium = nullptr;
	uint8_t* split_db_bulk_data_low = nullptr;
	const error_result split_result = split_database_bulk_data(allocator, *db01, split_db, split_db_bulk_data_medium, split_db_bulk_data_low);
	ACL_ASSERT(split_result.empty(), "Failed to split database");
	ACL_ASSERT(split_db->is_valid(true).empty(), "Failed to split database");

	ACL_ASSERT(split_db->contains(*db_tracks01[0]), "Database should contain our clip");
	ACL_ASSERT(split_db->contains(*db_tracks01[1]), "Database should contain our clip");

	// Measure the tier error through simulated streaming
	validate_db_streaming(allocator, raw_tracks, additive_base_tracks, error_metric, high_quality_tier_error_ref, *db_tracks01[0], *db_tracks01[1], *split_db, split_db_bulk_data_medium, split_db_bulk_data_low);

	// Measure the tier error when stripping
	validate_db_stripping(allocator, raw_tracks, additive_base_tracks, error_metric, *db_tracks01[0], *db_tracks01[1], *split_db, split_db_bulk_data_medium, split_db_bulk_data_low);

	// Duplicate our clips so we can modify them
	compressed_tracks* compressed_tracks_copy0 = safe_ptr_cast<compressed_tracks>(allocate_type_array_aligned<uint8_t>(allocator, db_tracks0[0]->get_size(), alignof(compressed_tracks)));
	compressed_tracks* compressed_tracks_copy1 = safe_ptr_cast<compressed_tracks>(allocate_type_array_aligned<uint8_t>(allocator, db_tracks1[0]->get_size(), alignof(compressed_tracks)));

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C6011: Dereferencing NULL pointer '...'.
	// Crashing is fine since this is used for regression testing
	#pragma warning(disable : 6011)
	// warning C6387: '...' could be '0':  this does not adhere to the specification for the function 'memcpy'. See line 523 for an earlier location where this can occur
	// Pointers will never be null and if they are, we'll crash which is fine in a regression test
	#pragma warning(disable : 6387)
#endif

	std::memcpy(reinterpret_cast<uint8_t*>(compressed_tracks_copy0), db_tracks0[0], db_tracks0[0]->get_size());
	std::memcpy(reinterpret_cast<uint8_t*>(compressed_tracks_copy1), db_tracks1[0], db_tracks1[0]->get_size());

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

	// Free our memory
	allocator.deallocate(split_db_bulk_data_medium, split_db->get_bulk_data_size(quality_tier::medium_importance));
	allocator.deallocate(split_db_bulk_data_low, split_db->get_bulk_data_size(quality_tier::lowest_importance));
	allocator.deallocate(split_db, split_db->get_size());
	allocator.deallocate(compressed_tracks_copy0, compressed_tracks_copy0->get_size());
	allocator.deallocate(compressed_tracks_copy1, compressed_tracks_copy1->get_size());
	allocator.deallocate(db_tracks0[0], db_tracks0[0]->get_size());
	allocator.deallocate(db_tracks1[0], db_tracks1[0]->get_size());
	allocator.deallocate(db_tracks01[0], db_tracks01[0]->get_size());
	allocator.deallocate(db_tracks01[1], db_tracks01[1]->get_size());
	allocator.deallocate(db0, db0->get_size());
	allocator.deallocate(db1, db1->get_size());
	allocator.deallocate(db01, db01->get_size());
}
#endif
