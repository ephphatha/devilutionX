#pragma once

#include <algorithm>
#include <charconv>
#include <string_view>
#include <system_error>

#include <expected.hpp>

namespace devilution {

enum class ParseIntError {
	ParseError = 1,
	OutOfRange
};

template <typename IntT>
using ParseIntResult = tl::expected<IntT, ParseIntError>;

template <typename IntT>
ParseIntResult<IntT> ParseInt(
    std::string_view str, IntT min = std::numeric_limits<IntT>::min(),
    IntT max = std::numeric_limits<IntT>::max(), const char **endOfParse = nullptr)
{
	IntT value;
	const std::from_chars_result result = std::from_chars(str.data(), str.data() + str.size(), value);
	if (endOfParse != nullptr) {
		*endOfParse = result.ptr;
	}
	if (result.ec == std::errc::invalid_argument)
		return tl::unexpected(ParseIntError::ParseError);
	if (result.ec == std::errc::result_out_of_range || value < min || value > max)
		return tl::unexpected(ParseIntError::OutOfRange);
	if (result.ec != std::errc())
		return tl::unexpected(ParseIntError::ParseError);
	return value;
}

inline uint8_t ParseFixed6Fraction(std::string_view str, const char **endOfParse = nullptr)
{
	unsigned numDigits = 0;
	uint32_t decimalFraction = 0;

	// Read at most 7 digits, at that threshold we're able to determine an exact rounding for 6 bit fixed point numbers
	while (!str.empty() && numDigits < 7) {
		if (str[0] < '0' || str[0] > '9') {
			break;
		}
		decimalFraction = decimalFraction * 10 + str[0] - '0';
		++numDigits;
		str.remove_prefix(1);
	}
	if (endOfParse != nullptr) {
		// to mimic the behaviour of std::from_chars consume all remaining digits in case the value was overly precise.
		*endOfParse = std::find_if_not(str.data(), str.data() + str.size(), [](char character) { return character >= '0' && character <= '9'; });
	}
	// to ensure rounding to nearest we normalise all values to 7 decimal places
	if (numDigits < 7) {
		decimalFraction *= std::pow(10U, 7U - numDigits);
	}
	// we add half the step between representable values to use integer truncation as a substitute for rounding to nearest.
	return (decimalFraction + 78125) / 156250;
}

} // namespace devilution
