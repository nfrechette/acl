#pragma once

////////////////////////////////////////////////////////////////////////////////
// The MIT License (MIT)
//
// Copyright (c) 2017 Nicholas Frechette & Animation Compression Library contributors
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

namespace acl
{
	struct SJSONParserError
	{
		SJSONParserError()
			: error(SJSONParserError::None)
			, line()
			, column()
		{
		}

		enum : uint32_t
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
			UnexpectedContentAtEnd,

			Last
		};

		uint32_t error;
		uint32_t line;
		uint32_t column;

		virtual const char* get_description() const
		{
			switch (error)
			{
			case None:
				return "None";
			case InputTruncated:
				return "The file ended sooner than expected";
			case OpeningBraceExpected:
				return "An opening { is expected here";
			case ClosingBraceExpected:
				return "A closing } is expected here";
			case EqualSignExpected:
				return "An equal sign is expected here";
			case OpeningBracketExpected:
				return "An opening [ is expected here";
			case ClosingBracketExpected:
				return "A closing ] is expected here";
			case CommaExpected:
				return "A comma is expected here";
			case CommentBeginsIncorrectly:
				return "Comments must start with either // or /*";
			case CannotUseQuotationMarkInUnquotedString:
				return "Quotation marks cannot be used in unquoted strings";
			case KeyExpected:
				return "The name of a key is expected here";
			case IncorrectKey:
				return "A different key is expected here";
			case TrueOrFalseExpected:
				return "Only the literals true or false are allowed here";
			case QuotationMarkExpected:
				return "A quotation mark is expected here";
			case NumberExpected:
				return "A number is expected here";
			case NumberIsTooLong:
				return "The number is too long; increase SJSONParser::MAX_NUMBER_LENGTH";
			case InvalidNumber:
				return "This number has an invalid format";
			case NumberCouldNotBeConverted:
				return "This number could not be converted";
			case UnexpectedContentAtEnd:
				return "There should not be any more content in this file";
			default:
				return "Unknown error";
			}
		}
	};
}
