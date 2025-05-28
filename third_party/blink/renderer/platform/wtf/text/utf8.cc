/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

#include <unicode/utf16.h>

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hasher.h"

namespace WTF {
namespace unicode {

namespace {

inline int InlineUTF8SequenceLengthNonASCII(uint8_t b0) {
  if ((b0 & 0xC0) != 0xC0)
    return 0;
  if ((b0 & 0xE0) == 0xC0)
    return 2;
  if ((b0 & 0xF0) == 0xE0)
    return 3;
  if ((b0 & 0xF8) == 0xF0)
    return 4;
  return 0;
}

inline int InlineUTF8SequenceLength(uint8_t b0) {
  return IsASCII(b0) ? 1 : InlineUTF8SequenceLengthNonASCII(b0);
}

// Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
// into the first byte, depending on how many bytes follow.  There are
// as many entries in this table as there are UTF-8 sequence types.
// (I.e., one byte sequence, two byte... etc.). Remember that sequences
// for *legal* UTF-8 will be 4 or fewer bytes total.
const unsigned char kFirstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0,
                                         0xF0, 0xF8, 0xFC};

ConversionStatus ConvertLatin1ToUTF8(const LChar** source_start,
                                     const LChar* source_end,
                                     char** target_start,
                                     char* target_end) {
  ConversionStatus status = kConversionOK;
  const LChar* source = *source_start;
  char* target = *target_start;
  while (source < source_end) {
    UChar32 ch;
    uint8_t bytes_to_write = 0;
    const UChar32 kByteMask = 0xBF;
    const UChar32 kByteMark = 0x80;
    const LChar* old_source =
        source;  // In case we have to back up because of target overflow.
    ch = static_cast<UChar32>(*source++);

    // Figure out how many bytes the result will require
    if (ch < (UChar32)0x80)
      bytes_to_write = 1;
    else
      bytes_to_write = 2;

    target += bytes_to_write;
    if (target > target_end) {
      source = old_source;  // Back up source pointer!
      target -= bytes_to_write;
      status = kTargetExhausted;
      break;
    }
    switch (bytes_to_write) {
      case 2:
        *--target = (char)((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 1:
        *--target = (char)(ch | kFirstByteMark[bytes_to_write]);
    }
    target += bytes_to_write;
  }
  *source_start = source;
  *target_start = target;
  return status;
}

ConversionStatus ConvertUTF16ToUTF8(const UChar** source_start,
                                    const UChar* source_end,
                                    char** target_start,
                                    char* target_end,
                                    bool strict) {
  ConversionStatus status = kConversionOK;
  const UChar* source = *source_start;
  char* target = *target_start;
  while (source < source_end) {
    UChar32 ch;
    uint8_t bytes_to_write = 0;
    const UChar32 kByteMask = 0xBF;
    const UChar32 kByteMark = 0x80;
    const UChar* old_source =
        source;  // In case we have to back up because of target overflow.
    ch = static_cast<UChar32>(*source++);
    // If we have a surrogate pair, convert to UChar32 first.
    if (ch >= 0xD800 && ch <= 0xDBFF) {
      // If the 16 bits following the high surrogate are in the source buffer...
      if (source < source_end) {
        UChar32 ch2 = static_cast<UChar32>(*source);
        // If it's a low surrogate, convert to UChar32.
        if (ch2 >= 0xDC00 && ch2 <= 0xDFFF) {
          ch = ((ch - 0xD800) << 10) + (ch2 - 0xDC00) + 0x0010000;
          ++source;
        } else if (strict) {  // it's an unpaired high surrogate
          --source;           // return to the illegal value itself
          status = kSourceIllegal;
          break;
        }
      } else {     // We don't have the 16 bits following the high surrogate.
        --source;  // return to the high surrogate
        status = kSourceExhausted;
        break;
      }
    } else if (strict) {
      // UTF-16 surrogate values are illegal in UTF-32
      if (ch >= 0xDC00 && ch <= 0xDFFF) {
        --source;  // return to the illegal value itself
        status = kSourceIllegal;
        break;
      }
    }
    // Figure out how many bytes the result will require
    if (ch < (UChar32)0x80) {
      bytes_to_write = 1;
    } else if (ch < (UChar32)0x800) {
      bytes_to_write = 2;
    } else if (ch < (UChar32)0x10000) {
      bytes_to_write = 3;
    } else if (ch < (UChar32)0x110000) {
      bytes_to_write = 4;
    } else {
      // TODO(crbug.com/329702346): Surrogate pairs cannot represent codepoints
      // higher than 0x10FFFF, so this should not be reachable.
      NOTREACHED(base::NotFatalUntil::M134);
      bytes_to_write = 3;
      ch = kReplacementCharacter;
    }

    target += bytes_to_write;
    if (target > target_end) {
      source = old_source;  // Back up source pointer!
      target -= bytes_to_write;
      status = kTargetExhausted;
      break;
    }
    switch (bytes_to_write) {
      case 4:
        *--target = (char)((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 3:
        *--target = (char)((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 2:
        *--target = (char)((ch | kByteMark) & kByteMask);
        ch >>= 6;
        [[fallthrough]];
      case 1:
        *--target = (char)(ch | kFirstByteMark[bytes_to_write]);
    }
    target += bytes_to_write;
  }
  *source_start = source;
  *target_start = target;
  return status;
}

// This must be called with the length pre-determined by the first byte.
// If presented with a length > 4, this returns false.  The Unicode
// definition of UTF-8 goes up to 4-byte sequences.
bool IsLegalUTF8(const unsigned char* source, int length) {
  unsigned char a;
  const unsigned char* srcptr = source + length;
  switch (length) {
    default:
      return false;
    case 4:
      if ((a = (*--srcptr)) < 0x80 || a > 0xBF)
        return false;
      [[fallthrough]];
    case 3:
      if ((a = (*--srcptr)) < 0x80 || a > 0xBF)
        return false;
      [[fallthrough]];
    case 2:
      if ((a = (*--srcptr)) > 0xBF)
        return false;

      // no fall-through in this inner switch
      switch (*source) {
        case 0xE0:
          if (a < 0xA0)
            return false;
          break;
        case 0xED:
          if (a > 0x9F)
            return false;
          break;
        case 0xF0:
          if (a < 0x90)
            return false;
          break;
        case 0xF4:
          if (a > 0x8F)
            return false;
          break;
        default:
          if (a < 0x80)
            return false;
      }
      [[fallthrough]];

    case 1:
      if (*source >= 0x80 && *source < 0xC2)
        return false;
  }
  if (*source > 0xF4)
    return false;
  return true;
}

// Magic values subtracted from a buffer value during UTF8 conversion.
// This table contains as many values as there might be trailing bytes
// in a UTF-8 sequence.
const UChar32 kOffsetsFromUTF8[6] = {0x00000000UL,
                                     0x00003080UL,
                                     0x000E2080UL,
                                     0x03C82080UL,
                                     static_cast<UChar32>(0xFA082080UL),
                                     static_cast<UChar32>(0x82082080UL)};

inline UChar32 ReadUTF8Sequence(const uint8_t*& sequence, unsigned length) {
  UChar32 character = 0;

  switch (length) {
    case 6:
      character += *sequence++;
      character <<= 6;
      [[fallthrough]];
    case 5:
      character += *sequence++;
      character <<= 6;
      [[fallthrough]];
    case 4:
      character += *sequence++;
      character <<= 6;
      [[fallthrough]];
    case 3:
      character += *sequence++;
      character <<= 6;
      [[fallthrough]];
    case 2:
      character += *sequence++;
      character <<= 6;
      [[fallthrough]];
    case 1:
      character += *sequence++;
  }

  return character - kOffsetsFromUTF8[length - 1];
}

ConversionStatus ConvertUTF8ToUTF16(const uint8_t** source_start,
                                    const uint8_t* source_end,
                                    UChar** target_start,
                                    UChar* target_end,
                                    bool strict) {
  ConversionStatus status = kConversionOK;
  const uint8_t* source = *source_start;
  UChar* target = *target_start;
  while (source < source_end) {
    int utf8_sequence_length = InlineUTF8SequenceLength(*source);
    if (source_end - source < utf8_sequence_length) {
      status = kSourceExhausted;
      break;
    }
    // Do this check whether lenient or strict
    if (!IsLegalUTF8(source, utf8_sequence_length)) {
      status = kSourceIllegal;
      break;
    }

    UChar32 character = ReadUTF8Sequence(source, utf8_sequence_length);

    if (target >= target_end) {
      source -= utf8_sequence_length;  // Back up source pointer!
      status = kTargetExhausted;
      break;
    }

    if (U_IS_BMP(character)) {
      // UTF-16 surrogate values are illegal in UTF-32
      if (U_IS_SURROGATE(character)) {
        if (strict) {
          source -= utf8_sequence_length;  // return to the illegal value itself
          status = kSourceIllegal;
          break;
        }
        *target++ = kReplacementCharacter;
      } else {
        *target++ = static_cast<UChar>(character);  // normal case
      }
    } else if (U_IS_SUPPLEMENTARY(character)) {
      // target is a character in range 0xFFFF - 0x10FFFF
      if (target + 1 >= target_end) {
        source -= utf8_sequence_length;  // Back up source pointer!
        status = kTargetExhausted;
        break;
      }
      *target++ = U16_LEAD(character);
      *target++ = U16_TRAIL(character);
    } else {
      // TODO(crbug.com/329702346): This should never happen;
      // InlineUTF8SequenceLength() can never return a value higher than 4, and
      // a 4-byte UTF-8 sequence can never encode anything higher than 0x10FFFF.
      NOTREACHED(base::NotFatalUntil::M134);
      if (strict) {
        source -= utf8_sequence_length;  // return to the start
        status = kSourceIllegal;
        break;  // Bail out; shouldn't continue
      } else {
        *target++ = kReplacementCharacter;
      }
    }
  }
  *source_start = source;
  *target_start = target;

  return status;
}

}  // namespace

ConversionResult<uint8_t> ConvertLatin1ToUTF8(base::span<const LChar> source,
                                              base::span<uint8_t> target) {
  const LChar* source_start = source.data();
  auto target_chars = base::as_writable_chars(target);
  char* target_start = target_chars.data();
  auto status =
      ConvertLatin1ToUTF8(&source_start, source_start + source.size(),
                          &target_start, target_start + target_chars.size());
  return {
      target.first(static_cast<size_t>(target_start - target_chars.data())),
      static_cast<size_t>(source_start - source.data()),
      status,
  };
}

ConversionResult<uint8_t> ConvertUTF16ToUTF8(base::span<const UChar> source,
                                             base::span<uint8_t> target,
                                             bool strict) {
  const UChar* source_start = source.data();
  auto target_chars = base::as_writable_chars(target);
  char* target_start = target_chars.data();
  auto status = ConvertUTF16ToUTF8(&source_start, source_start + source.size(),
                                   &target_start,
                                   target_start + target_chars.size(), strict);
  return {
      target.first(static_cast<size_t>(target_start - target_chars.data())),
      static_cast<size_t>(source_start - source.data()),
      status,
  };
}

ConversionResult<UChar> ConvertUTF8ToUTF16(base::span<const uint8_t> source,
                                           base::span<UChar> target,
                                           bool strict) {
  const uint8_t* source_start = source.data();
  UChar* target_start = target.data();
  auto status =
      ConvertUTF8ToUTF16(&source_start, source_start + source.size(),
                         &target_start, target_start + target.size(), strict);
  return {
      target.first(static_cast<size_t>(target_start - target.data())),
      static_cast<size_t>(source_start - source.data()),
      status,
  };
}

unsigned CalculateStringLengthFromUTF8(const uint8_t* data,
                                       const uint8_t*& data_end,
                                       bool& seen_non_ascii,
                                       bool& seen_non_latin1) {
  seen_non_ascii = false;
  seen_non_latin1 = false;
  if (!data)
    return 0;

  unsigned utf16_length = 0;

  while (data < data_end || (!data_end && *data)) {
    if (IsASCII(*data)) {
      ++data;
      utf16_length++;
      continue;
    }

    seen_non_ascii = true;
    int utf8_sequence_length = InlineUTF8SequenceLengthNonASCII(*data);

    if (!data_end) {
      for (int i = 1; i < utf8_sequence_length; ++i) {
        if (!data[i])
          return 0;
      }
    } else if (data_end - data < utf8_sequence_length) {
      return 0;
    }

    if (!IsLegalUTF8(data, utf8_sequence_length)) {
      return 0;
    }

    UChar32 character = ReadUTF8Sequence(data, utf8_sequence_length);
    DCHECK(!IsASCII(character));

    if (character > 0xff) {
      seen_non_latin1 = true;
    }

    if (U_IS_BMP(character)) {
      // UTF-16 surrogate values are illegal in UTF-32
      if (U_IS_SURROGATE(character))
        return 0;
      utf16_length++;
    } else if (U_IS_SUPPLEMENTARY(character)) {
      utf16_length += 2;
    } else {
      return 0;
    }
  }

  data_end = data;
  return utf16_length;
}

}  // namespace unicode
}  // namespace WTF
