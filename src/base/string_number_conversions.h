// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING_NUMBER_CONVERSIONS_H_
#define BASE_STRING_NUMBER_CONVERSIONS_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/string_piece.h"
#include "base/string16.h"

// ----------------------------------------------------------------------------
// IMPORTANT MESSAGE FROM YOUR SPONSOR
//
// This file contains no "wstring" variants. New code should use string16. If
// you need to make old code work, use the UTF8 version and convert. Please do
// not add wstring variants.
//
// Please do not add "convenience" functions for converting strings to integers
// that return the value and ignore success/failure. That encourages people to
// write code that doesn't properly handle the error conditions.
// ----------------------------------------------------------------------------

namespace base {

// The following macro expects a macro functor accepting two parameters: a name
// and a C++ type.  It is used to generate the declarations, definitions and
// unit tests for types listed below.
#define INTEGRAL_STRING_CONVERSIONS_FOR_EACH(MacroOp) \
  MacroOp(Int, int)                                   \
  MacroOp(Uint, unsigned int)                         \
  MacroOp(Int8, int8)                                 \
  MacroOp(Uint8, uint8)                               \
  MacroOp(Int16, int16)                               \
  MacroOp(Uint16, uint16)                             \
  MacroOp(Int32, int32)                               \
  MacroOp(Uint32, uint32)                             \
  MacroOp(Int64, int64)                               \
  MacroOp(Uint64, uint64)                             \
  MacroOp(SizeT, size_t)

// Number -> string conversions ------------------------------------------------
#define DECLARE_INTEGRAL_TO_STRING_CONVERSIONS(Name, CppType) \
  BASE_EXPORT std::string Name##ToString(CppType value);      \
  BASE_EXPORT string16 Name##ToString16(CppType value);

INTEGRAL_STRING_CONVERSIONS_FOR_EACH(DECLARE_INTEGRAL_TO_STRING_CONVERSIONS)
#undef DECLARE_INTEGRAL_TO_STRING_CONVERSIONS

// DoubleToString converts the double to a string format that ignores the
// locale. If you want to use locale specific formatting, use ICU.
BASE_EXPORT std::string DoubleToString(double value);

// String -> number conversions ------------------------------------------------

// Perform a best-effort conversion of the input string to a numeric type,
// setting |*output| to the result of the conversion.  Returns true for
// "perfect" conversions; returns false in the following cases:
//  - Overflow/underflow.  |*output| will be set to the maximum value supported
//    by the data type.
//  - Trailing characters in the string after parsing the number.  |*output|
//    will be set to the value of the number that was parsed.
//  - Leading whitespace in the string before parsing the number. |*output| will
//    be set to the value of the number that was parsed.
//  - No characters parseable as a number at the beginning of the string.
//    |*output| will be set to 0.
//  - Empty string.  |*output| will be set to 0.
#define DECLARE_STRING_TO_INTEGRAL_CONVERSIONS(Name, CppType)                 \
  BASE_EXPORT bool StringTo##Name(const StringPiece& input, CppType* output); \
  BASE_EXPORT bool StringTo##Name(const StringPiece16& input, CppType* output);

INTEGRAL_STRING_CONVERSIONS_FOR_EACH(DECLARE_STRING_TO_INTEGRAL_CONVERSIONS)
#undef DECLARE_STRING_TO_INTEGRAL_CONVERSIONS

// For floating-point conversions, only conversions of input strings in decimal
// form are defined to work.  Behavior with strings representing floating-point
// numbers in hexadecimal, and strings representing non-fininte values (such as
// NaN and inf) is undefined.  Otherwise, these behave the same as the integral
// variants.  This expects the input string to NOT be specific to the locale.
// If your input is locale specific, use ICU to read the number.
BASE_EXPORT bool StringToDouble(const std::string& input, double* output);

// Hex encoding ----------------------------------------------------------------

// Returns a hex string representation of a binary buffer. The returned hex
// string will be in upper case. This function does not check if |size| is
// within reasonable limits since it's written with trusted data in mind.  If
// you suspect that the data you want to format might be large, the absolute
// max size for |size| should be is
//   std::numeric_limits<size_t>::max() / 2
BASE_EXPORT std::string HexEncode(const void* bytes, size_t size);

// Best effort conversion, see StringToInt above for restrictions.
BASE_EXPORT bool HexStringToInt(const StringPiece& input, int* output);
BASE_EXPORT bool HexStringToUInt(const StringPiece& input,
                                 unsigned int* output);

// Similar to the previous functions, except that output is a vector of bytes.
// |*output| will contain as many bytes as were successfully parsed prior to the
// error.  There is no overflow, but input.size() must be evenly divisible by 2.
// Leading 0x or +/- are not allowed.
BASE_EXPORT bool HexStringToBytes(const std::string& input,
                                  std::vector<uint8>* output);

}  // namespace base

#endif  // BASE_STRING_NUMBER_CONVERSIONS_H_

