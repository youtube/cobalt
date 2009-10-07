// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/third_party/icu/icu_utf.h"

namespace {

inline bool IsValidCodepoint(uint32 code_point) {
  // Excludes the surrogate code points ([0xD800, 0xDFFF]) and
  // codepoints larger than 0x10FFFF (the highest codepoint allowed).
  // Non-characters and unassigned codepoints are allowed.
  return code_point < 0xD800u ||
         (code_point >= 0xE000u && code_point <= 0x10FFFFu);
}

// ReadUnicodeCharacter --------------------------------------------------------

// Reads a UTF-8 stream, placing the next code point into the given output
// |*code_point|. |src| represents the entire string to read, and |*char_index|
// is the character offset within the string to start reading at. |*char_index|
// will be updated to index the last character read, such that incrementing it
// (as in a for loop) will take the reader to the next character.
//
// Returns true on success. On false, |*code_point| will be invalid.
bool ReadUnicodeCharacter(const char* src, int32 src_len,
                          int32* char_index, uint32* code_point_out) {
  // U8_NEXT expects to be able to use -1 to signal an error, so we must
  // use a signed type for code_point.  But this function returns false
  // on error anyway, so code_point_out is unsigned.
  int32 code_point;
  CBU8_NEXT(src, *char_index, src_len, code_point);
  *code_point_out = static_cast<uint32>(code_point);

  // The ICU macro above moves to the next char, we want to point to the last
  // char consumed.
  (*char_index)--;

  // Validate the decoded value.
  return IsValidCodepoint(code_point);
}

// Reads a UTF-16 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const char16* src, int32 src_len,
                          int32* char_index, uint32* code_point) {
  if (CBU16_IS_SURROGATE(src[*char_index])) {
    if (!CBU16_IS_SURROGATE_LEAD(src[*char_index]) ||
        *char_index + 1 >= src_len ||
        !CBU16_IS_TRAIL(src[*char_index + 1])) {
      // Invalid surrogate pair.
      return false;
    }

    // Valid surrogate pair.
    *code_point = CBU16_GET_SUPPLEMENTARY(src[*char_index],
                                          src[*char_index + 1]);
    (*char_index)++;
  } else {
    // Not a surrogate, just one 16-bit word.
    *code_point = src[*char_index];
  }

  return IsValidCodepoint(*code_point);
}

#if defined(WCHAR_T_IS_UTF32)
// Reads UTF-32 character. The usage is the same as the 8-bit version above.
bool ReadUnicodeCharacter(const wchar_t* src, int32 src_len,
                          int32* char_index, uint32* code_point) {
  // Conversion is easy since the source is 32-bit.
  *code_point = src[*char_index];

  // Validate the value.
  return IsValidCodepoint(*code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// WriteUnicodeCharacter -------------------------------------------------------

// Appends a UTF-8 character to the given 8-bit string.
void WriteUnicodeCharacter(uint32 code_point, std::string* output) {
  if (code_point <= 0x7f) {
    // Fast path the common case of one byte.
    output->push_back(code_point);
    return;
  }

  // U8_APPEND_UNSAFE can append up to 4 bytes.
  int32 char_offset = static_cast<int32>(output->length());
  output->resize(char_offset + CBU8_MAX_LENGTH);

  CBU8_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);

  // U8_APPEND_UNSAFE will advance our pointer past the inserted character, so
  // it will represent the new length of the string.
  output->resize(char_offset);
}

// Appends the given code point as a UTF-16 character to the STL string.
void WriteUnicodeCharacter(uint32 code_point, string16* output) {
  if (CBU16_LENGTH(code_point) == 1) {
    // Thie code point is in the Basic Multilingual Plane (BMP).
    output->push_back(static_cast<char16>(code_point));
  } else {
    // Non-BMP characters use a double-character encoding.
    int32 char_offset = static_cast<int32>(output->length());
    output->resize(char_offset + CBU16_MAX_LENGTH);
    CBU16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);
  }
}

#if defined(WCHAR_T_IS_UTF32)
// Appends the given UTF-32 character to the given 32-bit string.
inline void WriteUnicodeCharacter(uint32 code_point, std::wstring* output) {
  // This is the easy case, just append the character.
  output->push_back(code_point);
}
#endif  // defined(WCHAR_T_IS_UTF32)

// Generalized Unicode converter -----------------------------------------------

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SRC_CHAR, typename DEST_STRING>
bool ConvertUnicode(const SRC_CHAR* src, size_t src_len, DEST_STRING* output) {
  output->clear();

  // ICU requires 32-bit numbers.
  bool success = true;
  int32 src_len32 = static_cast<int32>(src_len);
  for (int32 i = 0; i < src_len32; i++) {
    uint32 code_point;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point)) {
      WriteUnicodeCharacter(code_point, output);
    } else {
      // TODO(jungshik): consider adding 'Replacement character' (U+FFFD)
      // in place of an invalid codepoint.
      success = false;
    }
  }
  return success;
}

// Guesses the length of the output in UTF-8 in bytes, and reserves that amount
// of space in the given string. We also assume that the input character types
// are unsigned, which will be true for UTF-16 and -32 on our systems. We assume
// the string length is greater than zero.
template<typename CHAR>
void ReserveUTF8Output(const CHAR* src, size_t src_len, std::string* output) {
  if (src[0] < 0x80) {
    // Assume that the entire input will be ASCII.
    output->reserve(src_len);
  } else {
    // Assume that the entire input is non-ASCII and will have 3 bytes per char.
    output->reserve(src_len * 3);
  }
}

// Guesses the size of the output buffer (containing either UTF-16 or -32 data)
// given some UTF-8 input that will be converted to it. See ReserveUTF8Output.
// We assume the source length is > 0.
template<typename STRING>
void ReserveUTF16Or32Output(const char* src, size_t src_len, STRING* output) {
  if (static_cast<unsigned char>(src[0]) < 0x80) {
    // Assume the input is all ASCII, which means 1:1 correspondence.
    output->reserve(src_len);
  } else {
    // Otherwise assume that the UTF-8 sequences will have 2 bytes for each
    // character.
    output->reserve(src_len / 2);
  }
}

}  // namespace

// UTF-8 <-> Wide --------------------------------------------------------------

std::string WideToUTF8(const std::wstring& wide) {
  std::string ret;
  if (wide.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUTF8(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<wchar_t, std::string>(src, src_len, output);
}

std::wstring UTF8ToWide(const base::StringPiece& utf8) {
  std::wstring ret;
  if (utf8.empty())
    return ret;

  UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, std::wstring>(src, src_len, output);
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF16)

// When wide == UTF-16, then conversions are a NOP.
string16 WideToUTF16(const std::wstring& wide) {
  return wide;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  output->assign(src, src_len);
  return true;
}

std::wstring UTF16ToWide(const string16& utf16) {
  return utf16;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  output->assign(src, src_len);
  return true;
}

#elif defined(WCHAR_T_IS_UTF32)

string16 WideToUTF16(const std::wstring& wide) {
  string16 ret;
  if (wide.empty())
    return ret;

  WideToUTF16(wide.data(), wide.length(), &ret);
  return ret;
}

bool WideToUTF16(const wchar_t* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<wchar_t, string16>(src, src_len, output);
}

std::wstring UTF16ToWide(const string16& utf16) {
  std::wstring ret;
  if (utf16.empty())
    return ret;

  UTF16ToWide(utf16.data(), utf16.length(), &ret);
  return ret;
}

bool UTF16ToWide(const char16* src, size_t src_len, std::wstring* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  // Assume that normally we won't have any non-BMP characters so the counts
  // will be the same.
  output->reserve(src_len);
  return ConvertUnicode<char16, std::wstring>(src, src_len, output);
}

#endif  // defined(WCHAR_T_IS_UTF32)

// UTF16 <-> UTF8 --------------------------------------------------------------

#if defined(WCHAR_T_IS_UTF32)

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF16Or32Output(src, src_len, output);
  return ConvertUnicode<char, string16>(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  string16 ret;
  if (utf8.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF8ToUTF16(utf8.data(), utf8.length(), &ret);
  return ret;
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  if (src_len == 0) {
    output->clear();
    return true;
  }

  ReserveUTF8Output(src, src_len, output);
  return ConvertUnicode<char16, std::string>(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  std::string ret;
  if (utf16.empty())
    return ret;

  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF16ToUTF8(utf16.data(), utf16.length(), &ret);
  return ret;
}

#elif defined(WCHAR_T_IS_UTF16)
// Easy case since we can use the "wide" versions we already wrote above.

bool UTF8ToUTF16(const char* src, size_t src_len, string16* output) {
  return UTF8ToWide(src, src_len, output);
}

string16 UTF8ToUTF16(const std::string& utf8) {
  return UTF8ToWide(utf8);
}

bool UTF16ToUTF8(const char16* src, size_t src_len, std::string* output) {
  return WideToUTF8(src, src_len, output);
}

std::string UTF16ToUTF8(const string16& utf16) {
  return WideToUTF8(utf16);
}

#endif
