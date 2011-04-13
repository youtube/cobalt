// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/escape.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/utf_offset_string_conversions.h"

namespace {

static const char* const kHexString = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK(i >= 0 && i <= 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
class Charmap {
 public:
  Charmap(uint32 b0, uint32 b1, uint32 b2, uint32 b3,
          uint32 b4, uint32 b5, uint32 b6, uint32 b7) {
    map_[0] = b0; map_[1] = b1; map_[2] = b2; map_[3] = b3;
    map_[4] = b4; map_[5] = b5; map_[6] = b6; map_[7] = b7;
  }

  bool Contains(unsigned char c) const {
    return (map_[c >> 5] & (1 << (c & 31))) ? true : false;
  }

 private:
  uint32 map_[8];
};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20.
std::string Escape(const std::string& text, const Charmap& charmap,
                   bool use_plus) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

// Contains nonzero when the corresponding character is unescapable for normal
// URLs. These characters are the ones that may change the parsing of a URL, so
// we don't want to unescape them sometimes. In many case we won't want to
// unescape spaces, but that is controlled by parameters to Unescape*.
//
// The basic rule is that we can't unescape anything that would changing parsing
// like # or ?. We also can't unescape &, =, or + since that could be part of a
// query and that could change the server's parsing of the query. Nor can we
// unescape \ since googleurl will convert it to a /.
//
// Lastly, we can't unescape anything that doesn't have a canonical
// representation in a URL. This means that unescaping will change the URL, and
// you could get different behavior if you copy and paste the URL, or press
// enter in the URL bar. The list of characters that fall into this category
// are the ones labeled PASS (allow either escaped or unescaped) in the big
// lookup table at the top of googleurl/src/url_canon_path.cc
const char kUrlUnescape[128] = {
//   NULL, control chars...
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//  ' ' !  "  #  $  %  &  '  (  )  *  +  ,  -  .  /
     0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,
//   0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 0, 1, 0,
//   @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
     0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
//   `  a  b  c  d  e  f  g  h  i  j  k  l  m  n  o
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//   p  q  r  s  t  u  v  w  x  y  z  {  |  }  ~  <NBSP>
     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
};

template<typename STR>
STR UnescapeURLWithOffsetsImpl(const STR& escaped_text,
                               UnescapeRule::Type rules,
                               std::vector<size_t>* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<std::wstring>(escaped_text.length()));
  }
  // Do not unescape anything, return the |escaped_text| text.
  if (rules == UnescapeRule::NONE)
    return escaped_text;

  // The output of the unescaping is always smaller than the input, so we can
  // reserve the input size to make sure we have enough buffer and don't have
  // to allocate in the loop below.
  STR result;
  result.reserve(escaped_text.length());

  AdjustEncodingOffset::Adjustments adjustments;  // Locations of adjusted text.
  for (size_t i = 0, max = escaped_text.size(); i < max; ++i) {
    if (static_cast<unsigned char>(escaped_text[i]) >= 128) {
      // Non ASCII character, append as is.
      result.push_back(escaped_text[i]);
      continue;
    }

    char current_char = static_cast<char>(escaped_text[i]);
    if (current_char == '%' && i + 2 < max) {
      const typename STR::value_type most_sig_digit(
          static_cast<typename STR::value_type>(escaped_text[i + 1]));
      const typename STR::value_type least_sig_digit(
          static_cast<typename STR::value_type>(escaped_text[i + 2]));
      if (IsHexDigit(most_sig_digit) && IsHexDigit(least_sig_digit)) {
        unsigned char value = HexDigitToInt(most_sig_digit) * 16 +
            HexDigitToInt(least_sig_digit);
        if (value >= 0x80 ||  // Unescape all high-bit characters.
            // For 7-bit characters, the lookup table tells us all valid chars.
            (kUrlUnescape[value] ||
             // ...and we allow some additional unescaping when flags are set.
             (value == ' ' && (rules & UnescapeRule::SPACES)) ||
             // Allow any of the prohibited but non-control characters when
             // we're doing "special" chars.
             (value > ' ' && (rules & UnescapeRule::URL_SPECIAL_CHARS)) ||
             // Additionally allow control characters if requested.
             (value < ' ' && (rules & UnescapeRule::CONTROL_CHARS)))) {
          // Use the unescaped version of the character.
          adjustments.push_back(i);
          result.push_back(value);
          i += 2;
        } else {
          // Keep escaped. Append a percent and we'll get the following two
          // digits on the next loops through.
          result.push_back('%');
        }
      } else {
        // Invalid escape sequence, just pass the percent through and continue
        // right after it.
        result.push_back('%');
      }
    } else if ((rules & UnescapeRule::REPLACE_PLUS_WITH_SPACE) &&
               escaped_text[i] == '+') {
      result.push_back(' ');
    } else {
      // Normal case for unescaped characters.
      result.push_back(escaped_text[i]);
    }
  }

  // Make offset adjustment.
  if (offsets_for_adjustment && !adjustments.empty()) {
    std::for_each(offsets_for_adjustment->begin(),
                   offsets_for_adjustment->end(),
                   AdjustEncodingOffset(adjustments));
  }

  return result;
}

template<typename STR>
STR UnescapeURLImpl(const STR& escaped_text,
                    UnescapeRule::Type rules,
                    size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  STR result = UnescapeURLWithOffsetsImpl(escaped_text, rules, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

}  // namespace

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap(
  0xffffffffL, 0xfc00987dL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeQueryParamValue(const std::string& text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

// Convert the string to a sequence of bytes and then % escape anything
// except alphanumerics and !'()*-._~
string16 EscapeQueryParamValueUTF8(const string16& text,
                                   bool use_plus) {
  return UTF8ToUTF16(Escape(UTF16ToUTF8(text), kQueryCharmap, use_plus));
}

// non-printable, non-7bit, and (including space)  "#%:<>?[\]^`{|}
static const Charmap kPathCharmap(
  0xffffffffL, 0xd400002dL, 0x78000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapePath(const std::string& path) {
  return Escape(path, kPathCharmap, false);
}

// non-printable, non-7bit, and (including space) ?>=<;+'&%$#"![\]^`{|}
static const Charmap kUrlEscape(
  0xffffffffL, 0xf80008fdL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
);

std::string EscapeUrlEncodedData(const std::string& path) {
  return Escape(path, kUrlEscape, true);
}

// non-7bit
static const Charmap kNonASCIICharmap(
  0x00000000L, 0x00000000L, 0x00000000L, 0x00000000L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeNonASCII(const std::string& input) {
  return Escape(input, kNonASCIICharmap, false);
}

// Everything except alphanumerics, the reserved characters(;/?:@&=+$,) and
// !'()*-._~%
static const Charmap kExternalHandlerCharmap(
  0xffffffffL, 0x5000080dL, 0x68000000L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL);

std::string EscapeExternalHandlerValue(const std::string& text) {
  return Escape(text, kExternalHandlerCharmap, false);
}

string16 UnescapeAndDecodeUTF8URLComponentWithOffsets(
    const std::string& text,
    UnescapeRule::Type rules,
    std::vector<size_t>* offsets_for_adjustment) {
  std::wstring result;
  std::vector<size_t> original_offsets;
  if (offsets_for_adjustment)
    original_offsets = *offsets_for_adjustment;
  std::string unescaped_url(
      UnescapeURLWithOffsetsImpl(text, rules, offsets_for_adjustment));
  if (UTF8ToWideAndAdjustOffsets(unescaped_url.data(), unescaped_url.length(),
                                &result, offsets_for_adjustment))
    return WideToUTF16Hack(result);      // Character set looks like it's valid.

  // Not valid.  Return the escaped version.  Undo our changes to
  // |offset_for_adjustment| since we haven't changed the string after all.
  if (offsets_for_adjustment)
    *offsets_for_adjustment = original_offsets;
  return WideToUTF16Hack(UTF8ToWideAndAdjustOffsets(
      text, offsets_for_adjustment));
}

string16 UnescapeAndDecodeUTF8URLComponent(const std::string& text,
                                           UnescapeRule::Type rules,
                                           size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  string16 result =
      UnescapeAndDecodeUTF8URLComponentWithOffsets(text, rules, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

std::string UnescapeURLComponent(const std::string& escaped_text,
                                 UnescapeRule::Type rules) {
  return UnescapeURLWithOffsetsImpl<std::string>(escaped_text, rules, NULL);
}

string16 UnescapeURLComponent(const string16& escaped_text,
                              UnescapeRule::Type rules) {
  return UnescapeURLWithOffsetsImpl<string16>(escaped_text, rules, NULL);
}


template <class str>
void AppendEscapedCharForHTMLImpl(typename str::value_type c, str* output) {
  static const struct {
    char key;
    const char* replacement;
  } kCharsToEscape[] = {
    { '<', "&lt;" },
    { '>', "&gt;" },
    { '&', "&amp;" },
    { '"', "&quot;" },
    { '\'', "&#39;" },
  };
  size_t k;
  for (k = 0; k < ARRAYSIZE_UNSAFE(kCharsToEscape); ++k) {
    if (c == kCharsToEscape[k].key) {
      const char* p = kCharsToEscape[k].replacement;
      while (*p)
        output->push_back(*p++);
      break;
    }
  }
  if (k == ARRAYSIZE_UNSAFE(kCharsToEscape))
    output->push_back(c);
}

void AppendEscapedCharForHTML(char c, std::string* output) {
  AppendEscapedCharForHTMLImpl(c, output);
}

void AppendEscapedCharForHTML(wchar_t c, string16* output) {
  AppendEscapedCharForHTMLImpl(c, output);
}

template <class str>
str EscapeForHTMLImpl(const str& input) {
  str result;
  result.reserve(input.size());  // optimize for no escaping

  for (typename str::const_iterator it = input.begin(); it != input.end(); ++it)
    AppendEscapedCharForHTMLImpl(*it, &result);

  return result;
}

std::string EscapeForHTML(const std::string& input) {
  return EscapeForHTMLImpl(input);
}

string16 EscapeForHTML(const string16& input) {
  return EscapeForHTMLImpl(input);
}

string16 UnescapeForHTML(const string16& input) {
  static const struct {
    const wchar_t* ampersand_code;
    const char replacement;
  } kEscapeToChars[] = {
    { L"&lt;", '<' },
    { L"&gt;", '>' },
    { L"&amp;", '&' },
    { L"&quot;", '"' },
    { L"&#39;", '\''},
  };

  if (input.find(WideToUTF16(L"&")) == std::string::npos)
    return input;

  string16 ampersand_chars[ARRAYSIZE_UNSAFE(kEscapeToChars)];
  string16 text(input);
  for (string16::iterator iter = text.begin(); iter != text.end(); ++iter) {
    if (*iter == '&') {
      // Potential ampersand encode char.
      size_t index = iter - text.begin();
      for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kEscapeToChars); i++) {
        if (ampersand_chars[i].empty())
          ampersand_chars[i] = WideToUTF16(kEscapeToChars[i].ampersand_code);
        if (text.find(ampersand_chars[i], index) == index) {
          text.replace(iter, iter + ampersand_chars[i].length(),
                       1, kEscapeToChars[i].replacement);
          break;
        }
      }
    }
  }
  return text;
}

AdjustEncodingOffset::AdjustEncodingOffset(const Adjustments& adjustments)
  : adjustments(adjustments) {}

void AdjustEncodingOffset::operator()(size_t& offset) {
  // For each encoded character occurring before an offset subtract 2.
  if (offset == string16::npos)
    return;
  size_t adjusted_offset = offset;
  for (Adjustments::const_iterator i = adjustments.begin();
       i != adjustments.end(); ++i) {
    size_t location = *i;
    if (offset <= location) {
      offset = adjusted_offset;
      return;
    }
    if (offset <= (location + 2)) {
      offset = string16::npos;
      return;
    }
    adjusted_offset -= 2;
  }
  offset = adjusted_offset;
}
