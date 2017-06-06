#pragma once

#include "acl/sjson/sjson_parser_error.h"

namespace acl
{
	struct ClipReaderError : SJSONParserError
	{
		enum
		{
			UnsupportedVersion = SJSONParserError::Last,
			InputDidNotEnd,
			NoParentWithThatName,
			NoBoneWithThatName,
		};

		ClipReaderError()
		{
			error = SJSONParserError::None;
			line = -1;
			column = -1;
		}

		ClipReaderError(const SJSONParserError &e)
		{
			error = e.error;
			line = e.line;
			column = e.column;
		}
	};
}