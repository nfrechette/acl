#pragma once

#include <stdint.h>

namespace acl
{
	// Misc globals
	static constexpr uint32_t COMPRESSED_CLIP_TAG					= 0xac10ac10;

	// Algorithm version numbers
	static constexpr uint16_t FULL_PRECISION_ALGORITHM_VERSION		= 0;

	enum class AlgorithmType : uint16_t
	{
		FullPrecision			= 0,
	};

	//////////////////////////////////////////////////////////////////////////

	// TODO: constexpr
	inline uint16_t get_algorithm_version(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::FullPrecision:	return FULL_PRECISION_ALGORITHM_VERSION;
			default:							return 0xFFFF;
		}
	}

	// TODO: constexpr
	inline bool is_valid_algorithm_type(AlgorithmType type)
	{
		switch (type)
		{
			case AlgorithmType::FullPrecision:	return true;
			default:							return false;
		}
	}
}
