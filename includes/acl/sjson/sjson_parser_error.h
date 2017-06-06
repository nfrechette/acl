#pragma once

namespace acl
{
	struct SJSONParserError
	{
		enum
		{
			None,
			InputTruncated,
			OpeningBraceExpected,
			ClosingBraceExpected,
			EqualSignExpected,
			OpeningBracketExpected,
			ClosingBracketExpected,
			CommaExpected,
			CommentBeginsIncorrectly,
			CannotUseQuotationMarkInUnquotedString,
			KeyExpected,
			IncorrectKey,
			TrueOrFalseExpected,
			QuotationMarkExpected,
			NumberExpected,
			NumberIsTooLong,
			InvalidNumber,
			NumberCouldNotBeConverted,
			IntegerExpected,
			UnexpectedContentAtEnd,

			Last
		};

		int error;
		int line;
		int column;
	};
}