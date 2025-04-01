#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2020 Nicholas Frechette & Animation Compression Library contributors
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

#include "acl/config.h"
#include "acl/version.h"
#include "acl/core/bit_manip_utils.h"
#include "acl/core/bitset.h"
#include "acl/core/compressed_tracks.h"
#include "acl/core/compressed_tracks_version.h"
#include "acl/core/interpolation_utils.h"
#include "acl/core/range_reduction_types.h"
#include "acl/core/track_formats.h"
#include "acl/core/track_writer.h"
#include "acl/core/impl/atomic.impl.h"
#include "acl/core/impl/bit_cast.impl.h"
#include "acl/core/impl/compiler_utils.h"
#include "acl/core/impl/variable_bit_rates.h"
#include "acl/decompression/database/database.h"
#include "acl/decompression/impl/animated_track_cache.transform.h"
#include "acl/decompression/impl/constant_track_cache.transform.h"
#include "acl/decompression/impl/decompression_context.transform.h"
#include "acl/math/quatf.h"
#include "acl/math/quat_packing.h"
#include "acl/math/vector4f.h"

#include <rtm/quatf.h>
#include <rtm/scalarf.h>
#include <rtm/vector4f.h>

#include <cstdint>
#include <type_traits>

// Use bit scanning to iterate over default/constant/animated sub-tracks instead
// of unrolling and branching
// This beats the current impl and the unrolled version on ARM64 M1
//#define ACL_IMPL_USE_BIT_SCAN_ITERATION_DEFAULT

// Not measured
//#define ACL_IMPL_USE_BIT_SCAN_ITERATION_CONSTANT

// Not measured
//#define ACL_IMPL_USE_BIT_SCAN_ITERATION_ANIMATED

// Use unrolling to iterate over default sub-tracks instead of bit scanning or
// the old method
// This appears to be slighty faster than the current impl on ARM64 M1
//#define ACL_IMPL_USE_UNROLLED_ITERATION_DEFAULT




// Why are all the changes almost the same within noise margin?
// It could be that all of this executes mostly for free hidden
// behind a cache miss (as intended). As a result of this, the
// instructions here which are independent continue to be scheduled
// and executed. Eventually, we reach the end of the ROB and we stall
// waiting for memory. As long as execution finishes before this,
// the cost will boil down to one thing: how many instructions
// need to retire. This needs to happen in-order. The ROB size
// on M1 is 636 instructions and the scheduler has 326 entries.
// Since our benchmark clips mostly have 44 bones, this means
// we only have 96 bits (88 rounded up) to process.
// We have about 32 instructions for the prologue and 32 for
// the epilogue. Our core loop is about 60 instructions.
// We need 3 iterations to cover our bit set and so the total
// cost should be around 32+60*3+32=244 instructions. This is
// well within the ROB/scheduler windows. After this, instructions
// must retire. For example, the Zen2 can retire 8 instructions
// per cycle. This means we are much less sensitive to how many
// instructions per sub-track we need to execute. This would also
// explain why adding branches seems to help a bit: we can avoid
// retiring some instructions. The fact that they don't execute
// is less relevant here and even if we fail to predict, it
// won't matter as much for such small pose buffers.
// Unrolling ends up with a cost per sub-track of about 4 instructions
// and 6 instructions per set bit.
// Bit scanning has a cost of 9 instructions per set bit so it
// is a bit cheaper overall. The benefits increase the fewer set
// bits there are.

// Memory latency is around 100ns for a cache miss to DRAM.
// At 1.6 Ghz, that comes down to 160 cycles.
// If we assume that the frontend can at least triple dispatch
// which is common on weaker mobile processors, then that
// means we can cover around 320-480 instructions in that time.
// On modern processors, frontends can typically dispatch up
// to 4-5 instructions easily per cycle, bringing us to
// 640-800 instructions. A processor twice as fast at 3.2 Ghz
// would then execute twice as many in that time window.
// Conservatively, if we want to remain memory bound as much
// as possible, we should aim to execute 450 instructions
// in between memory loads on weak hardware and 900 or so on
// newer hardware. Memory latency might also be lower than
// this, meaning we have fewer cycles to get the job done.

// There are two limits we might hit while prefetching:
// the ROB window and the scheduler window sizes.
// The scheduler is easiest to deal with: we just need to
// find enough independent work from L1 to perform while
// we prefetch (aka work with what we have).
// The ROB window is thus the bigger issue.
// A Cortex-A57 (ARM64) has a ROB of 40 (up to x8) entries.
// A Cortex-A72 (ARM64) has a ROB of 128 entries.
// A Jaguar (x64) has a ROB of 64 entries.
// A Zen2 (x64) has a ROB of 224 entries.
// A Zen5 (x64) has a ROB of 448 entries.
// As such, we can easily see that we will hit this limit
// long before the memory fetch or prefetch is completed.
// It would thus be most helpful if we could split the work
// into bundles of ROB * 1.25 entries per platform.

// Another limitation we can run into when prefetching is
// the amount of line fill buffers. We have buffers that service
// L1 misses that get used when we load/prefetch and we miss
// in L1. We have separate buffers in L2 if we miss there.
// Typically, we have 5-8 line fill buffers when we miss in
// L1 and maybe 8-12 if we miss in L2. The line fill buffers
// are also shared between hyperthreads and similarly when
// the L2 is shared, the buffers there are shared as well.
// As such, ideally, we want to prefetch into L2 only about
// 6 cache lines to leave room for the neighbor thread and
// only 3-4 cache lines into L1.

// We will assume that the output pose lives in the CPU L1
// and thus writes are very cheap/free.

// When we prefetch, we have to be careful to spread out
// the things that can TLB miss. It is common to have a single
// unit for the code and another for the data to resolve
// virtual memory addresses. As such, latency can be high.
// As such, early bundles should include one prefetch to
// something that might TLB miss, but no more.

// Unpacking default sub-tracks requires no prefetching as
// such they are ideal to prime the remainder of the decompression
// process. However, default sub-tracks can commonly be
// stripped entirely when the bind pose is stripped and so
// we might not be able to leverage them to prefetch ahead.
// Unpacking constant sub-tracks requires some prefetching,
// but less than is required for unpacking animated sub-tracks
// which need multiple different parallel streams.
// Unpacking animated sub-tracks requires a lot of prefetching.
// And so by the time we get to these, bundles can focus on
// just this.

// Let us assume we have no default sub-tracks, a common case.
// We'll assume 12 instructions per set bit, using bit scanning.
// This is just the iteration. Added on top is the unpacking for
// rotations and the load/stores for translations/scales.
// Unpacking rotations requires ~18 instructions while translations/scales
// require ~4 instructions.
// This means that we can process 4 constant rotations in 66 cycles
// or so. That seems like a good bundle size.
// We could unpack 4 constant rotations (48 bytes) and write them out
// in a single bundle while we prefetch ahead.
// We can execute 4+ bundles of this in the shadow of a cache miss.
// This means that if we prefetch 5-6 bundles ahead, by the time we
// reach the prefetched one, the cache line will be almost ready
// and we'll remain memory bound even on lower hardware (newer
// hardware could execute many more instructions). This means that
// we need to be able to execute 5-6 bundles without stalling
// which would require 4-5 cache lines to be prefetched.
// And so, to remain memory bound while unpacking constant rotations
// we must prefetch 4 cache lines in one bundle, and none in the next
// 5. This is out entire prefetching budget just for this.
// Unpacking constant translations/scales has the same bandwidth
// requirement (48 bytes for 4 sub-tracks).
// And so, we want to prefetch into L2 5x different cache lines:
// 4x for our future bundles of the same type, and 1x for misc
// things.
// We have a finite set of misc things to prefetch. Misc things
// are used to prime the next bundle stage. Let us assume that
// we need to prefetch 3 cache lines of each state.
// We end up with:
//    - We need 3x for constant translations
//    - We need 3x for constant scales (optional)
//    - We need 3-6x for segment metadata (rotations) (from 1-2 segments)
//    - We need 3-6x for segment metadata (translations)
//    - We need 3-6x for segment metadata (scales)
//    - We need 3-6x for segment range (rotations) (from 1-2 segments)
//    - We need 3-6x for segment range (translations)
//    - We need 3-6x for segment range (scales)
//    - We need 6x for animated data (rotations) (from two keyframes)
//    - We need 6x for animated data (translations)
//    - We need 6x for animated data (scales)
//    - We need 6x for clip range (rotations) (clip range is 4x12x2=96 bytes)
//    - We need 6x for clip range (translations)
//    - We need 6x for clip range (scales)
// Thus, our worst case is a total of 78 cache lines that need to be prefetched ahead.
// If we assume that constant sub-tracks are broken into bundles of 4,
// it means we need 3 bundles to prefetch our translations (12 constant rotations).
// This is a sensible number.
// If we have 3 more bundles of rotations or translations, we can prefetch our
// constant scales ahead of time.
// We need 3-6 more for the segment metadata and 3-6 more for the segment range data.
// That means we need 15 bundles to prefetch everything up to the first part of
// our animated unpacking.
// By the time we reach the animated unpacking, these bundles have a lot more
// instructions. Processing 1 cache line worth of animated data requires 0.375
// cache line of segment range data (24 bytes for 4 values), segment metadata
// requires 0.625 cache lines (4 bytes for 4 values), and 1.5 cache lines of clip
// range data (96 bytes for 4 values). However, some of these steps can be split
// into multiple bundles. And so, by the time we hit animated data, we can
// sustain the rythm.
// Unpacking segment range data can be one bundle per segment (or one for both).
// Unpacking animated data can be one bundle per keyframe.
// Remaping ranges and reconstructing quat.w can be another per keyframe.
// Interpolation and writing can be another bundle.
// This means that for animated data, each group of 4 values needs 6 bundles.
// That is ample space to prefetch the next bundles ahead.
//
// The difficulty of achieving this bundle chaining is in the stiching
// required near the end of one phase to prefetch the next. The idea would
// be for buffers to be contiguous. By this I don't mean between rotations/translations.
// Constant translations would follow constant rotations in memory.
// Similarly for the other buffers.
// This way, each bundle can prefetch ahead of its thing.
// This would leave only the case where we go from constant to animated.
// For that jump, we need to write ahead a list of pointers to prefetch
// that we compute ahead of time. Bundles that execute for constant
// sub-tracks would prefetch one entry. If not enough constant sub-tracks
// are present in a clip, we'll simply cache miss when we start unpacking
// animated values. This is unavoidable if we don't have enough work to
// perform to hide the memory latency.
// Once we start unpacking animated data, we no longer need this
// prefetch ptr buffer.
//
// This leaves just the case when we have default sub-tracks to unpack
// as well. These are fairly cheap as they do not need any prefetched
// memory. As such, each bundle of default sub-tracks can help prefetch ahead
// just like constant sub-tracks. They can leverage the prefetch ptr buffer
// in the same way.
//
// The next question becomes: how do we build and stich bundles together.
// Ideally, we want bundles to be dynamic, this means we need an indirect
// branch from bundle to bundle. To that end, we'd like to leverage
// tail call forwarding.
// For default/constant sub-tracks, we don't really need to interleave
// bundles of different types. We can have a single bundle that loops
// a certain number of times before moving on to the next bundle.
// This would also allow us to specialize the last bundle in a group to
// handle the trailing bit set bits.
// And so, we need the following bundles:
//    - Default rotations/translations/scales (+1 for trail)
//    - Constant rotations/translations/scales (+1 for trail)
//    - Animated rotations/translations/scales (+1 for trail)
// The animated bundles can be further broken down into sub-bundles:
//    - Unpack segment range data (+1 for second optional segment) (prefetch segment range data)
//    - Unpack keyframe 0 (prefetch animated data)
//    - Unpack keyframe 1 (prefetch animated data)
//    - Unpack remap and reconstruct keyframe 0 (prefetch clip range data)
//    - Unpack remap and reconstruct keyframe 1 (prefetch clip range data)
//    - Lerp keyframes and write (prefetch segment metadata x2)
// We don't really need bundles to be dynamic. We could hardcode them since
// we know ahead of time what comes next. It just means that if a bundle
// if empty, we waste a bit of time to skip it.
//
// Because we have a fixed number of typed bundles, we don't care if
// tail call optimization is not performed (e.g. debug builds). We have
// at most 9 bundles executing which doesn't make for a very deep callstack
// even if they are recursive.
//
// In the event where a processor is faster than memory, prefetch
// instructions will stall. This is fine, it means we are memory bound.
// If the opposite happens, memory is faster than the CPU, then
// none of the bundles will stall and we can operate as fast as the CPU
// allows.
// Ideally, we want to be memory bound as much as possible as decompression
// is a glorified specialized memcpy operation. Memory is slow and so
// we want to keep bandwidth saturated as much as possible, hence the heavy
// emphasis on prefetching. If we keep bundles small enough, we should be
// able to easily finish executing them before memory arrives for the next
// bundle, ensuring we remain memory bound.
//
// Proposed flow:
//    - Initialize
//    - While not done
//        - Prefetch 4x misc cache lines
//        - Unpack 4-8 default rotations
//    - While not done
//        - Prefetch 4x misc cache lines
//        - Unpack 4-8 default translations
//    - While not done
//        - Prefetch 4x misc cache lines
//        - Unpack 4-8 default scales
//    - While not done
//        - Prefetch 4x constant rotations cache lines
//        - Prefetch 1x misc cache line
//        - Unpack 4 constant rotations
//    - While not done
//        - Prefetch 4x constant translations cache lines
//        - Prefetch 1x misc cache line
//        - Unpack 4 constant translations
//    - While not done
//        - Prefetch 4x constant scales cache lines
//        - Prefetch 1x misc cache line
//        - Unpack 4 constant scales
//    - While not done
//        - Bundle 0
//            - Prefetch 1-2x segment range data (rotations)
//            - Unpack segment range data (rotations)
//        - Bundle 1
//            - Prefetch 1x animated data (rotations)
//            - Unpack animated keyframe 0
//        - Bundle 2
//            - Prefetch 1x animated data (rotations)
//            - Unpack animated keyframe 1
//        - Bundle 3
//            - Prefetch 1x clip range data (rotations)
//            - Remap and reconstruct keyframe 0
//        - Bundle 4
//            - Prefetch 1x clip range data (rotations)
//            - Remap and reconstruct keyframe 1
//        - Bundle 5
//            - Prefetch segment metadata (rotations)
//            - Lerp and write
//    - While not done
//        - Bundle 0
//            - Prefetch 1-2x segment range data (translations)
//            - Unpack segment range data (translations)
//        - Bundle 1
//            - Prefetch 1x animated data (translations)
//            - Unpack animated keyframe 0
//        - Bundle 2
//            - Prefetch 1x animated data (translations)
//            - Unpack animated keyframe 1
//        - Bundle 3
//            - Prefetch 1x clip range data (translations)
//            - Remap and reconstruct keyframe 0
//        - Bundle 4
//            - Prefetch 1x clip range data (translations)
//            - Remap and reconstruct keyframe 1
//        - Bundle 5
//            - Prefetch segment metadata (translations)
//            - Lerp and write
//    - While not done
//        - Bundle 0
//            - Prefetch 1-2x segment range data (scales)
//            - Unpack segment range data (scales)
//        - Bundle 1
//            - Prefetch 1x animated data (scales)
//            - Unpack animated keyframe 0
//        - Bundle 2
//            - Prefetch 1x animated data (scales)
//            - Unpack animated keyframe 1
//        - Bundle 3
//            - Prefetch 1x clip range data (scales)
//            - Remap and reconstruct keyframe 0
//        - Bundle 4
//            - Prefetch 1x clip range data (scales)
//            - Remap and reconstruct keyframe 1
//        - Bundle 5
//            - Prefetch segment metadata (scales)
//            - Lerp and write





// Unpacking default sub-tracks is very cheap. The bit sets
// we iterate are compact and will be in L1 by the time we need them.
// Most animation clips will only need 1-3 cache lines for the
// bit sets. This means that we can iterate over these almost
// for free while we prefetch something useful.

// Instead, why don't we try interleaving all 3 default sub-track
// loops? Their work will be independent all the same. This way,
// we can use bit scanning to unpack 3 indices at the same time.
// We just need to handle the left-overs as the number of set bits
// is unlikely to be the same.

// We can build on the same idea for constant sub-tracks.
// We could interleave all 3. This way, we can prefetch memory
// for each in parallel which should leave ample room for
// everything else. This would also give us plenty of extra
// work to perform.




// Current decompressio flow:
//   - Initialize
//   - Prefetch constant rotations/translations and segment metadata (6, potential TLB miss)
//   - Unpack default rotations
//   - Unpack constant rotations
//   - Prefetch segment range and animated data (6)
//   - Unpack default translations
//   - Unpack constant translations
//   - With scale
//      - Unpack default scales
//      - Unpack constant scales
//   - Without scale
//      - Unpack default scales
//   - Prefetch animated data and clip range data (6)
//   - Unpack animated rotations
//   - Unpack animated translations
//   - With scale
//      - Unpack animated scales

#define ACL_IMPL_USE_SEEK_PREFETCH

ACL_IMPL_FILE_PRAGMA_PUSH

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(push)
	// warning C4127: conditional expression is constant
	// This is fine, the optimizer will strip the code away when it can, but it isn't always constant in practice
	#pragma warning(disable : 4127)
#endif

namespace acl
{
	ACL_IMPL_VERSION_NAMESPACE_BEGIN

	namespace acl_impl
	{
#if defined(ACL_IMPL_USE_SEEK_PREFETCH)
#define ACL_IMPL_SEEK_PREFETCH(ptr) memory_prefetch(ptr)
#else
#define ACL_IMPL_SEEK_PREFETCH(ptr) (void)(ptr)
#endif

		template<class decompression_settings_type>
		constexpr bool is_database_supported_impl()
		{
			return decompression_settings_type::database_settings_type::version_supported() != compressed_tracks_version16::none;
		}

		template<class decompression_settings_type, class database_settings_type>
		inline bool initialize_v0(persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks, const database_context<database_settings_type>* database)
		{
			ACL_ASSERT(tracks.get_algorithm_type() == algorithm_type8::uniformly_sampled, "Invalid algorithm type [" ACL_ASSERT_STRING_FORMAT_SPECIFIER "], expected [" ACL_ASSERT_STRING_FORMAT_SPECIFIER "]", get_algorithm_name(tracks.get_algorithm_type()), get_algorithm_name(algorithm_type8::uniformly_sampled));

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const tracks_header& header = get_tracks_header(tracks);
			const transform_tracks_header& transform_header = get_transform_tracks_header(tracks);

			const rotation_format8 packed_rotation_format = header.get_rotation_format();
			const vector_format8 packed_translation_format = header.get_translation_format();
			const vector_format8 packed_scale_format = header.get_scale_format();
			const rotation_format8 rotation_format = get_rotation_format<decompression_settings_type>(packed_rotation_format);
			const vector_format8 translation_format = get_vector_format<translation_adapter>(packed_translation_format);
			const vector_format8 scale_format = get_vector_format<scale_adapter>(packed_scale_format);

			ACL_ASSERT(rotation_format == packed_rotation_format, "Statically compiled rotation format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ") differs from the compressed rotation format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ")!", get_rotation_format_name(rotation_format), get_rotation_format_name(packed_rotation_format));
			ACL_ASSERT(translation_format == packed_translation_format, "Statically compiled translation format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ") differs from the compressed translation format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ")!", get_vector_format_name(translation_format), get_vector_format_name(packed_translation_format));
			ACL_ASSERT(scale_format == packed_scale_format, "Statically compiled scale format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ") differs from the compressed scale format (" ACL_ASSERT_STRING_FORMAT_SPECIFIER ")!", get_vector_format_name(scale_format), get_vector_format_name(packed_scale_format));

			// Context is always the first member and versions should always match
			const database_context_v0* db = bit_cast<const database_context_v0*>(database);

			context.tracks = &tracks;
			context.db = db;
			context.tracks_hash = tracks.get_hash();
			context.db_hash = db != nullptr ? db->db_hash : 0;
			context.sample_time = -1.0F;
			context.rotation_format = rotation_format;
			context.translation_format = translation_format;
			context.scale_format = scale_format;
			context.has_scale = header.get_has_scale();
			context.has_segments = transform_header.has_multiple_segments();

			if (decompression_settings_type::is_wrapping_supported())
			{
				context.clip_duration = tracks.get_finite_duration();
				context.looping_policy = static_cast<uint8_t>(tracks.get_looping_policy());
			}
			else
			{
				context.clip_duration = tracks.get_finite_duration(sample_looping_policy::clamp);
				context.looping_policy = static_cast<uint8_t>(sample_looping_policy::clamp);
			}

			return true;
		}

		template<class decompression_settings_type, class database_settings_type>
		inline bool relocated_v0(persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks, const database_context<database_settings_type>* database)
		{
			if (context.tracks_hash != tracks.get_hash())
				return false;	// Hash is different, this instance did not relocate, it is different

			// Context is always the first member and versions should always match
			const database_context_v0* db = bit_cast<const database_context_v0*>(database);
			const uint32_t db_hash = db != nullptr ? db->db_hash : 0;

			if (context.db_hash != db_hash)
				return false;	// Hash is different, this instance did not relocate, it is different

			// The instances are identical and might have relocated, update our metadata
			context.tracks = &tracks;
			context.db = db;

			// Reset the sample time to force seek() to be called again.
			// The context otherwise contains pointers within the tracks and database instances
			// that are populated during seek.
			context.sample_time = -1.0F;

			return true;
		}

		template<class decompression_settings_type>
		inline bool is_bound_to_v0(const persistent_transform_decompression_context_v0& context, const compressed_tracks& tracks)
		{
			if (context.tracks != &tracks)
				return false;	// Different pointer, no guarantees

			if (context.tracks_hash != tracks.get_hash())
				return false;	// Different hash

			// Must be bound to it!
			return true;
		}

		template<class decompression_settings_type>
		inline bool is_bound_to_v0(const persistent_transform_decompression_context_v0& context, const compressed_database& database)
		{
			if (context.db == nullptr)
				return false;	// Not bound to any database

			if (context.db->db != &database)
				return false;	// Different pointer, no guarantees

			if (context.db_hash != database.get_hash())
				return false;	// Different hash

			// Must be bound to it!
			return true;
		}

		template<class decompression_settings_type>
		inline void set_looping_policy_v0(persistent_transform_decompression_context_v0& context, sample_looping_policy policy)
		{
			if (!decompression_settings_type::is_wrapping_supported())
				return;	// Only clamping is supported

			const compressed_tracks* tracks = context.tracks;

			if (policy == sample_looping_policy::as_compressed)
				policy = tracks->get_looping_policy();

			const sample_looping_policy current_policy = static_cast<sample_looping_policy>(context.looping_policy);
			if (current_policy != policy)
			{
				// Policy changed
				context.clip_duration = tracks->get_finite_duration(policy);
				context.looping_policy = static_cast<uint8_t>(policy);
			}
		}

		template<class decompression_settings_type>
		inline void seek_v0(persistent_transform_decompression_context_v0& context, float sample_time, sample_rounding_policy rounding_policy)
		{
			const compressed_tracks* tracks = context.tracks;
			const tracks_header& header = get_tracks_header(*tracks);
			if (header.num_tracks == 0)
				return;	// Empty track list

			// Clamp for safety, the caller should normally handle this but in practice, it often isn't the case
			if (decompression_settings_type::clamp_sample_time())
				sample_time = rtm::scalar_clamp(sample_time, 0.0F, context.clip_duration);

			const uint8_t requested_rounding_policy = static_cast<uint8_t>(rounding_policy);
			if (context.sample_time == sample_time && context.requested_rounding_policy == requested_rounding_policy)
				return;

			const transform_tracks_header& transform_header = get_transform_tracks_header(*tracks);

			// Prefetch our sub-track types, we'll need them soon when we start decompressing
			// Most clips will have their sub-track types fit into 1 or 2 cache lines, we'll prefetch 2
			// to be safe
			{
				const uint8_t* sub_track_types = bit_cast<const uint8_t*>(transform_header.get_sub_track_types());

				ACL_IMPL_SEEK_PREFETCH(sub_track_types);
				ACL_IMPL_SEEK_PREFETCH(sub_track_types + 64);
			}

			context.sample_time = sample_time;

			// If the wrap looping policy isn't supported, use our statically known value
			const sample_looping_policy looping_policy_ = decompression_settings_type::is_wrapping_supported() ? static_cast<sample_looping_policy>(context.looping_policy) : sample_looping_policy::clamp;

			uint32_t clip_key_frame0;
			uint32_t clip_key_frame1;
			find_linear_interpolation_samples_with_sample_rate(header.num_samples, header.sample_rate, sample_time, rounding_policy, looping_policy_, clip_key_frame0, clip_key_frame1, context.interpolation_alpha);

			// When per-track support is disabled in the decompression settings, the effective rounding policy
			// is ignored as we always interpolate using the alpha from the rounding policy
			context.requested_rounding_policy = requested_rounding_policy;
			context.effective_rounding_policy = requested_rounding_policy;

			uint32_t segment_key_frame0;
			uint32_t segment_key_frame1;

			const segment_header* segment_header0;
			const segment_header* segment_header1;

			const uint8_t* db_animated_track_data0 = nullptr;
			const uint8_t* db_animated_track_data1 = nullptr;

			// These two pointers are the same, the compiler should optimize one out, only here for type safety later
			const segment_header* segment_headers = transform_header.get_segment_headers();
			const stripped_segment_header_t* segment_tier0_headers = transform_header.get_stripped_segment_headers();

			const uint32_t num_segments = transform_header.num_segments;

			constexpr bool is_database_supported = is_database_supported_impl<decompression_settings_type>();
			ACL_ASSERT(is_database_supported || !tracks->has_database(), "Cannot have a database when it isn't supported");

			const bool has_database = is_database_supported && tracks->has_database();
			const database_context_v0* db = context.db;

			const bool has_stripped_keyframes = has_database || tracks->has_stripped_keyframes();

			if (num_segments == 1)
			{
				// Key frame 0 and 1 are in the only segment present
				// This is a really common case and when it happens, we don't store the segment start index (zero)

				if (has_stripped_keyframes)
				{
					const stripped_segment_header_t* segment_tier0_header0 = segment_tier0_headers;

					// This will cache miss
					uint32_t sample_indices0 = segment_tier0_header0->sample_indices;

					// Calculate our clip relative sample index, we'll remap it later relative to the samples we'll use
					const float sample_index = context.interpolation_alpha + float(clip_key_frame0);

					// When we load our sample indices and offsets from the database, there can be another thread writing
					// to those memory locations at the same time (e.g. streaming in/out).
					// To ensure thread safety, we atomically load the offset and sample indices.
					uint64_t medium_importance_tier_metadata0 = 0;
					uint64_t low_importance_tier_metadata0 = 0;

					// Combine all our loaded samples into a single bit set to find which samples we need to interpolate
					if (is_database_supported && db != nullptr)
					{
						// Possible cache miss for the clip header offset
						// Cache miss for the db clip segment headers pointer
						const tracks_database_header* tracks_db_header = transform_header.get_database_header();
						const database_runtime_clip_header* db_clip_header = tracks_db_header->get_clip_header(db->clip_segment_headers);
						const database_runtime_segment_header* db_segment_headers = db_clip_header->get_segment_headers();

						// Cache miss for the db segment headers
						const database_runtime_segment_header* db_segment_header0 = db_segment_headers;
						medium_importance_tier_metadata0 = db_segment_header0->tier_metadata[0].load(k_memory_order_relaxed);
						low_importance_tier_metadata0 = db_segment_header0->tier_metadata[1].load(k_memory_order_relaxed);

						sample_indices0 |= uint32_t(medium_importance_tier_metadata0);
						sample_indices0 |= uint32_t(low_importance_tier_metadata0);
					}

					// Find the closest loaded samples
					// Mask all trailing samples to find the first sample by counting trailing zeros
					const uint32_t candidate_indices0 = sample_indices0 & (0xFFFFFFFFU << (31 - clip_key_frame0));
					clip_key_frame0 = 31 - count_trailing_zeros(candidate_indices0);

					// Mask all leading samples to find the second sample by counting leading zeros
					const uint32_t candidate_indices1 = sample_indices0 & (0xFFFFFFFFU >> clip_key_frame1);
					clip_key_frame1 = count_leading_zeros(candidate_indices1);

					// Calculate our new interpolation alpha
					// We used the rounding policy above to snap to the correct key frame earlier but we might need to interpolate now
					// if key frames have been removed
					context.interpolation_alpha = find_linear_interpolation_alpha(sample_index, clip_key_frame0, clip_key_frame1, sample_rounding_policy::none, looping_policy_);

					// If we use floor/ceil/nearest then perhaps the keyframe we need has been stripped and isn't present
					// When this happens, we need to reconstruct it through interpolation which requires us to update
					// the effective rounding policy
					// If we use none, then we were already interpolating
					// If we use per-track rounding, then we assume that the caller knows what they are doing,
					// see sample_rounding_policy::per_track for details
					// We floor the interpolation alpha and compare it against itself to detect if it is different from 0.0 and 1.0
					if (rounding_policy != sample_rounding_policy::per_track && rtm::scalar_floor(context.interpolation_alpha) != context.interpolation_alpha)
						context.effective_rounding_policy = static_cast<uint8_t>(sample_rounding_policy::none);

					// Find where our data lives (clip or database tier X)
					sample_indices0 = segment_tier0_header0->sample_indices;
					uint32_t sample_indices1 = sample_indices0;	// Identical

					if (is_database_supported && db != nullptr)
					{
						const uint64_t sample_index0 = uint64_t(1) << (31 - clip_key_frame0);
						const uint64_t sample_index1 = uint64_t(1) << (31 - clip_key_frame1);

						const uint8_t* bulk_data_medium = db->bulk_data[0];		// Might be nullptr if we haven't streamed in yet
						const uint8_t* bulk_data_low = db->bulk_data[1];		// Might be nullptr if we haven't streamed in yet
						if ((medium_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}

						// Only one segment, our metadata is the same for our second key frame
						if ((medium_importance_tier_metadata0 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data1 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data1 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}
					}

					// Remap our sample indices within the ones actually stored (e.g. index 3 might be the second frame stored)
					segment_key_frame0 = count_set_bits(and_not(0xFFFFFFFFU >> clip_key_frame0, sample_indices0));
					segment_key_frame1 = count_set_bits(and_not(0xFFFFFFFFU >> clip_key_frame1, sample_indices1));

					// Nasty but safe since they have the same layout
					segment_header0 = static_cast<const segment_header*>(segment_tier0_header0);
					segment_header1 = static_cast<const segment_header*>(segment_tier0_header0);
				}
				else
				{
					segment_header0 = segment_headers;
					segment_header1 = segment_headers;

					segment_key_frame0 = clip_key_frame0;
					segment_key_frame1 = clip_key_frame1;
				}
			}
			else
			{
				const uint32_t* segment_start_indices = transform_header.get_segment_start_indices();

				// See segment_streams(..) for implementation details. This implementation is directly tied to it.
				const uint32_t approx_num_samples_per_segment = header.num_samples / num_segments;	// TODO: Store in header?
				const uint32_t approx_segment_index = clip_key_frame0 / approx_num_samples_per_segment;

				uint32_t segment_index0 = 0;
				uint32_t segment_index1 = 0;

				// Our approximate segment guess is just that, a guess. The actual segments we need could be just before or after.
				// We start looking one segment earlier and up to 2 after. If we have too few segments after, we will hit the
				// sentinel value of 0xFFFFFFFF and exit the loop.
				// TODO: Can we do this with SIMD? Load all 4 values, set key_frame0, compare, move mask, count leading zeroes
				const uint32_t start_segment_index = approx_segment_index > 0 ? (approx_segment_index - 1) : 0;
				const uint32_t end_segment_index = start_segment_index + 4;

				for (uint32_t segment_index = start_segment_index; segment_index < end_segment_index; ++segment_index)
				{
					if (clip_key_frame0 < segment_start_indices[segment_index])
					{
						// We went too far, use previous segment
						ACL_ASSERT(segment_index > 0, "Invalid segment index: %u", segment_index);
						segment_index0 = segment_index - 1;

						// If wrapping is enabled and we wrapped, use the first segment
						if (decompression_settings_type::is_wrapping_supported() && clip_key_frame1 == 0)
							segment_index1 = 0;
						else
							segment_index1 = clip_key_frame1 < segment_start_indices[segment_index] ? segment_index0 : segment_index;

						break;
					}
				}

				segment_key_frame0 = clip_key_frame0 - segment_start_indices[segment_index0];
				segment_key_frame1 = clip_key_frame1 - segment_start_indices[segment_index1];

				if (has_stripped_keyframes)
				{
					const stripped_segment_header_t* segment_tier0_header0 = segment_tier0_headers + segment_index0;
					const stripped_segment_header_t* segment_tier0_header1 = segment_tier0_headers + segment_index1;

					// This will cache miss
					uint32_t sample_indices0 = segment_tier0_header0->sample_indices;
					uint32_t sample_indices1 = segment_tier0_header1->sample_indices;

					// Calculate our clip relative sample index, we'll remap it later relative to the samples we'll use
					const float sample_index = context.interpolation_alpha + float(clip_key_frame0);

					// When we load our sample indices and offsets from the database, there can be another thread writing
					// to those memory locations at the same time (e.g. streaming in/out).
					// To ensure thread safety, we atomically load the offset and sample indices.
					uint64_t medium_importance_tier_metadata0 = 0;
					uint64_t medium_importance_tier_metadata1 = 0;
					uint64_t low_importance_tier_metadata0 = 0;
					uint64_t low_importance_tier_metadata1 = 0;

					// Combine all our loaded samples into a single bit set to find which samples we need to interpolate
					if (is_database_supported && db != nullptr)
					{
						// Possible cache miss for the clip header offset
						// Cache miss for the db clip segment headers pointer
						const tracks_database_header* tracks_db_header = transform_header.get_database_header();
						const database_runtime_clip_header* db_clip_header = tracks_db_header->get_clip_header(db->clip_segment_headers);
						const database_runtime_segment_header* db_segment_headers = db_clip_header->get_segment_headers();

						// Cache miss for the db segment headers
						const database_runtime_segment_header* db_segment_header0 = db_segment_headers + segment_index0;
						medium_importance_tier_metadata0 = db_segment_header0->tier_metadata[0].load(k_memory_order_relaxed);
						low_importance_tier_metadata0 = db_segment_header0->tier_metadata[1].load(k_memory_order_relaxed);

						sample_indices0 |= uint32_t(medium_importance_tier_metadata0);
						sample_indices0 |= uint32_t(low_importance_tier_metadata0);

						const database_runtime_segment_header* db_segment_header1 = db_segment_headers + segment_index1;
						medium_importance_tier_metadata1 = db_segment_header1->tier_metadata[0].load(k_memory_order_relaxed);
						low_importance_tier_metadata1 = db_segment_header1->tier_metadata[1].load(k_memory_order_relaxed);

						sample_indices1 |= uint32_t(medium_importance_tier_metadata1);
						sample_indices1 |= uint32_t(low_importance_tier_metadata1);
					}

					// Find the closest loaded samples
					// Mask all trailing samples to find the first sample by counting trailing zeros
					const uint32_t candidate_indices0 = sample_indices0 & (0xFFFFFFFFU << (31 - segment_key_frame0));
					segment_key_frame0 = 31 - count_trailing_zeros(candidate_indices0);

					// Mask all leading samples to find the second sample by counting leading zeros
					const uint32_t candidate_indices1 = sample_indices1 & (0xFFFFFFFFU >> segment_key_frame1);
					segment_key_frame1 = count_leading_zeros(candidate_indices1);

					// Calculate our clip relative sample indices
					clip_key_frame0 = segment_start_indices[segment_index0] + segment_key_frame0;
					clip_key_frame1 = segment_start_indices[segment_index1] + segment_key_frame1;

					// Calculate our new interpolation alpha
					// We used the rounding policy above to snap to the correct key frame earlier but we might need to interpolate now
					// if key frames have been removed
					context.interpolation_alpha = find_linear_interpolation_alpha(sample_index, clip_key_frame0, clip_key_frame1, sample_rounding_policy::none, looping_policy_);

					// If we use floor/ceil/nearest then perhaps the keyframe we need has been stripped and isn't present
					// When this happens, we need to reconstruct it through interpolation which requires us to update
					// the effective rounding policy
					// If we use none, then we were already interpolating
					// If we use per-track rounding, then we assume that the caller knows what they are doing,
					// see sample_rounding_policy::per_track for details
					// We floor the interpolation alpha and compare it against itself to detect if it is different from 0.0 and 1.0
					if (rounding_policy != sample_rounding_policy::per_track && rtm::scalar_floor(context.interpolation_alpha) != context.interpolation_alpha)
						context.effective_rounding_policy = static_cast<uint8_t>(sample_rounding_policy::none);

					// Find where our data lives (clip or database tier X)
					sample_indices0 = segment_tier0_header0->sample_indices;
					sample_indices1 = segment_tier0_header1->sample_indices;

					if (is_database_supported && db != nullptr)
					{
						const uint64_t sample_index0 = uint64_t(1) << (31 - segment_key_frame0);
						const uint64_t sample_index1 = uint64_t(1) << (31 - segment_key_frame1);

						const uint8_t* bulk_data_medium = db->bulk_data[0];		// Might be nullptr if we haven't streamed in yet
						const uint8_t* bulk_data_low = db->bulk_data[1];		// Might be nullptr if we haven't streamed in yet
						if ((medium_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(medium_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_medium + uint32_t(medium_importance_tier_metadata0 >> 32);
						}
						else if ((low_importance_tier_metadata0 & sample_index0) != 0)
						{
							sample_indices0 = uint32_t(low_importance_tier_metadata0);
							db_animated_track_data0 = bulk_data_low + uint32_t(low_importance_tier_metadata0 >> 32);
						}

						if ((medium_importance_tier_metadata1 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(medium_importance_tier_metadata1);
							db_animated_track_data1 = bulk_data_medium + uint32_t(medium_importance_tier_metadata1 >> 32);
						}
						else if ((low_importance_tier_metadata1 & sample_index1) != 0)
						{
							sample_indices1 = uint32_t(low_importance_tier_metadata1);
							db_animated_track_data1 = bulk_data_low + uint32_t(low_importance_tier_metadata1 >> 32);
						}
					}

					// Remap our sample indices within the ones actually stored (e.g. index 3 might be the second frame stored)
					segment_key_frame0 = count_set_bits(and_not(0xFFFFFFFFU >> segment_key_frame0, sample_indices0));
					segment_key_frame1 = count_set_bits(and_not(0xFFFFFFFFU >> segment_key_frame1, sample_indices1));

					// Nasty but safe since they have the same layout
					segment_header0 = static_cast<const segment_header*>(segment_tier0_header0);
					segment_header1 = static_cast<const segment_header*>(segment_tier0_header1);
				}
				else
				{
					segment_header0 = segment_headers + segment_index0;
					segment_header1 = segment_headers + segment_index1;
				}
			}

			{
				// Prefetch our constant rotation data, we'll need it soon when we start decompressing and we are about to cache miss on the segment headers
				const uint8_t* constant_data_rotations = transform_header.get_constant_track_data();
				ACL_IMPL_SEEK_PREFETCH(constant_data_rotations);
				ACL_IMPL_SEEK_PREFETCH(constant_data_rotations + 64);
			}

			const bool uses_single_segment = segment_header0 == segment_header1;
			context.uses_single_segment = uses_single_segment;

			// Cache miss if we don't access the db data
			transform_header.get_segment_data(*segment_header0, context.format_per_track_data[0], context.segment_range_data[0], context.animated_track_data[0]);

			// More often than not the two segments are identical, when this is the case, just copy our pointers
			if (!uses_single_segment)
			{
				transform_header.get_segment_data(*segment_header1, context.format_per_track_data[1], context.segment_range_data[1], context.animated_track_data[1]);
			}
			else
			{
				context.format_per_track_data[1] = context.format_per_track_data[0];
				context.segment_range_data[1] = context.segment_range_data[0];
				context.animated_track_data[1] = context.animated_track_data[0];
			}

			if (has_database)
			{
				// Update our pointers if the data lives within the database
				if (db_animated_track_data0 != nullptr)
					context.animated_track_data[0] = db_animated_track_data0;

				if (db_animated_track_data1 != nullptr)
					context.animated_track_data[1] = db_animated_track_data1;
			}

			context.key_frame_bit_offsets[0] = segment_key_frame0 * segment_header0->animated_pose_bit_size;
			context.key_frame_bit_offsets[1] = segment_key_frame1 * segment_header1->animated_pose_bit_size;

			context.segment_offsets[0] = ptr_offset32<segment_header>(tracks, segment_header0);
			context.segment_offsets[1] = ptr_offset32<segment_header>(tracks, segment_header1);
		}


		// TODO: Merge the per track format and segment range info into a single buffer? Less to prefetch and used together
		// TODO: Remove segment data alignment, no longer required?
		// TODO: sample_rounding_policy ends up being signed extended on x64 from a 32 bit value into 64 bit (edx -> rax)
		//       I tried using uint32_t and uint64_t as its underlying type but code generation remained the same
		//       Would using a raw uint32_t below instead of the typed enum help avoid the extra instruction?

		// Default rotations
#if defined(ACL_IMPL_USE_UNROLLED_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_rotation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default rotation if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();

			// Each entry can have at most 16 indices packed
			uint32_t unpacked_indices[16];
			uint32_t* unpacked_index_ptr = unpacked_indices;

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = rotation_sub_track_types->types;
				rotation_sub_track_types++;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = ~packed_entry - 0x55555555;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				const uint32_t base_track_index = entry_index * 16;

				if (packed_entry & 0xAA000000)
				{
					const uint32_t index_0 = (packed_entry >> 31) & 0x01;
					const uint32_t index_1 = (packed_entry >> 29) & 0x01;
					const uint32_t index_2 = (packed_entry >> 27) & 0x01;
					const uint32_t index_3 = (packed_entry >> 25) & 0x01;

					*unpacked_index_ptr = base_track_index | 0;
					unpacked_index_ptr += index_0;

					*unpacked_index_ptr = base_track_index | 1;
					unpacked_index_ptr += index_1;

					*unpacked_index_ptr = base_track_index | 2;
					unpacked_index_ptr += index_2;

					*unpacked_index_ptr = base_track_index | 3;
					unpacked_index_ptr += index_3;
				}

				if (packed_entry & 0x00AA0000)
				{
					const uint32_t index_4 = (packed_entry >> 23) & 0x01;
					const uint32_t index_5 = (packed_entry >> 21) & 0x01;
					const uint32_t index_6 = (packed_entry >> 19) & 0x01;
					const uint32_t index_7 = (packed_entry >> 17) & 0x01;

					*unpacked_index_ptr = base_track_index | 4;
					unpacked_index_ptr += index_4;

					*unpacked_index_ptr = base_track_index | 5;
					unpacked_index_ptr += index_5;

					*unpacked_index_ptr = base_track_index | 6;
					unpacked_index_ptr += index_6;

					*unpacked_index_ptr = base_track_index | 7;
					unpacked_index_ptr += index_7;
				}

				if (packed_entry & 0x0000AA00)
				{
					const uint32_t index_8 = (packed_entry >> 15) & 0x01;
					const uint32_t index_9 = (packed_entry >> 13) & 0x01;
					const uint32_t index_10 = (packed_entry >> 11) & 0x01;
					const uint32_t index_11 = (packed_entry >> 9) & 0x01;

					*unpacked_index_ptr = base_track_index | 8;
					unpacked_index_ptr += index_8;

					*unpacked_index_ptr = base_track_index | 9;
					unpacked_index_ptr += index_9;

					*unpacked_index_ptr = base_track_index | 10;
					unpacked_index_ptr += index_10;

					*unpacked_index_ptr = base_track_index | 11;
					unpacked_index_ptr += index_11;
				}

				if (packed_entry & 0x000000AA)
				{
					const uint32_t index_12 = (packed_entry >> 7) & 0x01;
					const uint32_t index_13 = (packed_entry >> 5) & 0x01;
					const uint32_t index_14 = (packed_entry >> 3) & 0x01;
					const uint32_t index_15 = (packed_entry >> 1) & 0x01;

					*unpacked_index_ptr = base_track_index | 12;
					unpacked_index_ptr += index_12;

					*unpacked_index_ptr = base_track_index | 13;
					unpacked_index_ptr += index_13;

					*unpacked_index_ptr = base_track_index | 14;
					unpacked_index_ptr += index_14;

					*unpacked_index_ptr = base_track_index | 15;
					unpacked_index_ptr += index_15;
				}

				const uint32_t* unpacked_index_read_ptr = unpacked_indices;
				while (unpacked_index_read_ptr < unpacked_index_ptr)
				//for (uint32_t unpacked_index = 0; unpacked_index < num_unpacked_indices; ++unpacked_index)
				{
					//const uint32_t unpacked_track_index = unpacked_indices[unpacked_index];
					const uint32_t unpacked_track_index = *unpacked_index_read_ptr;
					unpacked_index_read_ptr++;

					if (!writer.skip_track_rotation(unpacked_track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_rotation(unpacked_track_index, writer.get_variable_default_rotation(unpacked_track_index));
						else
							writer.write_rotation(unpacked_track_index, default_rotation);
					}
				}

				// Reset
				unpacked_index_ptr = unpacked_indices;
			}
		}
#elif defined(ACL_IMPL_USE_BIT_SCAN_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_rotation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default rotation if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = rotation_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				// We have 2 bits per sub-track
				const uint32_t curr_entry_track_index = entry_index * 16;

				while (packed_entry != 0)
				{
					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					if (!writer.skip_track_rotation(track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_rotation(track_index, writer.get_variable_default_rotation(track_index));
						else
							writer.write_rotation(track_index, default_rotation);
					}
				}
			}
		}
#else
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_rotation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default rotation if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = rotation_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no default sub-tracks, skip it

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index0))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index0, writer.get_variable_default_rotation(track_index0));
							else
								writer.write_rotation(track_index0, default_rotation);
						}
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index1))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index1, writer.get_variable_default_rotation(track_index1));
							else
								writer.write_rotation(track_index1, default_rotation);
						}
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index2))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index2, writer.get_variable_default_rotation(track_index2));
							else
								writer.write_rotation(track_index2, default_rotation);
						}
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index3))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_rotation(track_index3, writer.get_variable_default_rotation(track_index3));
							else
								writer.write_rotation(track_index3, default_rotation);
						}
					}
				}
			}
		}
#endif

		// Constant rotations
#if defined(ACL_IMPL_USE_BIT_SCAN_ITERATION_CONSTANT)
		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_constant_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			constant_track_cache_v0& constant_track_cache, track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, rotation_sub_track_types[entry_index].types);

				// Unpack our next 16 tracks
				constant_track_cache.unpack_rotation_group<decompression_settings_type>(context);

				// We have 2 bits per sub-track
				const uint32_t curr_entry_track_index = entry_index * 16;

				while (packed_entry != 0)
				{
					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					const rtm::quatf& rotation = constant_track_cache.consume_rotation();

					if (!writer.skip_track_rotation(track_index))
						writer.write_rotation(track_index, rotation);
				}
			}
		}
#else
		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_constant_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			constant_track_cache_v0& constant_track_cache, track_writer_type& writer)
		{
			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, rotation_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Unpack our next 16 tracks
				constant_track_cache.unpack_rotation_group<decompression_settings_type>(context);

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0x55000000) == 0)
						continue;	// This group contains no constant sub-tracks, skip it

					if ((packed_group & 0x40000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation();

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index0))
							writer.write_rotation(track_index0, rotation);
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation();

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index1))
							writer.write_rotation(track_index1, rotation);
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation();

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index2))
							writer.write_rotation(track_index2, rotation);
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;
						const rtm::quatf& rotation = constant_track_cache.consume_rotation();

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index3))
							writer.write_rotation(track_index3, rotation);
					}
				}
			}
		}
#endif

		// Animated rotations
#if defined(ACL_IMPL_USE_BIT_SCAN_ITERATION_ANIMATED)
		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_animated_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			animated_track_cache_v0& animated_track_cache, track_writer_type& writer)
		{
			if (track_writer_type::skip_all_rotations())
				return;

			const sample_rounding_policy rounding_policy = context.get_rounding_policy();

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but animated sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0xAAAAAAAAU, rotation_sub_track_types[entry_index].types);

				// We have 2 bits per sub-track
				const uint32_t curr_entry_track_index = entry_index * 16;

				while (packed_entry != 0)
				{
					// Unpack our next 4 tracks
					animated_track_cache.unpack_rotation_group<decompression_settings_type>(context);

					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					// We need the true rounding policy to be statically known when per track rounding is not supported
					// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
					// and rounding has already been performed for us.
					const sample_rounding_policy rounding_policy_ =
						decompression_settings_type::is_per_track_rounding_supported() ?
						writer.get_rounding_policy(rounding_policy, track_index) :
						sample_rounding_policy::none;

					ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

					const rtm::quatf& rotation = animated_track_cache.consume_rotation(rounding_policy_);

					ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
					ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

					if (!writer.skip_track_rotation(track_index))
						writer.write_rotation(track_index, rotation);
				}
			}
		}
#else
		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_animated_rotation_sub_tracks(
			const packed_sub_track_types* rotation_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			animated_track_cache_v0& animated_track_cache, track_writer_type& writer)
		{
			const sample_rounding_policy rounding_policy = context.get_rounding_policy();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but animated sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0xAAAAAAAAU, rotation_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no animated sub-tracks, skip it

					// Unpack our next 4 tracks
					animated_track_cache.unpack_rotation_group<decompression_settings_type>(context);

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index0) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::quatf& rotation = animated_track_cache.consume_rotation(rounding_policy_);

						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index0))
							writer.write_rotation(track_index0, rotation);
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index1) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::quatf& rotation = animated_track_cache.consume_rotation(rounding_policy_);

						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index1))
							writer.write_rotation(track_index1, rotation);
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index2) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::quatf& rotation = animated_track_cache.consume_rotation(rounding_policy_);

						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index2))
							writer.write_rotation(track_index2, rotation);
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index3) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::quatf& rotation = animated_track_cache.consume_rotation(rounding_policy_);

						ACL_ASSERT(rtm::quat_is_finite(rotation), "Rotation is not valid!");
						ACL_ASSERT(rtm::quat_is_normalized(rotation), "Rotation is not normalized!");

						if (!track_writer_type::skip_all_rotations() && !writer.skip_track_rotation(track_index3))
							writer.write_rotation(track_index3, rotation);
					}
				}
			}
		}
#endif

		// Default translations
#if defined(ACL_IMPL_USE_UNROLLED_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_translation_sub_tracks(
			const packed_sub_track_types* translation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_translations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_translation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for translations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default translation if we have one, otherwise init with some value
			const rtm::vector4f default_translation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_translation() : rtm::vector_zero();

			// Each entry can have at most 16 indices packed
			uint32_t unpacked_indices[16];
			uint32_t* unpacked_index_ptr = unpacked_indices;

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = translation_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = ~packed_entry - 0x55555555;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				const uint32_t base_track_index = entry_index * 16;

				if (packed_entry & 0xAA000000)
				{
					const uint32_t index_0 = (packed_entry >> 31) & 0x01;
					const uint32_t index_1 = (packed_entry >> 29) & 0x01;
					const uint32_t index_2 = (packed_entry >> 27) & 0x01;
					const uint32_t index_3 = (packed_entry >> 25) & 0x01;

					*unpacked_index_ptr = base_track_index | 0;
					unpacked_index_ptr += index_0;

					*unpacked_index_ptr = base_track_index | 1;
					unpacked_index_ptr += index_1;

					*unpacked_index_ptr = base_track_index | 2;
					unpacked_index_ptr += index_2;

					*unpacked_index_ptr = base_track_index | 3;
					unpacked_index_ptr += index_3;
				}

				if (packed_entry & 0x00AA0000)
				{
					const uint32_t index_4 = (packed_entry >> 23) & 0x01;
					const uint32_t index_5 = (packed_entry >> 21) & 0x01;
					const uint32_t index_6 = (packed_entry >> 19) & 0x01;
					const uint32_t index_7 = (packed_entry >> 17) & 0x01;

					*unpacked_index_ptr = base_track_index | 4;
					unpacked_index_ptr += index_4;

					*unpacked_index_ptr = base_track_index | 5;
					unpacked_index_ptr += index_5;

					*unpacked_index_ptr = base_track_index | 6;
					unpacked_index_ptr += index_6;

					*unpacked_index_ptr = base_track_index | 7;
					unpacked_index_ptr += index_7;
				}

				if (packed_entry & 0x0000AA00)
				{
					const uint32_t index_8 = (packed_entry >> 15) & 0x01;
					const uint32_t index_9 = (packed_entry >> 13) & 0x01;
					const uint32_t index_10 = (packed_entry >> 11) & 0x01;
					const uint32_t index_11 = (packed_entry >> 9) & 0x01;

					*unpacked_index_ptr = base_track_index | 8;
					unpacked_index_ptr += index_8;

					*unpacked_index_ptr = base_track_index | 9;
					unpacked_index_ptr += index_9;

					*unpacked_index_ptr = base_track_index | 10;
					unpacked_index_ptr += index_10;

					*unpacked_index_ptr = base_track_index | 11;
					unpacked_index_ptr += index_11;
				}

				if (packed_entry & 0x000000AA)
				{
					const uint32_t index_12 = (packed_entry >> 7) & 0x01;
					const uint32_t index_13 = (packed_entry >> 5) & 0x01;
					const uint32_t index_14 = (packed_entry >> 3) & 0x01;
					const uint32_t index_15 = (packed_entry >> 1) & 0x01;

					*unpacked_index_ptr = base_track_index | 12;
					unpacked_index_ptr += index_12;

					*unpacked_index_ptr = base_track_index | 13;
					unpacked_index_ptr += index_13;

					*unpacked_index_ptr = base_track_index | 14;
					unpacked_index_ptr += index_14;

					*unpacked_index_ptr = base_track_index | 15;
					unpacked_index_ptr += index_15;
				}

				const uint32_t* unpacked_index_read_ptr = unpacked_indices;
				while (unpacked_index_read_ptr < unpacked_index_ptr)
				//for (uint32_t unpacked_index = 0; unpacked_index < num_unpacked_indices; ++unpacked_index)
				{
					//const uint32_t unpacked_track_index = unpacked_indices[unpacked_index];
					const uint32_t unpacked_track_index = *unpacked_index_read_ptr;
					unpacked_index_read_ptr++;

					if (!writer.skip_track_translation(unpacked_track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_translation(unpacked_track_index, writer.get_variable_default_translation(unpacked_track_index));
						else
							writer.write_translation(unpacked_track_index, default_translation);
					}
				}

				// Reset
				unpacked_index_ptr = unpacked_indices;
			}
		}
#elif defined(ACL_IMPL_USE_BIT_SCAN_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_translation_sub_tracks(
			const packed_sub_track_types* translation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			if (track_writer_type::skip_all_translations())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_translation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for translations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default translation if we have one, otherwise init with some value
			const rtm::vector4f default_translation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_translation() : rtm::vector_zero();

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = translation_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				// We have 2 bits per sub-track
				const uint32_t curr_entry_track_index = entry_index * 16;

				while (packed_entry != 0)
				{
					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					if (!writer.skip_track_translation(track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_translation(track_index, writer.get_variable_default_translation(track_index));
						else
							writer.write_translation(track_index, default_translation);
					}
				}
			}
		}
#else
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_translation_sub_tracks(
			const packed_sub_track_types* translation_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			track_writer_type& writer)
		{
			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_translation_mode();
			static_assert(default_mode != default_sub_track_mode::legacy, "Not supported for translations");
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default translation if we have one, otherwise init with some value
			const rtm::vector4f default_translation = default_mode == default_sub_track_mode::constant ? writer.get_constant_default_translation() : rtm::vector_zero();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = translation_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no default sub-tracks, skip it

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index0))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_translation(track_index0, writer.get_variable_default_translation(track_index0));
							else
								writer.write_translation(track_index0, default_translation);
						}
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index1))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_translation(track_index1, writer.get_variable_default_translation(track_index1));
							else
								writer.write_translation(track_index1, default_translation);
						}
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index2))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_translation(track_index2, writer.get_variable_default_translation(track_index2));
							else
								writer.write_translation(track_index2, default_translation);
						}
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index3))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_translation(track_index3, writer.get_variable_default_translation(track_index3));
							else
								writer.write_translation(track_index3, default_translation);
						}
					}
				}
			}
		}
#endif

		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_constant_translation_sub_tracks(
			const packed_sub_track_types* translation_sub_track_types, uint32_t last_entry_index,
			constant_track_cache_v0& constant_track_cache, track_writer_type& writer)
		{
			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, translation_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0x55000000) == 0)
						continue;	// This group contains no constant sub-tracks, skip it

					if ((packed_group & 0x40000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;
						const uint8_t* translation_ptr = constant_track_cache.consume_translation();

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index0))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index0, translation);
						}
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;
						const uint8_t* translation_ptr = constant_track_cache.consume_translation();

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index1))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index1, translation);
						}
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;
						const uint8_t* translation_ptr = constant_track_cache.consume_translation();

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index2))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index2, translation);
						}
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;
						const uint8_t* translation_ptr = constant_track_cache.consume_translation();

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index3))
						{
							const rtm::vector4f translation = rtm::vector_load(translation_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

							writer.write_translation(track_index3, translation);
						}
					}
				}
			}
		}

		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_adapter_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_animated_translation_sub_tracks(
			const packed_sub_track_types* translation_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			animated_track_cache_v0& animated_track_cache, track_writer_type& writer)
		{
			const sample_rounding_policy rounding_policy = context.get_rounding_policy();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but animated sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0xAAAAAAAAU, translation_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no animated sub-tracks, skip it

					// Unpack our next 4 tracks
					animated_track_cache.unpack_translation_group<decompression_settings_adapter_type>(context);

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index0) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& translation = animated_track_cache.consume_translation(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index0))
							writer.write_translation(track_index0, translation);
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index1) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& translation = animated_track_cache.consume_translation(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index1))
							writer.write_translation(track_index1, translation);
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index2) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& translation = animated_track_cache.consume_translation(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index2))
							writer.write_translation(track_index2, translation);
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index3) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& translation = animated_track_cache.consume_translation(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(translation), "Translation is not valid!");

						if (!track_writer_type::skip_all_translations() && !writer.skip_track_translation(track_index3))
							writer.write_translation(track_index3, translation);
					}
				}
			}
		}

		// Default scales
#if defined(ACL_IMPL_USE_UNROLLED_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_scale_sub_tracks(
			const packed_sub_track_types* scale_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			rtm::vector4f_arg0 default_scale_, track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_scale_mode();
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default scale if we have one, otherwise init with some value
			rtm::vector4f default_scale;
			if (default_mode == default_sub_track_mode::constant)
				default_scale = writer.get_constant_default_scale();
			else if (default_mode == default_sub_track_mode::legacy)
				default_scale = default_scale_;
			else
				default_scale = rtm::vector_zero();

			// Each entry can have at most 16 indices packed
			uint32_t unpacked_indices[16];
			uint32_t* unpacked_index_ptr = unpacked_indices;

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = scale_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = ~packed_entry - 0x55555555;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				const uint32_t base_track_index = entry_index * 16;

				if (packed_entry & 0xAA000000)
				{
					const uint32_t index_0 = (packed_entry >> 31) & 0x01;
					const uint32_t index_1 = (packed_entry >> 29) & 0x01;
					const uint32_t index_2 = (packed_entry >> 27) & 0x01;
					const uint32_t index_3 = (packed_entry >> 25) & 0x01;

					*unpacked_index_ptr = base_track_index | 0;
					unpacked_index_ptr += index_0;

					*unpacked_index_ptr = base_track_index | 1;
					unpacked_index_ptr += index_1;

					*unpacked_index_ptr = base_track_index | 2;
					unpacked_index_ptr += index_2;

					*unpacked_index_ptr = base_track_index | 3;
					unpacked_index_ptr += index_3;
				}

				if (packed_entry & 0x00AA0000)
				{
					const uint32_t index_4 = (packed_entry >> 23) & 0x01;
					const uint32_t index_5 = (packed_entry >> 21) & 0x01;
					const uint32_t index_6 = (packed_entry >> 19) & 0x01;
					const uint32_t index_7 = (packed_entry >> 17) & 0x01;

					*unpacked_index_ptr = base_track_index | 4;
					unpacked_index_ptr += index_4;

					*unpacked_index_ptr = base_track_index | 5;
					unpacked_index_ptr += index_5;

					*unpacked_index_ptr = base_track_index | 6;
					unpacked_index_ptr += index_6;

					*unpacked_index_ptr = base_track_index | 7;
					unpacked_index_ptr += index_7;
				}

				if (packed_entry & 0x0000AA00)
				{
					const uint32_t index_8 = (packed_entry >> 15) & 0x01;
					const uint32_t index_9 = (packed_entry >> 13) & 0x01;
					const uint32_t index_10 = (packed_entry >> 11) & 0x01;
					const uint32_t index_11 = (packed_entry >> 9) & 0x01;

					*unpacked_index_ptr = base_track_index | 8;
					unpacked_index_ptr += index_8;

					*unpacked_index_ptr = base_track_index | 9;
					unpacked_index_ptr += index_9;

					*unpacked_index_ptr = base_track_index | 10;
					unpacked_index_ptr += index_10;

					*unpacked_index_ptr = base_track_index | 11;
					unpacked_index_ptr += index_11;
				}

				if (packed_entry & 0x000000AA)
				{
					const uint32_t index_12 = (packed_entry >> 7) & 0x01;
					const uint32_t index_13 = (packed_entry >> 5) & 0x01;
					const uint32_t index_14 = (packed_entry >> 3) & 0x01;
					const uint32_t index_15 = (packed_entry >> 1) & 0x01;

					*unpacked_index_ptr = base_track_index | 12;
					unpacked_index_ptr += index_12;

					*unpacked_index_ptr = base_track_index | 13;
					unpacked_index_ptr += index_13;

					*unpacked_index_ptr = base_track_index | 14;
					unpacked_index_ptr += index_14;

					*unpacked_index_ptr = base_track_index | 15;
					unpacked_index_ptr += index_15;
				}

				const uint32_t* unpacked_index_read_ptr = unpacked_indices;
				while (unpacked_index_read_ptr < unpacked_index_ptr)
				//for (uint32_t unpacked_index = 0; unpacked_index < num_unpacked_indices; ++unpacked_index)
				{
					//const uint32_t unpacked_track_index = unpacked_indices[unpacked_index];
					const uint32_t unpacked_track_index = *unpacked_index_read_ptr;
					unpacked_index_read_ptr++;

					if (!writer.skip_track_scale(unpacked_track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_scale(unpacked_track_index, writer.get_variable_default_scale(unpacked_track_index));
						else
							writer.write_scale(unpacked_track_index, default_scale);
					}
				}

				// Reset
				unpacked_index_ptr = unpacked_indices;
			}
		}
#elif defined(ACL_IMPL_USE_BIT_SCAN_ITERATION_DEFAULT)
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_scale_sub_tracks(
			const packed_sub_track_types* scale_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			rtm::vector4f_arg0 default_scale_, track_writer_type& writer)
		{
			if (track_writer_type::skip_all_scales())
				return;

			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_scale_mode();
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default scale if we have one, otherwise init with some value
			rtm::vector4f default_scale;
			if (default_mode == default_sub_track_mode::constant)
				default_scale = writer.get_constant_default_scale();
			else if (default_mode == default_sub_track_mode::legacy)
				default_scale = default_scale_;
			else
				default_scale = rtm::vector_zero();

			for (uint32_t entry_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = scale_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				// We have 2 bits per sub-track
				const uint32_t curr_entry_track_index = entry_index * 16;

				while (packed_entry != 0)
				{
					const uint32_t set_bit_index = count_leading_zeros(packed_entry);
					const uint32_t highest_set_bit = 1 << (31 - set_bit_index);

					// Mask out the bit we just consumed
					packed_entry ^= highest_set_bit;

					// We have 2 bits per sub-track
					const uint32_t track_index = curr_entry_track_index + (set_bit_index / 2);

					if (!writer.skip_track_scale(track_index))
					{
						if (default_mode == default_sub_track_mode::variable)
							writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
						else
							writer.write_scale(track_index, default_scale);
					}
				}
			}
		}
#else
		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_default_scale_sub_tracks(
			const packed_sub_track_types* scale_sub_track_types, uint32_t last_entry_index, uint32_t padding_mask,
			rtm::vector4f_arg0 default_scale_, track_writer_type& writer)
		{
			constexpr default_sub_track_mode default_mode = track_writer_type::get_default_scale_mode();
			if (default_mode == default_sub_track_mode::skipped)
				return;	// Nothing to write

			// Grab our constant default scale if we have one, otherwise init with some value
			rtm::vector4f default_scale;
			if (default_mode == default_sub_track_mode::constant)
				default_scale = writer.get_constant_default_scale();
			else if (default_mode == default_sub_track_mode::legacy)
				default_scale = default_scale_;
			else
				default_scale = rtm::vector_zero();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				uint32_t packed_entry = scale_sub_track_types[entry_index].types;

				// Mask out everything but default sub-tracks, this way we can early out when we iterate
				// Each sub-track is either 0 (default), 1 (constant), or 2 (animated)
				// By flipping the bits with logical NOT, 0 becomes 3, 1 becomes 2, and 2 becomes 1
				// We then subtract 1 from every group so 3 becomes 2, 2 becomes 1, and 1 becomes 0
				// Finally, we mask out everything but the second bit for each sub-track
				// After this, our original default tracks are equal to 2, our constant tracks are equal to 1, and our animated tracks are equal to 0
				// Testing for default tracks can be done by testing the second bit of each group (same as animated track testing)
				packed_entry = (~packed_entry - 0x55555555) & 0xAAAAAAAA;

				// Because our last entry might have padding with 0 (default), we have to strip any padding we might have
				const uint32_t entry_padding_mask = (entry_index == last_entry_index) ? padding_mask : 0xFFFFFFFF;
				packed_entry &= entry_padding_mask;

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no default sub-tracks, skip it

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index0))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_scale(track_index0, writer.get_variable_default_scale(track_index0));
							else
								writer.write_scale(track_index0, default_scale);
						}
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index1))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_scale(track_index1, writer.get_variable_default_scale(track_index1));
							else
								writer.write_scale(track_index1, default_scale);
						}
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index2))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_scale(track_index2, writer.get_variable_default_scale(track_index2));
							else
								writer.write_scale(track_index2, default_scale);
						}
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index3))
						{
							if (default_mode == default_sub_track_mode::variable)
								writer.write_scale(track_index3, writer.get_variable_default_scale(track_index3));
							else
								writer.write_scale(track_index3, default_scale);
						}
					}
				}
			}
		}
#endif

		// Force inline this function, we only use it to keep the code readable
		template<class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_constant_scale_sub_tracks(
			const packed_sub_track_types* scale_sub_track_types, uint32_t last_entry_index,
			constant_track_cache_v0& constant_track_cache, track_writer_type& writer)
		{
			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but constant sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0x55555555U, scale_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0x55000000) == 0)
						continue;	// This group contains no constant sub-tracks, skip it

					if ((packed_group & 0x40000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;
						const uint8_t* scale_ptr = constant_track_cache.consume_scale();

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index0))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index0, scale);
						}
					}

					if ((packed_group & 0x10000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;
						const uint8_t* scale_ptr = constant_track_cache.consume_scale();

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index1))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index1, scale);
						}
					}

					if ((packed_group & 0x04000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;
						const uint8_t* scale_ptr = constant_track_cache.consume_scale();

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index2))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index2, scale);
						}
					}

					if ((packed_group & 0x01000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;
						const uint8_t* scale_ptr = constant_track_cache.consume_scale();

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index3))
						{
							const rtm::vector4f scale = rtm::vector_load(scale_ptr);
							ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

							writer.write_scale(track_index3, scale);
						}
					}
				}
			}
		}

		// Force inline this function, we only use it to keep the code readable
		template<class decompression_settings_adapter_type, class track_writer_type>
		ACL_IMPL_DEBUG_FORCE_INLINE RTM_DISABLE_SECURITY_COOKIE_CHECK void RTM_SIMD_CALL unpack_animated_scale_sub_tracks(
			const packed_sub_track_types* scale_sub_track_types, uint32_t last_entry_index,
			const persistent_transform_decompression_context_v0& context,
			animated_track_cache_v0& animated_track_cache, track_writer_type& writer)
		{
			const sample_rounding_policy rounding_policy = context.get_rounding_policy();

			for (uint32_t entry_index = 0, track_index = 0; entry_index <= last_entry_index; ++entry_index)
			{
				// Mask out everything but animated sub-tracks, this way we can early out when we iterate
				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				uint32_t packed_entry = and_not(~0xAAAAAAAAU, scale_sub_track_types[entry_index].types);

				uint32_t curr_entry_track_index = track_index;

				// We might early out below, always skip 16 tracks
				track_index += 16;

				// Process 4 sub-tracks at a time
				while (packed_entry != 0)
				{
					const uint32_t packed_group = packed_entry;
					const uint32_t curr_group_track_index = curr_entry_track_index;

					// Move to the next group
					packed_entry <<= 8;
					curr_entry_track_index += 4;

					if ((packed_group & 0xAA000000) == 0)
						continue;	// This group contains no animated sub-tracks, skip it

					// Unpack our next 4 tracks
					animated_track_cache.unpack_scale_group<decompression_settings_adapter_type>(context);

					if ((packed_group & 0x80000000) != 0)
					{
						const uint32_t track_index0 = curr_group_track_index + 0;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index0) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& scale = animated_track_cache.consume_scale(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index0))
							writer.write_scale(track_index0, scale);
					}

					if ((packed_group & 0x20000000) != 0)
					{
						const uint32_t track_index1 = curr_group_track_index + 1;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index1) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& scale = animated_track_cache.consume_scale(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index1))
							writer.write_scale(track_index1, scale);
					}

					if ((packed_group & 0x08000000) != 0)
					{
						const uint32_t track_index2 = curr_group_track_index + 2;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index2) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& scale = animated_track_cache.consume_scale(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index2))
							writer.write_scale(track_index2, scale);
					}

					if ((packed_group & 0x02000000) != 0)
					{
						const uint32_t track_index3 = curr_group_track_index + 3;

						// We need the true rounding policy to be statically known when per track rounding is not supported
						// When it isn't supported, we always use 'none' since the interpolation alpha was properly calculated
						// and rounding has already been performed for us.
						const sample_rounding_policy rounding_policy_ =
							decompression_settings_adapter_type::is_per_track_rounding_supported() ?
							writer.get_rounding_policy(rounding_policy, track_index3) :
							sample_rounding_policy::none;

						ACL_ASSERT(rounding_policy_ != sample_rounding_policy::per_track, "track_writer::get_rounding_policy() cannot return per_track");

						const rtm::vector4f& scale = animated_track_cache.consume_scale(rounding_policy_);

						ACL_ASSERT(rtm::vector_is_finite3(scale), "Scale is not valid!");

						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index3))
							writer.write_scale(track_index3, scale);
					}
				}
			}
		}

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_tracks_v0(const persistent_transform_decompression_context_v0& context, track_writer_type& writer)
		{
			const compressed_tracks* tracks = context.tracks;
			const tracks_header& header = get_tracks_header(*tracks);
			const uint32_t num_tracks = header.num_tracks;
			if (num_tracks == 0)
				return;	// Empty track list

			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			const rtm::vector4f default_scale = rtm::vector_set(float(header.get_default_scale()));
			const uint32_t has_scale = context.has_scale;

			const packed_sub_track_types* sub_track_types = get_transform_tracks_header(*tracks).get_sub_track_types();
			const uint32_t num_sub_track_entries = (num_tracks + k_num_sub_tracks_per_packed_entry - 1) / k_num_sub_tracks_per_packed_entry;
			const uint32_t num_padded_sub_tracks = (num_sub_track_entries * k_num_sub_tracks_per_packed_entry) - num_tracks;
			const uint32_t last_entry_index = num_sub_track_entries - 1;

			// Build a mask to strip the extra sub-tracks we don't need that live in the padding
			// They are set to 0 which means they would be 'default' sub-tracks but they don't really exist
			// If we have no padding, we retain every sub-track
			// Sub-tracks that are kept have their bits set to 1 to mask them with logical AND later
			const uint32_t padding_mask = num_padded_sub_tracks != 0 ? ~(0xFFFFFFFF >> ((k_num_sub_tracks_per_packed_entry - num_padded_sub_tracks) * 2)) : 0xFFFFFFFF;

			const packed_sub_track_types* rotation_sub_track_types = sub_track_types;
			const packed_sub_track_types* translation_sub_track_types = rotation_sub_track_types + num_sub_track_entries;
			const packed_sub_track_types* scale_sub_track_types = translation_sub_track_types + num_sub_track_entries;

			constant_track_cache_v0 constant_track_cache;
			constant_track_cache.initialize<decompression_settings_type>(context);

			{
				// By now, our bit sets (1-2 cache lines) constant rotations (2 cache lines) have landed in the L2
				// We prefetched them ahead in the seek(..) function call and due to cache misses when seeking,
				// their latency should be fully hidden.
				// Prefetch our 3rd constant rotation cache line to prime the hardware prefetcher and do the same for constant translations

				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_rotations + 128);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations + 64);
				ACL_IMPL_SEEK_PREFETCH(constant_track_cache.constant_data_translations + 128);
			}

			animated_track_cache_v0 animated_track_cache;
			animated_track_cache.initialize<decompression_settings_type, translation_adapter>(context);

			{
				// Start prefetching the per track metadata of both segments
				// They might live in a different memory page than the clip's header and constant data
				// and we need to prime VMEM translation and the TLB

				ACL_IMPL_SEEK_PREFETCH(context.format_per_track_data[0]);
				ACL_IMPL_SEEK_PREFETCH(context.format_per_track_data[1]);
			}

			// TODO: The first time we iterate over the sub-track types, unpack it into our output pose as a temporary buffer
			// We can build a linked list
			// Store on the stack the first animated rot/trans/scale
			// For its rot/trans/scale, write instead the index of the next animated rot/trans/scale
			// We can even unpack it first on its own
			// Writer can expose this with something like write_rotation_index/read_rotation_index
			// The writer can then allocate a separate buffer for this or re-use the pose buffer
			// When the time comes to write our animated samples, we can unpack 4, grab the next 4 entries from the linked
			// list and write our samples. We can do this until all samples are written which should be faster than iterating a bit set
			// since it'll allow us to quickly skip entries we don't care about. The same scheme can be used for constant/default tracks.
			// When we unpack our bitset, we can also count the number of entries for each type to help iterate

			// Unpack our default rotation sub-tracks
			// Default rotation sub-tracks are uncommon, this shouldn't take much more than 50 cycles
			unpack_default_rotation_sub_tracks(rotation_sub_track_types, last_entry_index, padding_mask, writer);

			// Unpack our constant rotation sub-tracks
			// Constant rotation sub-tracks are very common, this should take at least 200 cycles
			unpack_constant_rotation_sub_tracks<decompression_settings_type>(rotation_sub_track_types, last_entry_index, context, constant_track_cache, writer);

			// By now, our constant translations (3 cache lines) have landed in L2 after our prefetching has completed
			// We typically will do enough work above to hide the latency
			// We do not prefetch our constant scales because scale is fairly rare
			// Instead, we prefetch our segment range and animated data
			// The second key frame of animated data might not live in the same memory page even if we use a single segment
			// so this allows us to prime the TLB as well
			{
				const uint8_t* segment_range_data0 = animated_track_cache.segment_sampling_context_rotations[0].segment_range_data;
				const uint8_t* segment_range_data1 = animated_track_cache.segment_sampling_context_rotations[1].segment_range_data;
				const uint8_t* animated_data0 = animated_track_cache.segment_sampling_context_rotations[0].animated_track_data;
				const uint8_t* animated_data1 = animated_track_cache.segment_sampling_context_rotations[1].animated_track_data;
				const uint8_t* frame_animated_data0 = animated_data0 + (animated_track_cache.segment_sampling_context_rotations[0].animated_track_data_bit_offset / 8);
				const uint8_t* frame_animated_data1 = animated_data1 + (animated_track_cache.segment_sampling_context_rotations[1].animated_track_data_bit_offset / 8);

				ACL_IMPL_SEEK_PREFETCH(segment_range_data0);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data0 + 64);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data1);
				ACL_IMPL_SEEK_PREFETCH(segment_range_data1 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data0);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data1);
			}

			// Unpack our default translation sub-tracks
			// Default translation sub-tracks are rare, this shouldn't take much more than 50 cycles
			unpack_default_translation_sub_tracks(translation_sub_track_types, last_entry_index, padding_mask, writer);

			// Unpack our constant translation sub-tracks
			// Constant translation sub-tracks are very common, this should take at least 200 cycles
			unpack_constant_translation_sub_tracks(translation_sub_track_types, last_entry_index, constant_track_cache, writer);

			if (has_scale)
			{
				// Unpack our default scale sub-tracks
				// Scale sub-tracks are almost always default, this should take at least 200 cycles
				unpack_default_scale_sub_tracks(scale_sub_track_types, last_entry_index, padding_mask, default_scale, writer);

				// Unpack our constant scale sub-tracks
				// Constant scale sub-tracks are very rare, this shouldn't take much more than 50 cycles
				unpack_constant_scale_sub_tracks(scale_sub_track_types, last_entry_index, constant_track_cache, writer);
			}
			else
			{
				constexpr default_sub_track_mode default_scale_mode = track_writer_type::get_default_scale_mode();
				if (default_scale_mode != default_sub_track_mode::skipped)
				{
					// Grab our constant default scale if we have one, otherwise init with some value
					rtm::vector4f scale;
					if (default_scale_mode == default_sub_track_mode::constant)
						scale = writer.get_constant_default_scale();
					else if (default_scale_mode == default_sub_track_mode::legacy)
						scale = default_scale;
					else
						scale = rtm::vector_zero();

					// No scale present, everything is just the default value
					// This shouldn't take much more than 50 cycles
					for (uint32_t track_index = 0; track_index < num_tracks; ++track_index)
					{
						if (!track_writer_type::skip_all_scales() && !writer.skip_track_scale(track_index))
						{
							if (default_scale_mode == default_sub_track_mode::variable)
								writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
							else
								writer.write_scale(track_index, scale);
						}
					}
				}
			}

			{
				// By now the first few cache lines of our segment data has landed in the L2
				// Prefetch ahead some more to prime the hardware prefetcher
				// We also start prefetching the clip range data since we'll need it soon and we need to prime the TLB
				// and the hardware prefetcher

				const uint8_t* per_track_metadata0 = animated_track_cache.segment_sampling_context_rotations[0].format_per_track_data;
				const uint8_t* per_track_metadata1 = animated_track_cache.segment_sampling_context_rotations[1].format_per_track_data;
				const uint8_t* animated_data0 = animated_track_cache.segment_sampling_context_rotations[0].animated_track_data;
				const uint8_t* animated_data1 = animated_track_cache.segment_sampling_context_rotations[1].animated_track_data;
				const uint8_t* frame_animated_data0 = animated_data0 + (animated_track_cache.segment_sampling_context_rotations[0].animated_track_data_bit_offset / 8);
				const uint8_t* frame_animated_data1 = animated_data1 + (animated_track_cache.segment_sampling_context_rotations[1].animated_track_data_bit_offset / 8);

				ACL_IMPL_SEEK_PREFETCH(per_track_metadata0 + 64);
				ACL_IMPL_SEEK_PREFETCH(per_track_metadata1 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data0 + 64);
				ACL_IMPL_SEEK_PREFETCH(frame_animated_data1 + 64);
				ACL_IMPL_SEEK_PREFETCH(animated_track_cache.clip_sampling_context_rotations.clip_range_data);
				ACL_IMPL_SEEK_PREFETCH(animated_track_cache.clip_sampling_context_rotations.clip_range_data + 64);

				// TODO: Can we prefetch the translation data ahead instead to prime the TLB?
			}

			// Unpack our variable sub-tracks
			// Sub-track data is sorted by type: rotations ... translations ... scales ...
			// We process everything linearly in order, this should help the hardware prefetcher work with us
			// We use quite a few memory streams:
			//    - segment per track metadata: 1 per segment
			//    - segment range data: 1 per segment
			//    - animated frame data: 2 (might be in different segments or in database)
			//    - clip range data: 1
			// We thus need between 5 and 7 memory streams which is a lot.
			// This is why the unpacking code uses manual software prefetching to ensure prefetching happens.
			// Removing the manual prefetching slows down the execution on Zen2 and a Pixel 3.
			// Quite a few of these memory streams might live in separate memory pages if the clip is large
			// and might thus require TLB misses

			// TODO: Unpack 4, then iterate over tracks to write?
			// Can we keep the rotations in registers? Does it matter?

			// Unpack rotations first
			// Animated rotation sub-tracks are very common, this should take at least 400 cycles
			unpack_animated_rotation_sub_tracks<decompression_settings_type>(rotation_sub_track_types, last_entry_index, context, animated_track_cache, writer);

			// Unpack translations second
			// Animated translation sub-tracks are common, this should take at least 200 cycles
			unpack_animated_translation_sub_tracks<translation_adapter>(translation_sub_track_types, last_entry_index, context, animated_track_cache, writer);

			// Unpack scales last
			// Animated scale sub-tracks are very rare, this shouldn't take much more than 100 cycles
			if (has_scale)
				unpack_animated_scale_sub_tracks<scale_adapter>(scale_sub_track_types, last_entry_index, context, animated_track_cache, writer);

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		// We only initialize some variables when we need them which prompts the compiler to complain
		// The usage is perfectly safe and because this code is VERY hot and needs to be as fast as possible,
		// we disable the warning to avoid zeroing out things we don't need
#if defined(RTM_COMPILER_MSVC)
		#pragma warning(push)
		// warning C4701: potentially uninitialized local variable
		#pragma warning(disable : 4701)
		// warning C6001: Using uninitialized memory '...'.
		#pragma warning(disable : 6001)
#elif defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

		template<class decompression_settings_type, class track_writer_type>
		inline void decompress_track_v0(const persistent_transform_decompression_context_v0& context, uint32_t track_index, track_writer_type& writer)
		{
			const compressed_tracks* tracks = context.tracks;
			const tracks_header& tracks_header_ = get_tracks_header(*tracks);
			const uint32_t num_tracks = tracks_header_.num_tracks;
			if (num_tracks == 0)
				return;	// Empty track list

			ACL_ASSERT(context.sample_time >= 0.0f, "Context not set to a valid sample time");
			if (context.sample_time < 0.0F)
				return;	// Invalid sample time, we didn't seek yet

			ACL_ASSERT(track_index < num_tracks, "Invalid track index");
			if (track_index >= num_tracks)
				return;	// Invalid track index

			// Due to the SIMD operations, we sometimes overflow in the SIMD lanes not used.
			// Disable floating point exceptions to avoid issues.
			fp_environment fp_env;
			if (decompression_settings_type::disable_fp_exeptions())
				disable_fp_exceptions(fp_env);

			using translation_adapter = acl_impl::translation_decompression_settings_adapter<decompression_settings_type>;
			using scale_adapter = acl_impl::scale_decompression_settings_adapter<decompression_settings_type>;

			constexpr default_sub_track_mode default_rotation_mode = track_writer_type::get_default_rotation_mode();
			constexpr default_sub_track_mode default_translation_mode = track_writer_type::get_default_translation_mode();
			constexpr default_sub_track_mode default_scale_mode = track_writer_type::get_default_scale_mode();

			static_assert(default_rotation_mode != default_sub_track_mode::legacy, "Not supported for rotations");
			static_assert(default_translation_mode != default_sub_track_mode::legacy, "Not supported for translations");

			// Grab our constant default values if we have one, otherwise init with some value
			const rtm::quatf default_rotation = default_rotation_mode == default_sub_track_mode::constant ? writer.get_constant_default_rotation() : rtm::quat_identity();
			const rtm::vector4f default_translation = default_translation_mode == default_sub_track_mode::constant ? writer.get_constant_default_translation() : rtm::vector_zero();

			rtm::vector4f default_scale;
			if (default_scale_mode == default_sub_track_mode::constant)
				default_scale = writer.get_constant_default_scale();
			else if (default_scale_mode == default_sub_track_mode::legacy)
				default_scale = rtm::vector_set(float(tracks_header_.get_default_scale()));
			else
				default_scale = rtm::vector_zero();

			const uint32_t has_scale = context.has_scale;

			const packed_sub_track_types* sub_track_types = get_transform_tracks_header(*tracks).get_sub_track_types();
			const uint32_t num_sub_track_entries = (num_tracks + k_num_sub_tracks_per_packed_entry - 1) / k_num_sub_tracks_per_packed_entry;

			const packed_sub_track_types* rotation_sub_track_types = sub_track_types;
			const packed_sub_track_types* translation_sub_track_types = rotation_sub_track_types + num_sub_track_entries;

			// If we have no scale, we'll load the rotation sub-track types and mask it out to avoid branching, forcing it to be the default value
			const packed_sub_track_types* scale_sub_track_types = has_scale ? (translation_sub_track_types + num_sub_track_entries) : sub_track_types;

			// Build a mask to strip out the scale sub-track types if we have no scale present
			// has_scale is either 0 or 1, negating yields 0 (0x00000000) or -1 (0xFFFFFFFF)
			// Equivalent to: has_scale ? 0xFFFFFFFF : 0x00000000
			const uint32_t scale_sub_track_mask = static_cast<uint32_t>(-int32_t(has_scale));

			const uint32_t sub_track_entry_index = track_index / 16;
			const uint32_t packed_index = track_index % 16;

			// Shift our sub-track types so that the sub-track we care about ends up in the LSB position
			const uint32_t packed_shift = (15 - packed_index) * 2;

			const uint32_t rotation_sub_track_type = (rotation_sub_track_types[sub_track_entry_index].types >> packed_shift) & 0x3;
			const uint32_t translation_sub_track_type = (translation_sub_track_types[sub_track_entry_index].types >> packed_shift) & 0x3;
			const uint32_t scale_sub_track_type = scale_sub_track_mask & (scale_sub_track_types[sub_track_entry_index].types >> packed_shift) & 0x3;

			// Combine all three so we can quickly test if all are default and if any are constant/animated
			const uint32_t combined_sub_track_type = rotation_sub_track_type | translation_sub_track_type | scale_sub_track_type;

			if (combined_sub_track_type == 0)
			{
				// Everything is default
				if (default_rotation_mode != default_sub_track_mode::skipped)
				{
					if (default_rotation_mode == default_sub_track_mode::variable)
						writer.write_rotation(track_index, writer.get_variable_default_rotation(track_index));
					else
						writer.write_rotation(track_index, default_rotation);
				}

				if (default_translation_mode != default_sub_track_mode::skipped)
				{
					if (default_translation_mode == default_sub_track_mode::variable)
						writer.write_translation(track_index, writer.get_variable_default_translation(track_index));
					else
						writer.write_translation(track_index, default_translation);
				}

				if (default_scale_mode != default_sub_track_mode::skipped)
				{
					if (default_scale_mode == default_sub_track_mode::variable)
						writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
					else
						writer.write_scale(track_index, default_scale);
				}

				return;
			}

			uint32_t num_constant_rotations = 0;
			uint32_t num_constant_translations = 0;
			uint32_t num_constant_scales = 0;
			uint32_t num_animated_rotations = 0;
			uint32_t num_animated_translations = 0;
			uint32_t num_animated_scales = 0;

			const uint32_t last_entry_index = track_index / k_num_sub_tracks_per_packed_entry;
			const uint32_t num_padded_sub_tracks = ((last_entry_index + 1) * k_num_sub_tracks_per_packed_entry) - track_index;

			// Build a mask to strip the extra sub-tracks we don't need that live in the padding
			// They are set to 0 which means they would be 'default' sub-tracks but they don't really exist
			// If we have no padding, we retain every sub-track
			// Sub-tracks that are kept have their bits set to 0 to mask them with logical ANDNOT later
			const uint32_t padding_mask = num_padded_sub_tracks != 0 ? (0xFFFFFFFF >> ((k_num_sub_tracks_per_packed_entry - num_padded_sub_tracks) * 2)) : 0x00000000;

			for (uint32_t sub_track_entry_index_ = 0; sub_track_entry_index_ <= last_entry_index; ++sub_track_entry_index_)
			{
				// Our last entry might contain more information than we need so we strip the padding we don't need
				const uint32_t entry_padding_mask = (sub_track_entry_index_ == last_entry_index) ? padding_mask : 0x00000000;

				// Use and_not(..) to load our sub-track types directly from memory on x64 with BMI
				const uint32_t rotation_sub_track_type_ = and_not(entry_padding_mask, rotation_sub_track_types[sub_track_entry_index_].types);
				const uint32_t translation_sub_track_type_ = and_not(entry_padding_mask, translation_sub_track_types[sub_track_entry_index_].types);
				const uint32_t scale_sub_track_type_ = scale_sub_track_mask & and_not(entry_padding_mask, scale_sub_track_types[sub_track_entry_index_].types);

				num_constant_rotations += count_set_bits(rotation_sub_track_type_ & 0x55555555);
				num_animated_rotations += count_set_bits(rotation_sub_track_type_ & 0xAAAAAAAA);

				num_constant_translations += count_set_bits(translation_sub_track_type_ & 0x55555555);
				num_animated_translations += count_set_bits(translation_sub_track_type_ & 0xAAAAAAAA);

				num_constant_scales += count_set_bits(scale_sub_track_type_ & 0x55555555);
				num_animated_scales += count_set_bits(scale_sub_track_type_ & 0xAAAAAAAA);
			}

			uint32_t rotation_group_sample_index = 0;
			uint32_t translation_group_sample_index = 0;
			uint32_t scale_group_sample_index = 0;

			constant_track_cache_v0 constant_track_cache;

			// Skip the constant track data
			if ((combined_sub_track_type & 1) != 0)
			{
				// TODO: Can we init just what we need?
				constant_track_cache.initialize<decompression_settings_type>(context);

				// Calculate how many constant groups of each sub-track type we need to skip
				// Constant groups are easy to skip since they are contiguous in memory, we can just skip N trivially

				// Unpack the groups we need and skip the tracks before us
				if (rotation_sub_track_type & 1)
				{
					rotation_group_sample_index = num_constant_rotations % 4;

					const uint32_t num_rotation_constant_groups_to_skip = num_constant_rotations / 4;
					if (num_rotation_constant_groups_to_skip != 0)
						constant_track_cache.skip_rotation_groups<decompression_settings_type>(context, num_rotation_constant_groups_to_skip);
				}

				if (translation_sub_track_type & 1)
				{
					translation_group_sample_index = num_constant_translations % 4;

					const uint32_t num_translation_constant_groups_to_skip = num_constant_translations / 4;
					if (num_translation_constant_groups_to_skip != 0)
						constant_track_cache.skip_translation_groups(num_translation_constant_groups_to_skip);
				}

				if (scale_sub_track_type & 1)
				{
					scale_group_sample_index = num_constant_scales % 4;

					const uint32_t num_scale_constant_groups_to_skip = num_constant_scales / 4;
					if (num_scale_constant_groups_to_skip != 0)
						constant_track_cache.skip_scale_groups(num_scale_constant_groups_to_skip);
				}
			}

			animated_track_cache_v0 animated_track_cache;

			// Skip the animated track data
			if ((combined_sub_track_type & 2) != 0)
			{
				// TODO: Can we init just what we need?
				animated_track_cache.initialize<decompression_settings_type, translation_adapter>(context);

				if (rotation_sub_track_type & 2)
				{
					rotation_group_sample_index = num_animated_rotations % 4;

					const uint32_t num_groups_to_skip = num_animated_rotations / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_rotation_groups<decompression_settings_type>(context, num_groups_to_skip);
				}

				if (translation_sub_track_type & 2)
				{
					translation_group_sample_index = num_animated_translations % 4;

					const uint32_t num_groups_to_skip = num_animated_translations / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_translation_groups<translation_adapter>(context, num_groups_to_skip);
				}

				if (scale_sub_track_type & 2)
				{
					scale_group_sample_index = num_animated_scales % 4;

					const uint32_t num_groups_to_skip = num_animated_scales / 4;
					if (num_groups_to_skip != 0)
						animated_track_cache.skip_scale_groups<scale_adapter>(context, num_groups_to_skip);
				}
			}

			// Finally reached our desired track, unpack it

			if (rotation_sub_track_type == 0)
			{
				if (default_rotation_mode != default_sub_track_mode::skipped)
				{
					if (default_rotation_mode == default_sub_track_mode::variable)
						writer.write_rotation(track_index, writer.get_variable_default_rotation(track_index));
					else
						writer.write_rotation(track_index, default_rotation);
				}
			}
			else
			{
				rtm::quatf rotation;
				if (rotation_sub_track_type & 1)
					rotation = constant_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_sample_index);
				else
					rotation = animated_track_cache.unpack_rotation_within_group<decompression_settings_type>(context, rotation_group_sample_index);

				writer.write_rotation(track_index, rotation);
			}

			if (translation_sub_track_type == 0)
			{
				if (default_translation_mode != default_sub_track_mode::skipped)
				{
					if (default_translation_mode == default_sub_track_mode::variable)
						writer.write_translation(track_index, writer.get_variable_default_translation(track_index));
					else
						writer.write_translation(track_index, default_translation);
				}
			}
			else
			{
				rtm::vector4f translation;
				if (translation_sub_track_type & 1)
					translation = constant_track_cache.unpack_translation_within_group(translation_group_sample_index);
				else
					translation = animated_track_cache.unpack_translation_within_group<translation_adapter>(context, translation_group_sample_index);

				writer.write_translation(track_index, translation);
			}

			if (scale_sub_track_type == 0)
			{
				if (default_scale_mode != default_sub_track_mode::skipped)
				{
					if (default_scale_mode == default_sub_track_mode::variable)
						writer.write_scale(track_index, writer.get_variable_default_scale(track_index));
					else
						writer.write_scale(track_index, default_scale);
				}
			}
			else
			{
				rtm::vector4f scale;
				if (scale_sub_track_type & 1)
					scale = constant_track_cache.unpack_scale_within_group(scale_group_sample_index);
				else
					scale = animated_track_cache.unpack_scale_within_group<scale_adapter>(context, scale_group_sample_index);

				writer.write_scale(track_index, scale);
			}

			if (decompression_settings_type::disable_fp_exeptions())
				restore_fp_exceptions(fp_env);
		}

		// Restore our warnings
#if defined(RTM_COMPILER_MSVC)
		#pragma warning(pop)
#elif defined(RTM_COMPILER_GCC)
		#pragma GCC diagnostic pop
#endif
	}

	ACL_IMPL_VERSION_NAMESPACE_END
}

#if defined(RTM_COMPILER_MSVC)
	#pragma warning(pop)
#endif

ACL_IMPL_FILE_PRAGMA_POP
