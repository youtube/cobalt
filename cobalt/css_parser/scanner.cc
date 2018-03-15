// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/css_parser/scanner.h"

#include <limits>

#include "base/string_util.h"
#include "cobalt/base/compiler.h"
#include "cobalt/css_parser/grammar.h"
#include "cobalt/css_parser/string_pool.h"
#include "cobalt/cssom/character_classification.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/media_feature_keyword_value_names.h"
#include "cobalt/cssom/media_feature_names.h"
#include "cobalt/cssom/media_type_names.h"
#include "cobalt/cssom/pseudo_class_names.h"
#include "cobalt/cssom/pseudo_element_names.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace cobalt {
namespace css_parser {

namespace {

// Types for the main switch.
enum CharacterType {
  // The first 4 types must be grouped together, as they
  // represent the allowed chars in an identifier.
  kCaselessUCharacter,
  kIdentifierStartCharacter,
  kNumberCharacter,
  kDashCharacter,

  kOtherCharacter,
  kNullCharacter,
  kWhitespaceCharacter,
  kEndMediaQueryOrSupportsCharacter,
  kEndNthChildCharacter,
  kQuoteCharacter,
  kExclamationMarkCharacter,
  kHashmarkCharacter,
  kDollarCharacter,
  kAsteriskCharacter,
  kPlusCharacter,
  kDotCharacter,
  kSlashCharacter,
  kLessCharacter,
  kAtCharacter,
  kBackSlashCharacter,
  kXorCharacter,
  kVerticalBarCharacter,
  kTildeCharacter,
};

// 128 ASCII codes.
const CharacterType kTypesOfAsciiCharacters[128] = {
    /*   0 - Null               */ kNullCharacter,
    /*   1 - Start of Heading   */ kOtherCharacter,
    /*   2 - Start of Text      */ kOtherCharacter,
    /*   3 - End of Text        */ kOtherCharacter,
    /*   4 - End of Transm.     */ kOtherCharacter,
    /*   5 - Enquiry            */ kOtherCharacter,
    /*   6 - Acknowledgment     */ kOtherCharacter,
    /*   7 - Bell               */ kOtherCharacter,
    /*   8 - Back Space         */ kOtherCharacter,
    /*   9 - Horizontal Tab     */ kWhitespaceCharacter,
    /*  10 - Line Feed          */ kWhitespaceCharacter,
    /*  11 - Vertical Tab       */ kOtherCharacter,
    /*  12 - Form Feed          */ kWhitespaceCharacter,
    /*  13 - Carriage Return    */ kWhitespaceCharacter,
    /*  14 - Shift Out          */ kOtherCharacter,
    /*  15 - Shift In           */ kOtherCharacter,
    /*  16 - Data Line Escape   */ kOtherCharacter,
    /*  17 - Device Control 1   */ kOtherCharacter,
    /*  18 - Device Control 2   */ kOtherCharacter,
    /*  19 - Device Control 3   */ kOtherCharacter,
    /*  20 - Device Control 4   */ kOtherCharacter,
    /*  21 - Negative Ack.      */ kOtherCharacter,
    /*  22 - Synchronous Idle   */ kOtherCharacter,
    /*  23 - End of Transmit    */ kOtherCharacter,
    /*  24 - Cancel             */ kOtherCharacter,
    /*  25 - End of Medium      */ kOtherCharacter,
    /*  26 - Substitute         */ kOtherCharacter,
    /*  27 - Escape             */ kOtherCharacter,
    /*  28 - File Separator     */ kOtherCharacter,
    /*  29 - Group Separator    */ kOtherCharacter,
    /*  30 - Record Separator   */ kOtherCharacter,
    /*  31 - Unit Separator     */ kOtherCharacter,
    /*  32 - Space              */ kWhitespaceCharacter,
    /*  33 - !                  */ kExclamationMarkCharacter,
    /*  34 - "                  */ kQuoteCharacter,
    /*  35 - #                  */ kHashmarkCharacter,
    /*  36 - $                  */ kDollarCharacter,
    /*  37 - %                  */ kOtherCharacter,
    /*  38 - &                  */ kOtherCharacter,
    /*  39 - '                  */ kQuoteCharacter,
    /*  40 - (                  */ kOtherCharacter,
    /*  41 - )                  */ kEndNthChildCharacter,
    /*  42 - *                  */ kAsteriskCharacter,
    /*  43 - +                  */ kPlusCharacter,
    /*  44 - ,                  */ kOtherCharacter,
    /*  45 - -                  */ kDashCharacter,
    /*  46 - .                  */ kDotCharacter,
    /*  47 - /                  */ kSlashCharacter,
    /*  48 - 0                  */ kNumberCharacter,
    /*  49 - 1                  */ kNumberCharacter,
    /*  50 - 2                  */ kNumberCharacter,
    /*  51 - 3                  */ kNumberCharacter,
    /*  52 - 4                  */ kNumberCharacter,
    /*  53 - 5                  */ kNumberCharacter,
    /*  54 - 6                  */ kNumberCharacter,
    /*  55 - 7                  */ kNumberCharacter,
    /*  56 - 8                  */ kNumberCharacter,
    /*  57 - 9                  */ kNumberCharacter,
    /*  58 - :                  */ kOtherCharacter,
    /*  59 - ;                  */ kEndMediaQueryOrSupportsCharacter,
    /*  60 - <                  */ kLessCharacter,
    /*  61 - =                  */ kOtherCharacter,
    /*  62 - >                  */ kOtherCharacter,
    /*  63 - ?                  */ kOtherCharacter,
    /*  64 - @                  */ kAtCharacter,
    /*  65 - A                  */ kIdentifierStartCharacter,
    /*  66 - B                  */ kIdentifierStartCharacter,
    /*  67 - C                  */ kIdentifierStartCharacter,
    /*  68 - D                  */ kIdentifierStartCharacter,
    /*  69 - E                  */ kIdentifierStartCharacter,
    /*  70 - F                  */ kIdentifierStartCharacter,
    /*  71 - G                  */ kIdentifierStartCharacter,
    /*  72 - H                  */ kIdentifierStartCharacter,
    /*  73 - I                  */ kIdentifierStartCharacter,
    /*  74 - J                  */ kIdentifierStartCharacter,
    /*  75 - K                  */ kIdentifierStartCharacter,
    /*  76 - L                  */ kIdentifierStartCharacter,
    /*  77 - M                  */ kIdentifierStartCharacter,
    /*  78 - N                  */ kIdentifierStartCharacter,
    /*  79 - O                  */ kIdentifierStartCharacter,
    /*  80 - P                  */ kIdentifierStartCharacter,
    /*  81 - Q                  */ kIdentifierStartCharacter,
    /*  82 - R                  */ kIdentifierStartCharacter,
    /*  83 - S                  */ kIdentifierStartCharacter,
    /*  84 - T                  */ kIdentifierStartCharacter,
    /*  85 - U                  */ kCaselessUCharacter,
    /*  86 - V                  */ kIdentifierStartCharacter,
    /*  87 - W                  */ kIdentifierStartCharacter,
    /*  88 - X                  */ kIdentifierStartCharacter,
    /*  89 - Y                  */ kIdentifierStartCharacter,
    /*  90 - Z                  */ kIdentifierStartCharacter,
    /*  91 - [                  */ kOtherCharacter,
    /*  92 - \                  */ kBackSlashCharacter,
    /*  93 - ]                  */ kOtherCharacter,
    /*  94 - ^                  */ kXorCharacter,
    /*  95 - _                  */ kIdentifierStartCharacter,
    /*  96 - `                  */ kOtherCharacter,
    /*  97 - a                  */ kIdentifierStartCharacter,
    /*  98 - b                  */ kIdentifierStartCharacter,
    /*  99 - c                  */ kIdentifierStartCharacter,
    /* 100 - d                  */ kIdentifierStartCharacter,
    /* 101 - e                  */ kIdentifierStartCharacter,
    /* 102 - f                  */ kIdentifierStartCharacter,
    /* 103 - g                  */ kIdentifierStartCharacter,
    /* 104 - h                  */ kIdentifierStartCharacter,
    /* 105 - i                  */ kIdentifierStartCharacter,
    /* 106 - j                  */ kIdentifierStartCharacter,
    /* 107 - k                  */ kIdentifierStartCharacter,
    /* 108 - l                  */ kIdentifierStartCharacter,
    /* 109 - m                  */ kIdentifierStartCharacter,
    /* 110 - n                  */ kIdentifierStartCharacter,
    /* 111 - o                  */ kIdentifierStartCharacter,
    /* 112 - p                  */ kIdentifierStartCharacter,
    /* 113 - q                  */ kIdentifierStartCharacter,
    /* 114 - r                  */ kIdentifierStartCharacter,
    /* 115 - s                  */ kIdentifierStartCharacter,
    /* 116 - t                  */ kIdentifierStartCharacter,
    /* 117 - u                  */ kCaselessUCharacter,
    /* 118 - v                  */ kIdentifierStartCharacter,
    /* 119 - w                  */ kIdentifierStartCharacter,
    /* 120 - x                  */ kIdentifierStartCharacter,
    /* 121 - y                  */ kIdentifierStartCharacter,
    /* 122 - z                  */ kIdentifierStartCharacter,
    /* 123 - {                  */ kEndMediaQueryOrSupportsCharacter,
    /* 124 - |                  */ kVerticalBarCharacter,
    /* 125 - }                  */ kOtherCharacter,
    /* 126 - ~                  */ kTildeCharacter,
    /* 127 - Delete             */ kOtherCharacter,
};

// Facilitates comparison of an input character to a lowercase English
// character. This is low-level facility, normally you will want to use
// IsAsciiAlphaCaselessEqual() for that.
inline char ToAsciiLowerUnchecked(char character) { return character | 0x20; }

// Compares an input character to a (preferably) constant ASCII lowercase.
inline bool IsAsciiAlphaCaselessEqual(char actual, char expected) {
  DCHECK_GE(expected, 'a');
  DCHECK_LE(expected, 'z');
  return LIKELY(ToAsciiLowerUnchecked(actual) == expected);
}

inline bool IsCssEscape(char character) {
  return character >= ' ' && character != 127;
}

inline bool IsIdentifierStartAfterDash(const char* character_iterator) {
  return IsAsciiAlpha(character_iterator[0]) || character_iterator[0] == '_' ||
         character_iterator[0] < 0 ||
         (character_iterator[0] == '\\' && IsCssEscape(character_iterator[1]));
}

inline const char* SkipWhiteSpace(const char* input_iterator) {
  while (cssom::IsWhiteSpace(*input_iterator)) {
    ++input_iterator;
  }
  return input_iterator;
}

// Returns false if input iterator doesn't point at the escape sequence.
// Otherwise sets the result iterator to a position of the character that
// follows the escape sequence.
bool CheckAndSkipEscape(const char* input_iterator,
                        const char** result_iterator) {
  DCHECK_EQ(*input_iterator, '\\');

  ++input_iterator;
  if (!IsCssEscape(*input_iterator)) {
    return false;
  }

  if (IsHexDigit(*input_iterator)) {
    int length(6);

    do {
      ++input_iterator;
    } while (IsHexDigit(*input_iterator) && --length > 0);

    // Optional space after the escape sequence.
    if (cssom::IsWhiteSpace(*input_iterator)) {
      ++input_iterator;
    }
    *result_iterator = input_iterator;
    return true;
  }

  *result_iterator = input_iterator + 1;
  return true;
}

// Returns false if input iterator doesn't point at the valid string.
// Otherwise sets the result iterator to a position of the character that
// follows the string.
inline bool CheckAndSkipString(const char* input_iterator, char quote,
                               const char** result_iterator) {
  while (true) {
    if (UNLIKELY(*input_iterator == quote)) {
      // String parsing is successful.
      *result_iterator = input_iterator + 1;
      return true;
    }
    if (UNLIKELY(*input_iterator == '\0')) {
      // String parsing is successful up to end of input.
      *result_iterator = input_iterator;
      return true;
    }
    if (UNLIKELY(
            *input_iterator <= '\r' &&
            (*input_iterator == '\n' || (*input_iterator | 0x1) == '\r'))) {
      // String parsing is failed for character '\n', '\f' or '\r'.
      return false;
    }

    if (LIKELY(input_iterator[0] != '\\')) {
      ++input_iterator;
    } else if (input_iterator[1] == '\n' || input_iterator[1] == '\f') {
      input_iterator += 2;
    } else if (input_iterator[1] == '\r') {
      input_iterator += input_iterator[2] == '\n' ? 3 : 2;
    } else {
      if (!CheckAndSkipEscape(input_iterator, &input_iterator)) {
        return false;
      }
    }
  }
}

inline bool IsUriLetter(char character) {
  return (character >= '*' && character != 127) ||
         (character >= '#' && character <= '&') || character == '!';
}

inline bool IsCssLetter(char character) {
  return character < 0 ||
         kTypesOfAsciiCharacters[static_cast<unsigned char>(character)] <=
             kDashCharacter;
}

// Compares a character range with a zero terminated string.
inline bool IsEqualToCssIdentifier(const char* actual, const char* expected) {
  do {
    // The input must be part of an identifier if "actual" or "expected"
    // contains '-'. Otherwise ToAsciiLowerUnchecked('\r') would be equal
    // to '-'.
    DCHECK((*expected >= 'a' && *expected <= 'z') ||
           (*expected >= '0' && *expected <= '9') || *expected == '-');
    DCHECK(*expected != '-' || IsCssLetter(*actual));
    if (ToAsciiLowerUnchecked(*actual++) != (*expected++)) {
      return false;
    }
  } while (*expected);
  return true;
}

}  // namespace

Scanner::Scanner(const char* input_iterator, StringPool* string_pool)
    : input_iterator_(input_iterator),
      string_pool_(string_pool),
      parsing_mode_(kNormalMode),
      line_number_(1),
      line_start_(input_iterator) {}

void Scanner::ScanUnrecognizedAtRule() {
  std::stack<char> brace_stack;
  bool enter_rule_body = false;
  // Until a no matching closing curly-brace <}-token> is scanned, reach
  // the end of input or encounter a semicolon token before entering rule body.
  while (*input_iterator_ != '\0') {
    char first_input_iterator(*input_iterator_++);
    if (first_input_iterator == '\n') {
      ++line_number_;
      line_start_ = input_iterator_;
      continue;
    }

    if (!enter_rule_body && first_input_iterator == ';') {
      // Encountering a <semicolon-token> ends the at-rule immediately.
      return;
    }
    if (!enter_rule_body && first_input_iterator == '{') {
      // Encountering an opening curly-brace <{-token> starts the at-rule's
      // body.
      enter_rule_body = true;
      continue;
    }

    // The at-rule seeks forward, matching blocks
    // (content surrounded by (), {}, or []).
    if (first_input_iterator == '(' || first_input_iterator == '{' ||
        first_input_iterator == '[') {
      // Push the open brace to stack.
      brace_stack.push(first_input_iterator);
      continue;
    }

    if (first_input_iterator == ')' || first_input_iterator == '}' ||
        first_input_iterator == ']') {
      // Encountering a closing curly-brace <}-token> that isn't matched by
      // anything else or inside of another block.
      if (first_input_iterator == '}' && brace_stack.empty()) {
        return;
      }

      if (brace_stack.empty()) {
        // no matching found with ')' and ']' end brace, skip it. The scanner
        // does not care about the at rule grammar.
        continue;
      }

      if ((first_input_iterator == ')' && brace_stack.top() == '(') ||
          (first_input_iterator == '}' && brace_stack.top() == '{') ||
          (first_input_iterator == ']' && brace_stack.top() == '[')) {
        // Pop the open brace from stack when encourtering a match.
        brace_stack.pop();

        continue;
      }
    }
  }
}

void Scanner::HandleBraceIfExists(char character) {
  if (character == '[' || character == '(' || character == '{') {
    open_braces_.push(character);
  }

  if (character == ']' || character == ')' || character == '}') {
    if (!open_braces_.empty()) {
      char top = open_braces_.top();
      if ((top == '[' && character == ']') ||
          (top == '(' && character == ')') ||
          (top == '{' && character == '}')) {
        // Found a match, then pop the top element.
        open_braces_.pop();
      }
    }
  }
}

Token Scanner::Scan(TokenValue* token_value, YYLTYPE* token_location) {
  // Loop until non-comment token is scanned.
  while (true) {
    token_location->first_line = line_number_;
    token_location->first_column =
        static_cast<int>(input_iterator_ - line_start_) + 1;
    token_location->line_start = line_start_;

    // Return all prepended tokens before actual input.
    if (!prepended_tokens_.empty()) {
      Token prepended_token = prepended_tokens_.front();
      if ((prepended_token == kMediaListEntryPointToken) ||
          (prepended_token == kMediaQueryEntryPointToken)) {
        parsing_mode_ = kMediaQueryMode;
      }
      prepended_tokens_.pop_front();
      return prepended_token;
    }

    char first_character(*input_iterator_);
    HandleBraceIfExists(first_character);
    CharacterType character_type(
        first_character >= 0 ? kTypesOfAsciiCharacters
                                   [static_cast<unsigned char>(first_character)]
                             : kIdentifierStartCharacter);
    switch (character_type) {
      case kCaselessUCharacter:
        return ScanFromCaselessU(token_value);
      case kIdentifierStartCharacter:
        return ScanFromIdentifierStart(token_value);
      case kDotCharacter:
        return ScanFromDot(token_value);
      case kNumberCharacter:
        return ScanFromNumber(token_value);
      case kDashCharacter:
        return ScanFromDash(token_value);
      case kOtherCharacter:
        return ScanFromOtherCharacter();
      case kNullCharacter:
        return ScanFromNull();
      case kWhitespaceCharacter:
        return ScanFromWhitespace();
      case kEndMediaQueryOrSupportsCharacter:
        return ScanFromEndMediaQueryOrSupports();
      case kEndNthChildCharacter:
        return ScanFromEndNthChild();
      case kQuoteCharacter:
        return ScanFromQuote(token_value);
      case kExclamationMarkCharacter:
        return ScanFromExclamationMark(token_value);
      case kHashmarkCharacter:
        return ScanFromHashmark(token_value);
      case kSlashCharacter: {
        // Ignore comments. They are not even considered as white spaces.
        Token token(ScanFromSlash());
        if (token != kCommentToken) {
          return token;
        }
        break;
      }
      case kDollarCharacter:
        return ScanFromDollar();
      case kAsteriskCharacter:
        return ScanFromAsterisk();
      case kPlusCharacter:
        return ScanFromPlus(token_value);
      case kLessCharacter:
        return ScanFromLess();
      case kAtCharacter: {
        Token token(ScanFromAt(token_value));
        // Ignore other browsers at rule and not supported at rule.
        if (token == kInvalidAtBlockToken ||
            token == kOtherBrowserAtBlockToken) {
          ScanUnrecognizedAtRule();
        }
        return token;
      }
      case kBackSlashCharacter:
        return ScanFromBackSlash(token_value);
      case kXorCharacter:
        return ScanFromXor();
      case kVerticalBarCharacter:
        return ScanFromVerticalBar();
      case kTildeCharacter:
        return ScanFromTilde();
    }
  }
}

void Scanner::PrependToken(Token token) { prepended_tokens_.push_back(token); }

bool Scanner::DetectPropertyNameToken(const std::string& property_name,
                                      Token* property_name_token) const {
  return DetectPropertyNameToken(
      TrivialStringPiece::FromCString(property_name.c_str()),
      property_name_token);
}

Token Scanner::ScanFromCaselessU(TokenValue* token_value) {
  const char* saved_input_iterator(input_iterator_++);
  if (UNLIKELY(*input_iterator_ == '+')) {
    if (TryScanUnicodeRange(&token_value->integer_pair)) {
      return kUnicodeRangeToken;
    }
  }

  input_iterator_ = saved_input_iterator;
  return ScanFromIdentifierStart(token_value);
}

Token Scanner::ScanFromIdentifierStart(TokenValue* token_value) {
  const char* first_character_iterator(input_iterator_);

  if (parsing_mode_ == kMediaQueryMode) {
    Token media_feature_name_prefix;
    if (DetectMediaFeatureNamePrefix(&media_feature_name_prefix)) {
      return media_feature_name_prefix;
    }
  }

  bool has_escape;
  ScanIdentifier(&token_value->string, &has_escape);

  if (LIKELY(!has_escape)) {
    if (parsing_mode_ == kMediaQueryMode) {
      Token media_query_token;
      cssom::MediaFeatureName media_feature_name;
      if (DetectMediaQueryToken(token_value->string, &media_query_token,
                                &media_feature_name)) {
        token_value->integer = static_cast<int>(media_feature_name);
        return media_query_token;
      }
    } else {
      Token property_name_token;
      if (DetectPropertyNameToken(token_value->string, &property_name_token)) {
        return property_name_token;
      }

      Token property_value_token;
      if (DetectPropertyValueToken(token_value->string,
                                   &property_value_token)) {
        return property_value_token;
      }

      Token pseudo_class_name_token;
      if (DetectPseudoClassNameToken(token_value->string,
                                     &pseudo_class_name_token)) {
        return pseudo_class_name_token;
      }

      Token pseudo_element_name_token;
      if (DetectPseudoElementNameToken(token_value->string,
                                       &pseudo_element_name_token)) {
        return pseudo_element_name_token;
      }
    }
  }

  if (UNLIKELY(*input_iterator_ == '(')) {
    // Cache the open brace.
    open_braces_.push('(');

    if (parsing_mode_ == kSupportsMode && !has_escape) {
      Token supports_token;
      if (DetectSupportsToken(token_value->string, &supports_token)) {
        return supports_token;
      }
    }

    if (!has_escape) {
      Token function_token;
      if (DetectKnownFunctionTokenAndMaybeChangeParsingMode(token_value->string,
                                                            &function_token)) {
        ++input_iterator_;
        if (function_token == kUriToken) {
          TrivialStringPiece uri;
          if (TryScanUri(&uri)) {
            token_value->string = uri;
            return kUriToken;
          }
          return kInvalidFunctionToken;
        }
        return function_token;
      }

      if (parsing_mode_ == kMediaQueryMode) {
        // ... and(max-width: 480px) ... looks like a function, but in fact
        // it is not, so run more detection code in the MediaQueryMode.
        Token media_query_token;
        cssom::MediaFeatureName media_feature_name;
        if (DetectMediaQueryToken(token_value->string, &media_query_token,
                                  &media_feature_name)) {
          token_value->integer = static_cast<int>(media_feature_name);
          return media_query_token;
        }
      }
    }

    ++input_iterator_;
    return kInvalidFunctionToken;
  }

  if (UNLIKELY(parsing_mode_ != kNormalMode) && !has_escape) {
    if (parsing_mode_ == kMediaQueryMode) {
      Token media_query_token;
      cssom::MediaFeatureName media_feature_name;
      if (DetectMediaQueryToken(token_value->string, &media_query_token,
                                &media_feature_name)) {
        token_value->integer = static_cast<int>(media_feature_name);
        return media_query_token;
      }
    } else if (parsing_mode_ == kSupportsMode) {
      Token supports_token;
      if (DetectSupportsToken(token_value->string, &supports_token)) {
        return supports_token;
      }
    } else if (parsing_mode_ == kNthChildMode &&
               IsAsciiAlphaCaselessEqual(token_value->string.begin[0], 'n')) {
      if (token_value->string.size() == 1) {
        // String "n" is kIdentifier but "n+1" is kNthToken.
        TrivialStringPiece nth;
        if (TryScanNthChildExtra(&nth)) {
          token_value->string.end = nth.end;
          return kNthToken;
        }
      } else if (token_value->string.size() >= 2 &&
                 token_value->string.begin[1] == '-') {
        // String "n-" is kIdentifierToken but "n-1" is kNthToken.
        // Set input_iterator_ to '-' to continue parsing.
        const char* saved_input_iterator(input_iterator_);
        input_iterator_ = first_character_iterator + 1;

        TrivialStringPiece nth;
        if (TryScanNthChildExtra(&nth)) {
          token_value->string.end = nth.end;
          return kNthToken;
        }

        // Revert the change to input_iterator_ if unsuccessful.
        input_iterator_ = saved_input_iterator;
      }
    }
  }

  return kIdentifierToken;
}

Token Scanner::ScanFromDot(TokenValue* token_value) {
  if (!IsAsciiDigit(input_iterator_[1])) {
    ++input_iterator_;
    return '.';
  }
  return ScanFromNumber(token_value);
}

Token Scanner::ScanFromNumber(TokenValue* token_value) {
  TrivialStringPiece number;
  number.begin = input_iterator_;

  // Negative numbers are handled in the grammar.
  // Scientific notation is required by the standard but is not supported
  // neither by WebKit nor Blink.
  bool dot_seen(false);
  while (true) {
    if (!IsAsciiDigit(input_iterator_[0])) {
      // Only one dot is allowed for a number,
      // and it must be followed by a digit.
      if (input_iterator_[0] != '.' || dot_seen ||
          !IsAsciiDigit(input_iterator_[1])) {
        break;
      }
      dot_seen = true;
    }
    ++input_iterator_;
  }
  number.end = input_iterator_;

  if (UNLIKELY(parsing_mode_ == kNthChildMode) && !dot_seen &&
      IsAsciiAlphaCaselessEqual(*input_iterator_, 'n')) {
    // A string that matches a regexp "[0-9]+n" is always an kNthToken.
    token_value->string.begin = number.begin;
    token_value->string.end = ++input_iterator_;

    TrivialStringPiece nth;
    if (TryScanNthChildExtra(&nth)) {
      token_value->string.end = nth.end;
    }
    return kNthToken;
  }

  char* number_end(const_cast<char*>(number.end));
  // We parse into |double| for two reasons:
  //   - C++03 doesn't have std::strtof() function;
  //   - |float|'s significand is not large enough to represent |int| precisely.
  // |number_end| is used by std::strtod() as a pure output parameter - it's
  // input value is not used. std::strtod() may parse more of the number than
  // we expect, e.g. in the case of scientific notation or hexadecimal format.
  // In these cases (number_end != number.end), return an invalid number token.
  double real_as_double(std::strtod(number.begin, &number_end));

  if (number_end != number.end ||
      real_as_double != real_as_double ||  // n != n if and only if it's NaN.
      real_as_double == std::numeric_limits<float>::infinity() ||
      real_as_double > std::numeric_limits<float>::max()) {
    token_value->string.begin = number.begin;
    token_value->string.end = number.end;
    return kInvalidNumberToken;
  }

  float real_as_float = static_cast<float>(real_as_double);

  // Type of the function.
  if (IsInputIteratorAtIdentifierStart()) {
    TrivialStringPiece unit;
    bool has_escape;
    ScanIdentifier(&unit, &has_escape);

    Token unit_token;
    if (has_escape || !DetectUnitToken(unit, &unit_token)) {
      token_value->string.begin = number.begin;
      token_value->string.end = unit.end;
      return kInvalidDimensionToken;
    }

    token_value->real = real_as_float;
    return unit_token;
  }

  if (*input_iterator_ == '%') {
    ++input_iterator_;
    token_value->real = real_as_float;
    return kPercentageToken;
  }

  if (!dot_seen && real_as_double <= std::numeric_limits<int>::max()) {
    token_value->integer = static_cast<int>(real_as_double);
    return kIntegerToken;
  }

  token_value->real = real_as_float;
  return kRealToken;
}

Token Scanner::ScanFromDash(TokenValue* token_value) {
  if (IsIdentifierStartAfterDash(input_iterator_ + 1)) {
    const char* first_character_iterator(input_iterator_);

    TrivialStringPiece name;
    bool has_escape;
    ScanIdentifier(&name, &has_escape);

    if (LIKELY(!has_escape)) {
      Token property_name_token;
      if (DetectPropertyNameToken(name, &property_name_token)) {
        return property_name_token;
      }
    }

    if (*input_iterator_ == '(') {
      // Cache the open brace.
      open_braces_.push('(');

      if (!has_escape) {
        Token function_token;
        if (DetectKnownFunctionTokenAndMaybeChangeParsingMode(
                name, &function_token)) {
          ++input_iterator_;
          return function_token;
        }
      }

      ++input_iterator_;
      token_value->string = name;
      return kInvalidFunctionToken;
    }

    if (UNLIKELY(parsing_mode_ == kNthChildMode) && !has_escape &&
        IsAsciiAlphaCaselessEqual(first_character_iterator[1], 'n')) {
      if (name.size() == 2) {
        // String "-n" is kIdentifierToken but "-n+1" is kNthToken.
        TrivialStringPiece nth_extra;
        if (TryScanNthChildExtra(&nth_extra)) {
          token_value->string.begin = name.begin;
          token_value->string.end = nth_extra.end;
          return kNthToken;
        }
      }

      if (name.size() >= 3 && first_character_iterator[2] == '-') {
        // String "-n-" is kIdentifierToken but "-n-1" is kNthToken.
        // Set input_iterator_ to second '-' of '-n-' to continue parsing.
        const char* saved_input_iterator(input_iterator_);
        input_iterator_ = first_character_iterator + 2;

        TrivialStringPiece nth_extra;
        if (TryScanNthChildExtra(&nth_extra)) {
          token_value->string.begin = name.begin;
          token_value->string.end = nth_extra.end;
          return kNthToken;
        }

        // Revert the change to currentCharacter if unsuccessful.
        input_iterator_ = saved_input_iterator;
      }
    }

    token_value->string = name;
    return kIdentifierToken;
  }

  if (input_iterator_[1] == '-' && input_iterator_[2] == '>') {
    input_iterator_ += 3;
    return kSgmlCommentDelimiterToken;
  }

  ++input_iterator_;

  if (UNLIKELY(parsing_mode_ == kNthChildMode)) {
    // A string that matches a regexp "-[0-9]+n" is always an kNthToken.
    TrivialStringPiece nth;
    if (TryScanNthChild(&nth)) {
      token_value->string.begin = nth.begin - 1;  // Include leading minus.

      TrivialStringPiece nth_extra;
      token_value->string.end =
          TryScanNthChildExtra(&nth_extra) ? nth_extra.end : nth.end;
      return kNthToken;
    }
  }

  return '-';
}

Token Scanner::ScanFromOtherCharacter() {
  // A token is simply the current character.
  return *input_iterator_++;
}

Token Scanner::ScanFromNull() {
  // If there are still unmatched open braces at the end of input, pop the open
  // braces and return the their matched close braces.
  if (!open_braces_.empty()) {
    char pop_element = open_braces_.top();
    open_braces_.pop();

    if (pop_element == '[') {
      return ']';
    } else if (pop_element == '{') {
      return '}';
    } else if (pop_element == '(') {
      return ')';
    }
  }

  // Do not advance pointer at the end of input.
  return kEndOfFileToken;
}

Token Scanner::ScanFromWhitespace() {
  // Might start with a '\n'.
  do {
    if (*input_iterator_ == '\n') {
      ++line_number_;
      line_start_ = input_iterator_ + 1;
    }
    ++input_iterator_;
  } while (
      *input_iterator_ >= 0 &&
      (kTypesOfAsciiCharacters[static_cast<unsigned char>(*input_iterator_)] ==
       kWhitespaceCharacter));
  return kWhitespaceToken;
}

Token Scanner::ScanFromEndMediaQueryOrSupports() {
  if (parsing_mode_ == kMediaQueryMode || parsing_mode_ == kSupportsMode) {
    parsing_mode_ = kNormalMode;
  }
  return *input_iterator_++;
}

Token Scanner::ScanFromEndNthChild() {
  if (parsing_mode_ == kNthChildMode) {
    parsing_mode_ = kNormalMode;
  }
  ++input_iterator_;
  return ')';
}

Token Scanner::ScanFromQuote(TokenValue* token_value) {
  char quote(*input_iterator_++);

  const char* string_end;
  if (CheckAndSkipString(input_iterator_, quote, &string_end)) {
    ScanString(quote, &token_value->string);
    return kStringToken;
  }

  return quote;
}

Token Scanner::ScanFromExclamationMark(TokenValue* /*token_value*/) {
  ++input_iterator_;

  const char* important_start(SkipWhiteSpace(input_iterator_));
  if (IsEqualToCssIdentifier(important_start, "important")) {
    input_iterator_ = important_start + 9;
    return kImportantToken;
  }

  return '!';
}

Token Scanner::ScanFromHashmark(TokenValue* token_value) {
  ++input_iterator_;

  if (IsAsciiDigit(*input_iterator_)) {
    // This must be a valid hex number token.
    token_value->string.begin = input_iterator_;
    do {
      ++input_iterator_;
    } while (IsHexDigit(*input_iterator_));
    token_value->string.end = input_iterator_;
    return kHexToken;
  } else if (IsInputIteratorAtIdentifierStart()) {
    bool has_escape;
    ScanIdentifier(&token_value->string, &has_escape);

    if (has_escape) {
      return kIdSelectorToken;
    }

    // Check whether the identifier is also a valid hex number.
    for (const char* hex_iterator = token_value->string.begin;
         hex_iterator != token_value->string.end; ++hex_iterator) {
      if (!IsHexDigit(*hex_iterator)) {
        return kIdSelectorToken;
      }
    }

    return kHexToken;
  }

  return '#';
}

Token Scanner::ScanFromSlash() {
  ++input_iterator_;

  // Ignore comments. They are not even considered as white spaces.
  if (*input_iterator_ == '*') {
    ++input_iterator_;
    while (input_iterator_[0] != '*' || input_iterator_[1] != '/') {
      if (*input_iterator_ == '\n') {
        ++line_number_;
        line_start_ = input_iterator_ + 1;
      }
      if (*input_iterator_ == '\0') {
        // Unterminated comments are simply ignored.
        input_iterator_ -= 2;
        break;
      }
      ++input_iterator_;
    }
    input_iterator_ += 2;
    return kCommentToken;
  }

  return '/';
}

Token Scanner::ScanFromDollar() {
  ++input_iterator_;

  if (*input_iterator_ == '=') {
    ++input_iterator_;
    return kEndsWithToken;
  }

  return '$';
}

Token Scanner::ScanFromAsterisk() {
  ++input_iterator_;

  if (*input_iterator_ == '=') {
    ++input_iterator_;
    return kContainsToken;
  }

  return '*';
}

Token Scanner::ScanFromPlus(TokenValue* token_value) {
  ++input_iterator_;

  if (UNLIKELY(parsing_mode_ == kNthChildMode)) {
    // A string that matches a regexp "+[0-9]*n" is always an kNthToken.
    TrivialStringPiece nth;
    if (TryScanNthChild(&nth)) {
      token_value->string.begin = nth.begin;

      TrivialStringPiece nth_extra;
      token_value->string.end =
          TryScanNthChildExtra(&nth_extra) ? nth_extra.end : nth.end;
      return kNthToken;
    }
  }

  return '+';
}

bool Scanner::DetectMediaFeatureNamePrefix(Token* token) {
  if (ToAsciiLowerUnchecked(input_iterator_[0]) == 'm') {
    if (ToAsciiLowerUnchecked(input_iterator_[1]) == 'i' &&
        ToAsciiLowerUnchecked(input_iterator_[2]) == 'n' &&
        ToAsciiLowerUnchecked(input_iterator_[3]) == '-') {
      input_iterator_ += 4;
      *token = kMediaMinimumToken;
      return true;
    }
    if (ToAsciiLowerUnchecked(input_iterator_[1]) == 'a' &&
        ToAsciiLowerUnchecked(input_iterator_[2]) == 'x' &&
        ToAsciiLowerUnchecked(input_iterator_[3]) == '-') {
      input_iterator_ += 4;
      *token = kMediaMaximumToken;
      return true;
    }
  }
  return false;
}

Token Scanner::ScanFromLess() {
  ++input_iterator_;

  if (input_iterator_[0] == '!' &&
      input_iterator_[1] == '-' &&
      input_iterator_[2] == '-') {
    input_iterator_ += 3;
    return kSgmlCommentDelimiterToken;
  }

  return '<';
}

Token Scanner::ScanFromAt(TokenValue* token_value) {
  ++input_iterator_;

  if (IsInputIteratorAtIdentifierStart()) {
    TrivialStringPiece name;
    bool has_escape;
    ScanIdentifier(&name, &has_escape);

    // Include leading "@" in identifier.
    if (has_escape) {
      std::string* name_string = string_pool_->AllocateString();
      name_string->push_back('@');
      name_string->append(name.ToString());
      name.begin = name_string->c_str();
      name.end = name.begin + name_string->size();
    } else {
      --name.begin;
    }

    Token at_token;
    if (DetectAtTokenAndMaybeChangeParsingMode(name, has_escape, &at_token)) {
      if (at_token == kKeyframesToken || at_token == kMediaToken ||
          at_token == kFontFaceToken || at_token == kOtherBrowserAtBlockToken) {
        // We only return the at_token when matched these four at rules at this
        // point, and any other kinds of at rules are not supported.
        return at_token;
      }
    }

    token_value->string = name;
    return kInvalidAtBlockToken;
  }

  return '@';
}

Token Scanner::ScanFromBackSlash(TokenValue* token_value) {
  if (IsCssEscape(input_iterator_[1])) {
    bool has_escape;
    ScanIdentifier(&token_value->string, &has_escape);
    return kIdentifierToken;
  }

  return *input_iterator_++;
}

Token Scanner::ScanFromXor() {
  ++input_iterator_;

  if (*input_iterator_ == '=') {
    ++input_iterator_;
    return kBeginsWithToken;
  }

  return '^';
}

Token Scanner::ScanFromVerticalBar() {
  ++input_iterator_;

  if (*input_iterator_ == '=') {
    ++input_iterator_;
    return kDashMatchToken;
  }

  return '|';
}

Token Scanner::ScanFromTilde() {
  ++input_iterator_;

  if (*input_iterator_ == '=') {
    ++input_iterator_;
    return kIncludesToken;
  }

  return '~';
}

bool Scanner::TryScanUnicodeRange(TrivialIntPair* value) {
  DCHECK_EQ(*input_iterator_, '+');
  const char* saved_input_iterator(input_iterator_);
  ++input_iterator_;
  int length = 6;
  int start_value = 0;
  bool range_set = false;

  while (IsHexDigit(*input_iterator_) && length > 0) {
    start_value = start_value * 16 + HexDigitToInt(*input_iterator_);
    ++input_iterator_;
    --length;
  }

  int32 end_value = start_value;

  if (length > 0 && *input_iterator_ == '?') {
    // At most 5 hex digit followed by a question mark.
    do {
      start_value *= 16;
      end_value = end_value * 16 + 0xF;
      ++input_iterator_;
      --length;
    } while (length > 0 && *input_iterator_ == '?');
    range_set = true;
  } else if (length < 6) {
    // At least one hex digit.
    if (input_iterator_[0] == '-' && IsHexDigit(input_iterator_[1])) {
      // Followed by a dash and a hex digit.
      ++input_iterator_;
      length = 6;
      end_value = 0;
      do {
        end_value = end_value * 16 + HexDigitToInt(*input_iterator_);
        ++input_iterator_;
      } while (--length > 0 && IsHexDigit(*input_iterator_));
    }
    range_set = true;
  }

  if (range_set) {
    value->first = start_value;
    value->second = end_value;
    return true;
  }

  // Revert parsing position if failed to parse.
  input_iterator_ = saved_input_iterator;
  return false;
}

void Scanner::ScanIdentifier(TrivialStringPiece* value, bool* has_escape) {
  // If a valid identifier start is found, we can safely
  // parse the identifier until the next invalid character.
  DCHECK(IsInputIteratorAtIdentifierStart());

  *has_escape = !TryScanAndMaybeCopyIdentifier<false>(value, NULL);
  if (UNLIKELY(*has_escape)) {
    CHECK(TryScanAndMaybeCopyIdentifier<true>(value,
                                              string_pool_->AllocateString()));
  }
}

bool Scanner::IsInputIteratorAtIdentifierStart() const {
  // Check whether an identifier is started.
  return IsIdentifierStartAfterDash(
      *input_iterator_ != '-' ? input_iterator_ : input_iterator_ + 1);
}

template <bool copy>
bool Scanner::TryScanAndMaybeCopyIdentifier(TrivialStringPiece* value,
                                            std::string* value_copy) {
  DCHECK(!copy || value_copy != NULL);

  if (!copy) {
    value->begin = input_iterator_;
  }

  do {
    if (LIKELY(*input_iterator_ != '\\')) {
      if (copy) {
        value_copy->push_back(*input_iterator_);
      }
      ++input_iterator_;
    } else {
      if (!copy) {
        // Cannot return a result without a modification.
        input_iterator_ = value->begin;
        return false;
      }
      icu::UnicodeString(ScanEscape()).toUTF8String(*value_copy);
    }
  } while (IsCssLetter(input_iterator_[0]) ||
           (input_iterator_[0] == '\\' && IsCssEscape(input_iterator_[1])));

  if (copy) {
    value->begin = value_copy->c_str();
    value->end = value->begin + value_copy->size();
  } else {
    // value->begin was initialized in the beginning of the method.
    value->end = input_iterator_;
  }
  return true;
}

UChar32 Scanner::ScanEscape() {
  DCHECK_EQ(*input_iterator_, '\\');
  DCHECK(IsCssEscape(input_iterator_[1]));

  UChar32 character(0);

  ++input_iterator_;
  if (IsHexDigit(*input_iterator_)) {
    int length(6);

    do {
      character = (character << 4) + HexDigitToInt(*input_iterator_++);
    } while (--length > 0 && IsHexDigit(*input_iterator_));

    // Characters above 0x10ffff are not handled.
    if (character > 0x10ffff) {
      character = 0xfffd;
    }

    // Optional space after the escape sequence.
    if (cssom::IsWhiteSpace(*input_iterator_)) {
      ++input_iterator_;
    }

    return character;
  }

  return *input_iterator_++;
}

// WARNING: every time a new name token is introduced, it should be added
//          to |identifier_token| rule in grammar.y.
bool Scanner::DetectPropertyNameToken(const TrivialStringPiece& name,
                                      Token* property_name_token) const {
  DCHECK_GT(name.size(), 0U);
  switch (name.size()) {
    case 3:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::GetPropertyName(cssom::kAllProperty))) {
        *property_name_token = kAllToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::GetPropertyName(cssom::kSrcProperty))) {
        *property_name_token = kSrcToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::GetPropertyName(cssom::kTopProperty))) {
        *property_name_token = kTopToken;
        return true;
      }
      return false;

    case 4:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFontProperty))) {
        *property_name_token = kFontToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kLeftProperty))) {
        *property_name_token = kLeftToken;
        return true;
      }
      return false;

    case 5:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kColorProperty))) {
        *property_name_token = kColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kRightProperty))) {
        *property_name_token = kRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kWidthProperty))) {
        *property_name_token = kWidthToken;
        return true;
      }
      return false;

    case 6:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBorderProperty))) {
        *property_name_token = kBorderToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBottomProperty))) {
        *property_name_token = kBottomToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFilterProperty))) {
        *property_name_token = kFilterToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kHeightProperty))) {
        *property_name_token = kHeightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMarginProperty))) {
        *property_name_token = kMarginToken;
        return true;
      }
      return false;

    case 7:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kContentProperty))) {
        *property_name_token = kContentToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kDisplayProperty))) {
        *property_name_token = kDisplayToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kOpacityProperty))) {
        *property_name_token = kOpacityToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kOutlineProperty))) {
        *property_name_token = kOutlineToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kPaddingProperty))) {
        *property_name_token = kPaddingToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kZIndexProperty))) {
        *property_name_token = kZIndexToken;
        return true;
      }
      return false;

    case 8:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kOverflowProperty))) {
        *property_name_token = kOverflowToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kPositionProperty))) {
        *property_name_token = kPositionToken;
        return true;
      }
      return false;

    case 9:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kAnimationProperty))) {
        *property_name_token = kAnimationToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFontSizeProperty))) {
        *property_name_token = kFontSizeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMaxWidthProperty))) {
        *property_name_token = kMaxWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMinWidthProperty))) {
        *property_name_token = kMinWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kTransformProperty))) {
        *property_name_token = kTransformToken;
        return true;
      }
      // NOTE: word-wrap is treated as an alias for overflow-wrap
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kWordWrapProperty))) {
        *property_name_token = kOverflowWrapToken;
        return true;
      }
      return false;

    case 10:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBackgroundProperty))) {
        *property_name_token = kBackgroundToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBorderTopProperty))) {
        *property_name_token = kBorderTopToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBoxShadowProperty))) {
        *property_name_token = kBoxShadowToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFontStyleProperty))) {
        *property_name_token = kFontStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMarginTopProperty))) {
        *property_name_token = kMarginTopToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMaxHeightProperty))) {
        *property_name_token = kMaxHeightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMinHeightProperty))) {
        *property_name_token = kMinHeightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kTextAlignProperty))) {
        *property_name_token = kTextAlignToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kTransitionProperty))) {
        *property_name_token = kTransitionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kVisibilityProperty))) {
        *property_name_token = kVisibilityToken;
        return true;
      }
      return false;

    case 11:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kBorderLeftProperty))) {
        *property_name_token = kBorderLeftToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFontFamilyProperty))) {
        *property_name_token = kFontFamilyToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kFontWeightProperty))) {
        *property_name_token = kFontWeightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kLineHeightProperty))) {
        *property_name_token = kLineHeightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kMarginLeftProperty))) {
        *property_name_token = kMarginLeftToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kPaddingTopProperty))) {
        *property_name_token = kPaddingTopToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kTextIndentProperty))) {
        *property_name_token = kTextIndentToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kTextShadowProperty))) {
        *property_name_token = kTextShadowToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(cssom::kWhiteSpaceProperty))) {
        *property_name_token = kWhiteSpacePropertyToken;
        return true;
      }
      return false;

    case 12:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderRightProperty))) {
        *property_name_token = kBorderRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderColorProperty))) {
        *property_name_token = kBorderColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderStyleProperty))) {
        *property_name_token = kBorderStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderWidthProperty))) {
        *property_name_token = kBorderWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kMarginRightProperty))) {
        *property_name_token = kMarginRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kPaddingLeftProperty))) {
        *property_name_token = kPaddingLeftToken;
        return true;
      }
      return false;

    case 13:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderBottomProperty))) {
        *property_name_token = kBorderBottomToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderRadiusProperty))) {
        *property_name_token = kBorderRadiusToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kMarginBottomProperty))) {
        *property_name_token = kMarginBottomToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kOutlineColorProperty))) {
        *property_name_token = kOutlineColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kOutlineStyleProperty))) {
        *property_name_token = kOutlineStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kOutlineWidthProperty))) {
        *property_name_token = kOutlineWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kOverflowWrapProperty))) {
        *property_name_token = kOverflowWrapToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kPaddingRightProperty))) {
        *property_name_token = kPaddingRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTextOverflowProperty))) {
        *property_name_token = kTextOverflowToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kUnicodeRangeProperty))) {
        *property_name_token = kUnicodeRangePropertyToken;
        return true;
      }
      return false;

    case 14:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kAnimationNameProperty))) {
        *property_name_token = kAnimationNameToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kPaddingBottomProperty))) {
        *property_name_token = kPaddingBottomToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kPointerEventsProperty))) {
        *property_name_token = kPointerEventsToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTextTransformProperty))) {
        *property_name_token = kTextTransformToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kVerticalAlignProperty))) {
        *property_name_token = kVerticalAlignToken;
        return true;
      }
      return false;

    case 15:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kAnimationDelayProperty))) {
        *property_name_token = kAnimationDelayToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBackgroundSizeProperty))) {
        *property_name_token = kBackgroundSizeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTextDecorationProperty))) {
        *property_name_token = kTextDecorationToken;
        return true;
      }
      return false;

    case 16:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBackgroundColorProperty))) {
        *property_name_token = kBackgroundColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBackgroundImageProperty))) {
        *property_name_token = kBackgroundImageToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderTopColorProperty))) {
        *property_name_token = kBorderTopColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderTopStyleProperty))) {
        *property_name_token = kBorderTopStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderTopWidthProperty))) {
        *property_name_token = kBorderTopWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTransformOriginProperty))) {
        *property_name_token = kTransformOriginToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTransitionDelayProperty))) {
        *property_name_token = kTransitionDelayToken;
        return true;
      }
      return false;

    case 17:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderLeftColorProperty))) {
        *property_name_token = kBorderLeftColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderLeftStyleProperty))) {
        *property_name_token = kBorderLeftStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderLeftWidthProperty))) {
        *property_name_token = kBorderLeftWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBackgroundRepeatProperty))) {
        *property_name_token = kBackgroundRepeatToken;
        return true;
      }

      return false;

    case 18:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kAnimationDurationProperty))) {
        *property_name_token = kAnimationDurationToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderRightColorProperty))) {
        *property_name_token = kBorderRightColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderRightStyleProperty))) {
        *property_name_token = kBorderRightStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderRightWidthProperty))) {
        *property_name_token = kBorderRightWidthToken;
        return true;
      }
      return false;

    case 19:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kAnimationDirectionProperty))) {
        *property_name_token = kAnimationDirectionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kAnimationFillModeProperty))) {
        *property_name_token = kAnimationFillModeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBackgroundPositionProperty))) {
        *property_name_token = kBackgroundPositionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderBottomColorProperty))) {
        *property_name_token = kBorderBottomColorToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderBottomStyleProperty))) {
        *property_name_token = kBorderBottomStyleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderBottomWidthProperty))) {
        *property_name_token = kBorderBottomWidthToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTransitionDurationProperty))) {
        *property_name_token = kTransitionDurationToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTransitionPropertyProperty))) {
        *property_name_token = kTransitionPropertyToken;
        return true;
      }
      return false;

    case 20:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTextDecorationLineProperty))) {
        *property_name_token = kTextDecorationLineToken;
        return true;
      }
      return false;

    case 21:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kTextDecorationColorProperty))) {
        *property_name_token = kTextDecorationColorToken;
        return true;
      }
      return false;

    case 22:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderTopLeftRadiusProperty))) {
        *property_name_token = kBorderTopLeftRadiusToken;
        return true;
      }
      return false;

    case 23:
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderTopRightRadiusProperty))) {
        *property_name_token = kBorderTopRightRadiusToken;
        return true;
      }
      return false;

    case 25:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(
                              cssom::kAnimationIterationCountProperty))) {
        *property_name_token = kAnimationIterationCountToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(
                              cssom::kAnimationTimingFunctionProperty))) {
        *property_name_token = kAnimationTimingFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin,
              cssom::GetPropertyName(cssom::kBorderBottomLeftRadiusProperty))) {
        *property_name_token = kBorderBottomLeftRadiusToken;
        return true;
      }
      return false;

    case 26:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(
                              cssom::kTransitionTimingFunctionProperty))) {
        *property_name_token = kTransitionTimingFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::GetPropertyName(
                              cssom::kBorderBottomRightRadiusProperty))) {
        *property_name_token = kBorderBottomRightRadiusToken;
        return true;
      }
      return false;
  }

  return false;
}  // NOLINT(readability/fn_size)

// WARNING: every time a new value token is introduced, it should be added
//          to |identifier_token| rule in grammar.y.
bool Scanner::DetectPropertyValueToken(const TrivialStringPiece& name,
                                       Token* property_value_token) const {
  DCHECK_GT(name.size(), 0U);

  switch (name.size()) {
    case 2:
      if (IsEqualToCssIdentifier(name.begin, cssom::kAtKeywordName)) {
        *property_value_token = kAtToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kToKeywordName)) {
        *property_value_token = kToToken;
        return true;
      }
      return false;

    case 3:
      if (IsEqualToCssIdentifier(name.begin, cssom::kEndKeywordName)) {
        *property_value_token = kEndToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kPreKeywordName)) {
        *property_value_token = kPreToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRedKeywordName)) {
        *property_value_token = kRedToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kTopKeywordName)) {
        *property_value_token = kTopToken;
        return true;
      }
      return false;
    case 4:
      if (IsEqualToCssIdentifier(name.begin, cssom::kAquaKeywordName)) {
        *property_value_token = kAquaToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kAutoKeywordName)) {
        *property_value_token = kAutoToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBlueKeywordName)) {
        *property_value_token = kBlueToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBoldKeywordName)) {
        *property_value_token = kBoldToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBothKeywordName)) {
        *property_value_token = kBothToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kClipKeywordName)) {
        *property_value_token = kClipToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kEaseKeywordName)) {
        *property_value_token = kEaseToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kFromKeywordName)) {
        *property_value_token = kFromToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kGrayKeywordName)) {
        *property_value_token = kGrayToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kNavyKeywordName)) {
        *property_value_token = kNavyToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kNoneKeywordName)) {
        *property_value_token = kNoneToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kLeftKeywordName)) {
        *property_value_token = kLeftToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kLimeKeywordName)) {
        *property_value_token = kLimeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kTealKeywordName)) {
        *property_value_token = kTealToken;
        return true;
      }
      return false;

    case 5:
      if (IsEqualToCssIdentifier(name.begin, cssom::kBlackKeywordName)) {
        *property_value_token = kBlackToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBlockKeywordName)) {
        *property_value_token = kBlockToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kCoverKeywordName)) {
        *property_value_token = kCoverToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kFixedKeywordName)) {
        *property_value_token = kFixedToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kGreenKeywordName)) {
        *property_value_token = kGreenToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInsetKeywordName)) {
        *property_value_token = kInsetToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kOliveKeywordName)) {
        *property_value_token = kOliveToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRightKeywordName)) {
        *property_value_token = kRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kSerifKeywordName)) {
        *property_value_token = kSerifToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kSolidKeywordName)) {
        *property_value_token = kSolidToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kStartKeywordName)) {
        *property_value_token = kStartToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kWhiteKeywordName)) {
        *property_value_token = kWhiteToken;
        return true;
      }
      return false;

    case 6:
      if (IsEqualToCssIdentifier(name.begin, cssom::kBottomKeywordName)) {
        *property_value_token = kBottomToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kCenterKeywordName)) {
        *property_value_token = kCenterToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kCircleKeywordName)) {
        *property_value_token = kCircleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kHiddenKeywordName)) {
        *property_value_token = kHiddenToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInlineKeywordName)) {
        *property_value_token = kInlineToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kItalicKeywordName)) {
        *property_value_token = kItalicToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kLinearKeywordName)) {
        *property_value_token = kLinearToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kMaroonKeywordName)) {
        *property_value_token = kMaroonToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kMiddleKeywordName)) {
        *property_value_token = kMiddleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kNormalKeywordName)) {
        *property_value_token = kNormalToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kNoWrapKeywordName)) {
        *property_value_token = kNoWrapToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kPurpleKeywordName)) {
        *property_value_token = kPurpleToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRepeatKeywordName)) {
        *property_value_token = kRepeatToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kSilverKeywordName)) {
        *property_value_token = kSilverToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kStaticKeywordName)) {
        *property_value_token = kStaticToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kYellowKeywordName)) {
        *property_value_token = kYellowToken;
        return true;
      }
      return false;

    case 7:
      if (IsEqualToCssIdentifier(name.begin, cssom::kContainKeywordName)) {
        *property_value_token = kContainToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kCursiveKeywordName)) {
        *property_value_token = kCursiveToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kEaseInKeywordName)) {
        *property_value_token = kEaseInToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kEllipseKeywordName)) {
        *property_value_token = kEllipseToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kFantasyKeywordName)) {
        *property_value_token = kFantasyToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kFuchsiaKeywordName)) {
        *property_value_token = kFuchsiaToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInheritKeywordName)) {
        *property_value_token = kInheritToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInitialKeywordName)) {
        *property_value_token = kInitialToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kObliqueKeywordName)) {
        *property_value_token = kObliqueToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kReverseKeywordName)) {
        *property_value_token = kReverseToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kVisibleKeywordName)) {
        *property_value_token = kVisibleToken;
        return true;
      }
      return false;

    case 8:
      if (IsEqualToCssIdentifier(name.begin, cssom::kAbsoluteKeywordName)) {
        *property_value_token = kAbsoluteToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBaselineKeywordName)) {
        *property_value_token = kBaselineToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kEaseOutKeywordName)) {
        *property_value_token = kEaseOutToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kForwardsKeywordName)) {
        *property_value_token = kForwardsToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kEllipsisKeywordName)) {
        *property_value_token = kEllipsisToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInfiniteKeywordName)) {
        *property_value_token = kInfiniteToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kPreLineKeywordName)) {
        *property_value_token = kPreLineToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kPreWrapKeywordName)) {
        *property_value_token = kPreWrapToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRelativeKeywordName)) {
        *property_value_token = kRelativeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRepeatXKeywordName)) {
        *property_value_token = kRepeatXToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRepeatYKeywordName)) {
        *property_value_token = kRepeatYToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kStepEndKeywordName)) {
        *property_value_token = kStepEndToken;
        return true;
      }
      return false;

    case 9:
      if (IsEqualToCssIdentifier(name.begin, cssom::kAlternateKeywordName)) {
        *property_value_token = kAlternateToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kBackwardsKeywordName)) {
        *property_value_token = kBackwardsToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kMonospaceKeywordName)) {
        *property_value_token = kMonospaceToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kNoRepeatKeywordName)) {
        *property_value_token = kNoRepeatToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kUppercaseKeywordName)) {
        *property_value_token = kUppercaseToken;
        return true;
      }
      return false;

    case 10:
      if (IsEqualToCssIdentifier(name.begin, cssom::kBreakWordKeywordName)) {
        *property_value_token = kBreakWordToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kSansSerifKeywordName)) {
        *property_value_token = kSansSerifToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kStepStartKeywordName)) {
        *property_value_token = kStepStartToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kMonoscopicKeywordName)) {
        *property_value_token = kMonoscopicToken;
        return true;
      }
      return false;

    case 11:
      if (IsEqualToCssIdentifier(name.begin, cssom::kEaseInOutKeywordName)) {
        *property_value_token = kEaseInOutToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kTransparentKeywordName)) {
        *property_value_token = kTransparentToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kRectangularKeywordName)) {
        *property_value_token = kRectangularToken;
        return true;
      }
      return false;

    case 12:
      if (IsEqualToCssIdentifier(name.begin, cssom::kClosestSideKeywordName)) {
        *property_value_token = kClosestSideToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kLineThroughKeywordName)) {
        *property_value_token = kLineThroughToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kInlineBlockKeywordName)) {
        *property_value_token = kInlineBlockToken;
        return true;
      }
      return false;

    case 13:
      if (IsEqualToCssIdentifier(name.begin, cssom::kFarthestSideKeywordName)) {
        *property_value_token = kFarthestSideToken;
        return true;
      }
      return false;

    case 14:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kClosestCornerKeywordName)) {
        *property_value_token = kClosestCornerToken;
        return true;
      }
      return false;

    case 15:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kFarthestCornerKeywordName)) {
        *property_value_token = kFarthestCornerToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kEquirectangularKeywordName)) {
        *property_value_token = kEquirectangularToken;
        return true;
      }
      return false;

    case 17:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kAlternateReverseKeywordName)) {
        *property_value_token = kAlternateReverseToken;
        return true;
      }
      return false;

    case 23:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kStereoscopicLeftRightKeywordName)) {
        *property_value_token = kStereoscopicLeftRightToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kStereoscopicTopBottomKeywordName)) {
        *property_value_token = kStereoscopicTopBottomToken;
        return true;
      }
      return false;
  }

  return false;
}  // NOLINT(readability/fn_size)

// WARNING: every time a new value token is introduced, it should be added
//          to |identifier_token| rule in grammar.y.
bool Scanner::DetectPseudoClassNameToken(const TrivialStringPiece& name,
                                         Token* pseudo_class_name_token) const {
  DCHECK_GT(name.size(), 0U);

  switch (name.size()) {
    case 5:
      if (IsEqualToCssIdentifier(name.begin, cssom::kEmptyPseudoClassName)) {
        *pseudo_class_name_token = kEmptyToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kFocusPseudoClassName)) {
        *pseudo_class_name_token = kFocusToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kHoverPseudoClassName)) {
        *pseudo_class_name_token = kHoverToken;
        return true;
      }
      return false;
    case 6:
      if (IsEqualToCssIdentifier(name.begin, cssom::kActivePseudoClassName)) {
        *pseudo_class_name_token = kActiveToken;
        return true;
      }
      return false;
  }

  return false;
}

// WARNING: every time a new value token is introduced, it should be added
//          to |identifier_token| rule in grammar.y.
bool Scanner::DetectPseudoElementNameToken(
    const TrivialStringPiece& name, Token* pseudo_element_name_token) const {
  DCHECK_GT(name.size(), 0U);

  switch (name.size()) {
    case 5:
      if (IsEqualToCssIdentifier(name.begin, cssom::kAfterPseudoElementName)) {
        *pseudo_element_name_token = kAfterToken;
        return true;
      }
      return false;
    case 6:
      if (IsEqualToCssIdentifier(name.begin, cssom::kBeforePseudoElementName)) {
        *pseudo_element_name_token = kBeforeToken;
        return true;
      }
      return false;
  }

  return false;
}

bool Scanner::DetectSupportsToken(const TrivialStringPiece& name,
                                  Token* supports_token) const {
  DCHECK_EQ(parsing_mode_, kSupportsMode);

  if (name.size() == 2) {
    if (IsAsciiAlphaCaselessEqual(name.begin[0], 'o') &&
        IsAsciiAlphaCaselessEqual(name.begin[1], 'r')) {
      *supports_token = kSupportsOrToken;
      return true;
    }
  } else if (name.size() == 3) {
    if (IsAsciiAlphaCaselessEqual(name.begin[0], 'a') &&
        IsAsciiAlphaCaselessEqual(name.begin[1], 'n') &&
        IsAsciiAlphaCaselessEqual(name.begin[2], 'd')) {
      *supports_token = kSupportsAndToken;
      return true;
    }

    if (IsAsciiAlphaCaselessEqual(name.begin[0], 'n') &&
        IsAsciiAlphaCaselessEqual(name.begin[1], 'o') &&
        IsAsciiAlphaCaselessEqual(name.begin[2], 't')) {
      *supports_token = kSupportsNotToken;
      return true;
    }
  }
  return false;
}

bool Scanner::DetectKnownFunctionTokenAndMaybeChangeParsingMode(
    const TrivialStringPiece& name, Token* known_function_token) {
  DCHECK_GT(name.size(), 0U);

  switch (name.size()) {
    case 3:
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'c') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'u') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 'e')) {
        *known_function_token = kCueFunctionToken;
        return true;
      }
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'n') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'o') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 't')) {
        *known_function_token = kNotFunctionToken;
        return true;
      }
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'r') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'g') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 'b')) {
        *known_function_token = kRGBFunctionToken;
        return true;
      }
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'u') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'r') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 'l')) {
        *known_function_token = kUriToken;
        return true;
      }
      return false;

    case 4:
      if (IsEqualToCssIdentifier(name.begin, "calc")) {
        *known_function_token = kCalcFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "rgba")) {
        *known_function_token = kRGBAFunctionToken;
        return true;
      }
      return false;

    case 5:
      if (IsEqualToCssIdentifier(name.begin, "local")) {
        *known_function_token = kLocalFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "scale")) {
        *known_function_token = kScaleFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "steps")) {
        *known_function_token = kStepsFunctionToken;
        return true;
      }
      return false;

    case 6:
      if (IsEqualToCssIdentifier(name.begin, "format")) {
        *known_function_token = kFormatFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "matrix")) {
        *known_function_token = kMatrixFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "rotate")) {
        *known_function_token = kRotateFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "scalex")) {
        *known_function_token = kScaleXFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "scaley")) {
        *known_function_token = kScaleYFunctionToken;
        return true;
      }
      return false;

    case 8:
      if (IsEqualToCssIdentifier(name.begin, "matrix3d")) {
        *known_function_token = kMatrix3dFunctionToken;
        return true;
      }
      return false;

    case 9:
      if (IsEqualToCssIdentifier(name.begin, "translate")) {
        *known_function_token = kTranslateFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "nth-child")) {
        parsing_mode_ = kNthChildMode;
        *known_function_token = kNthChildFunctionToken;
        return true;
      }
      return false;

    case 10:
      if (IsEqualToCssIdentifier(name.begin, "translatex")) {
        *known_function_token = kTranslateXFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "translatey")) {
        *known_function_token = kTranslateYFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "translatez")) {
        *known_function_token = kTranslateZFunctionToken;
        return true;
      }
      return false;

    case 11:
      if (IsEqualToCssIdentifier(name.begin, "map-to-mesh")) {
        *known_function_token = kMapToMeshFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "nth-of-type")) {
        parsing_mode_ = kNthChildMode;
        *known_function_token = kNthOfTypeFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "-cobalt-mtm")) {
        *known_function_token = kCobaltMtmFunctionToken;
        return true;
      }
      return false;

    case 12:
      if (IsEqualToCssIdentifier(name.begin, "cubic-bezier")) {
        *known_function_token = kCubicBezierFunctionToken;
        return true;
      }
      return false;

    case 14:
      if (IsEqualToCssIdentifier(name.begin, "nth-last-child")) {
        parsing_mode_ = kNthChildMode;
        *known_function_token = kNthLastChildFunctionToken;
        return true;
      }
      return false;

    case 15:
      if (IsEqualToCssIdentifier(name.begin, "linear-gradient")) {
        *known_function_token = kLinearGradientFunctionToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, "radial-gradient")) {
        *known_function_token = kRadialGradientFunctionToken;
        return true;
      }
      return false;

    case 16:
      if (IsEqualToCssIdentifier(name.begin, "nth-last-of-type")) {
        parsing_mode_ = kNthChildMode;
        *known_function_token = kNthLastOfTypeFunctionToken;
        return true;
      }
      return false;
  }

  return false;
}

bool Scanner::DetectMediaQueryToken(
    const TrivialStringPiece& name, Token* media_query_token,
    cssom::MediaFeatureName* media_feature_name) const {
  DCHECK_EQ(parsing_mode_, kMediaQueryMode);
  *media_feature_name = cssom::kInvalidFeature;
  switch (name.size()) {
    case 2:
      if (IsAsciiAlphaCaselessEqual(name.begin[0],
                                    cssom::kTVMediaTypeName[0]) &&
          IsAsciiAlphaCaselessEqual(name.begin[1],
                                    cssom::kTVMediaTypeName[1])) {
        *media_query_token = kTVMediaTypeToken;
        return true;
      }
      return false;

    case 3:
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'a')) {
        if (IsAsciiAlphaCaselessEqual(name.begin[1], 'l') &&
            IsAsciiAlphaCaselessEqual(name.begin[2], 'l')) {
          *media_query_token = kAllMediaTypeToken;
          return true;
        }

        if (IsAsciiAlphaCaselessEqual(name.begin[1], 'n') &&
            IsAsciiAlphaCaselessEqual(name.begin[2], 'd')) {
          *media_query_token = kMediaAndToken;
          return true;
        }
        return false;
      }

      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'n') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'o') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 't')) {
        *media_query_token = kMediaNotToken;
        return true;
      }
      return false;

    case 4:
      if (IsAsciiAlphaCaselessEqual(name.begin[0], 'o') &&
          IsAsciiAlphaCaselessEqual(name.begin[1], 'n') &&
          IsAsciiAlphaCaselessEqual(name.begin[2], 'l') &&
          IsAsciiAlphaCaselessEqual(name.begin[3], 'y')) {
        *media_query_token = kMediaOnlyToken;
        return true;
      }

      if (IsAsciiAlphaCaselessEqual(name.begin[0],
                                    cssom::kGridMediaFeatureName[0]) &&
          IsAsciiAlphaCaselessEqual(name.begin[1],
                                    cssom::kGridMediaFeatureName[1]) &&
          IsAsciiAlphaCaselessEqual(name.begin[2],
                                    cssom::kGridMediaFeatureName[2]) &&
          IsAsciiAlphaCaselessEqual(name.begin[3],
                                    cssom::kGridMediaFeatureName[3])) {
        *media_feature_name = cssom::kGridMediaFeature;
        *media_query_token = kZeroOrOneMediaFeatureTypeToken;
        return true;
      }

      if (IsAsciiAlphaCaselessEqual(name.begin[0],
                                    cssom::kScanMediaFeatureName[0]) &&
          IsAsciiAlphaCaselessEqual(name.begin[1],
                                    cssom::kScanMediaFeatureName[1]) &&
          IsAsciiAlphaCaselessEqual(name.begin[2],
                                    cssom::kScanMediaFeatureName[2]) &&
          IsAsciiAlphaCaselessEqual(name.begin[3],
                                    cssom::kScanMediaFeatureName[3])) {
        *media_feature_name = cssom::kScanMediaFeature;
        *media_query_token = kScanMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 5:
      if (IsEqualToCssIdentifier(name.begin, cssom::kColorMediaFeatureName)) {
        *media_feature_name = cssom::kColorMediaFeature;
        *media_query_token = kNonNegativeIntegerMediaFeatureTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kWidthMediaFeatureName)) {
        *media_feature_name = cssom::kWidthMediaFeature;
        *media_query_token = kLengthMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 6:
      if (IsEqualToCssIdentifier(name.begin, cssom::kScreenMediaTypeName)) {
        *media_query_token = kScreenMediaTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin, cssom::kHeightMediaFeatureName)) {
        *media_feature_name = cssom::kHeightMediaFeature;
        *media_query_token = kLengthMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 8:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::kPortraitMediaFeatureKeywordValueName)) {
        *media_query_token = kPortraitMediaFeatureKeywordValueToken;
        return true;
      }
      return false;

    case 9:
      if (IsEqualToCssIdentifier(
              name.begin, cssom::kInterlaceMediaFeatureKeywordValueName)) {
        *media_query_token = kInterlaceMediaFeatureKeywordValueToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::kLandscapeMediaFeatureKeywordValueName)) {
        *media_query_token = kLandscapeMediaFeatureKeywordValueToken;
        return true;
      }
      return false;

    case 10:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kMonochromeMediaFeatureName)) {
        *media_feature_name = cssom::kMonochromeMediaFeature;
        *media_query_token = kNonNegativeIntegerMediaFeatureTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kResolutionMediaFeatureName)) {
        *media_feature_name = cssom::kResolutionMediaFeature;
        *media_query_token = kResolutionMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 11:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kColorIndexMediaFeatureName)) {
        *media_feature_name = cssom::kColorIndexMediaFeature;
        *media_query_token = kNonNegativeIntegerMediaFeatureTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kOrientationMediaFeatureName)) {
        *media_feature_name = cssom::kOrientationMediaFeature;
        *media_query_token = kOrientationMediaFeatureTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(
              name.begin, cssom::kProgressiveMediaFeatureKeywordValueName)) {
        *media_query_token = kProgressiveMediaFeatureKeywordValueToken;
        return true;
      }
      return false;

    case 12:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kAspectRatioMediaFeatureName)) {
        *media_feature_name = cssom::kAspectRatioMediaFeature;
        *media_query_token = kRatioMediaFeatureTypeToken;
        return true;
      }
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kDeviceWidthMediaFeatureName)) {
        *media_feature_name = cssom::kDeviceWidthMediaFeature;
        *media_query_token = kLengthMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 13:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kDeviceHeightMediaFeatureName)) {
        *media_feature_name = cssom::kDeviceHeightMediaFeature;
        *media_query_token = kLengthMediaFeatureTypeToken;
        return true;
      }
      return false;

    case 19:
      if (IsEqualToCssIdentifier(name.begin,
                                 cssom::kDeviceAspectRatioMediaFeatureName)) {
        *media_feature_name = cssom::kDeviceAspectRatioMediaFeature;
        *media_query_token = kRatioMediaFeatureTypeToken;
        return true;
      }
      return false;
  }
  return false;
}

bool Scanner::TryScanUri(TrivialStringPiece* uri) {
  const char* uri_start;
  const char* uri_end;
  char quote;
  if (!FindUri(&uri_start, &uri_end, &quote)) {
    return false;
  }

  input_iterator_ = uri_start;
  if (UNLIKELY(!TryScanAndMaybeCopyUri<false>(quote, uri, NULL))) {
    CHECK(TryScanAndMaybeCopyUri<true>(quote, uri,
                                       string_pool_->AllocateString()));
  }
  input_iterator_ = uri_end;
  return true;
}

bool Scanner::FindUri(const char** uri_start, const char** uri_end,
                      char* quote) const {
  *uri_start = SkipWhiteSpace(input_iterator_);

  if (**uri_start == '"' || **uri_start == '\'') {
    *quote = **uri_start;
    ++*uri_start;
    if (!CheckAndSkipString(*uri_start, *quote, uri_end)) {
      return false;
    }
  } else {
    *quote = 0;
    *uri_end = *uri_start;
    while (IsUriLetter(**uri_end)) {
      if (LIKELY(**uri_end != '\\')) {
        ++*uri_end;
      } else {
        if (!CheckAndSkipEscape(*uri_end, uri_end)) {
          return false;
        }
      }
    }
  }

  *uri_end = SkipWhiteSpace(*uri_end);
  return **uri_end == ')' || **uri_end == '\0';
}

template <bool copy>
bool Scanner::TryScanAndMaybeCopyUri(char quote, TrivialStringPiece* uri,
                                     std::string* uri_copy) {
  if (quote) {
    DCHECK(quote == '"' || quote == '\'');
    return TryScanAndMaybeCopyString<copy>(quote, uri, uri_copy);
  }

  if (!copy) {
    uri->begin = input_iterator_;
  }

  while (IsUriLetter(*input_iterator_)) {
    if (LIKELY(*input_iterator_ != '\\')) {
      if (copy) {
        uri_copy->push_back(*input_iterator_);
      }
      ++input_iterator_;
    } else {
      if (!copy) {
        input_iterator_ = uri->begin;
        return false;
      }
      icu::UnicodeString(ScanEscape()).toUTF8String(*uri_copy);
    }
  }

  if (copy) {
    uri->begin = uri_copy->c_str();
    uri->end = uri->begin + uri_copy->size();
  } else {
    // uri->begin was initialized in the beginning of the method.
    uri->end = input_iterator_;
  }
  return true;
}

void Scanner::ScanString(char quote, TrivialStringPiece* value) {
  if (UNLIKELY(!TryScanAndMaybeCopyString<false>(quote, value, NULL))) {
    CHECK(TryScanAndMaybeCopyString<true>(quote, value,
                                          string_pool_->AllocateString()));
  }
}

template <bool copy>
bool Scanner::TryScanAndMaybeCopyString(char quote, TrivialStringPiece* value,
                                        std::string* value_copy) {
  if (!copy) {
    value->begin = input_iterator_;
  }

  while (true) {
    if (UNLIKELY(*input_iterator_ == quote)) {
      // String parsing is done.
      if (!copy) {
        value->end = input_iterator_;
      }
      ++input_iterator_;
      break;
    }
    if (UNLIKELY(*input_iterator_ == '\0')) {
      // String parsing is done, but don't advance pointer if at the end
      // of input.
      if (!copy) {
        value->end = input_iterator_;
      }
      break;
    }
    DCHECK(*input_iterator_ > '\r' ||
           (*input_iterator_ < '\n' && *input_iterator_) ||
           *input_iterator_ == '\v');

    if (LIKELY(input_iterator_[0] != '\\')) {
      if (copy) {
        value_copy->push_back(*input_iterator_);
      }
      ++input_iterator_;
    } else if (input_iterator_[1] == '\n' || input_iterator_[1] == '\f') {
      input_iterator_ += 2;
    } else if (input_iterator_[1] == '\r') {
      input_iterator_ += input_iterator_[2] == '\n' ? 3 : 2;
    } else {
      if (!copy) {
        input_iterator_ = value->begin;
        return false;
      }
      icu::UnicodeString(ScanEscape()).toUTF8String(*value_copy);
    }
  }

  if (copy) {
    value->begin = value_copy->c_str();
    value->end = value->begin + value_copy->size();
  }
  return true;
}

bool Scanner::TryScanNthChild(TrivialStringPiece* nth) {
  nth->begin = SkipWhiteSpace(input_iterator_);
  nth->end = nth->begin;

  while (IsAsciiDigit(*nth->end)) {
    ++nth->end;
  }
  if (IsAsciiAlphaCaselessEqual(*nth->end, 'n')) {
    ++nth->end;
    input_iterator_ = nth->end;
    return true;
  }
  return false;
}

bool Scanner::TryScanNthChildExtra(TrivialStringPiece* nth) {
  nth->begin = SkipWhiteSpace(input_iterator_);
  nth->end = nth->begin;

  if (*nth->end != '+' && *nth->end != '-') {
    return false;
  }

  nth->end = SkipWhiteSpace(nth->end + 1);
  if (!IsAsciiDigit(*nth->end)) {
    return false;
  }

  do {
    ++nth->end;
  } while (IsAsciiDigit(*nth->end));

  input_iterator_ = nth->end;
  return true;
}

bool Scanner::DetectUnitToken(const TrivialStringPiece& unit,
                              Token* token) const {
  std::size_t length(unit.size());
  DCHECK_GT(length, 0U);

  switch (ToAsciiLowerUnchecked(unit.begin[0])) {
    case 'c':
      if (length == 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'm')) {
        *token = kCentimetersToken;
        return true;
      }
      if (length == 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'h')) {
        *token = kZeroGlyphWidthsAkaChToken;
        return true;
      }
      return false;

    case 'd':
      if (length == 3 &&
          IsAsciiAlphaCaselessEqual(unit.begin[1], 'e') &&
          IsAsciiAlphaCaselessEqual(unit.begin[2], 'g')) {
        *token = kDegreesToken;
        return true;
      }
      if (length > 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'p')) {
        if (length == 4) {
          // There is a discussion about the name of this unit on www-style.
          // Keep this compile time guard in place until that is resolved.
          // http://lists.w3.org/Archives/Public/www-style/2012May/0915.html
          if (IsAsciiAlphaCaselessEqual(unit.begin[2], 'p') &&
              IsAsciiAlphaCaselessEqual(unit.begin[3], 'x')) {
            *token = kDotsPerPixelToken;
            return true;
          }
          if (IsAsciiAlphaCaselessEqual(unit.begin[2], 'c') &&
              IsAsciiAlphaCaselessEqual(unit.begin[3], 'm')) {
            *token = kDotsPerCentimeterToken;
            return true;
          }
        } else if (length == 3 &&
                   IsAsciiAlphaCaselessEqual(unit.begin[2], 'i')) {
          *token = kDotsPerInchToken;
          return true;
        }
      }
      return false;

    case 'e':
      if (length == 2) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'm')) {
          *token = kFontSizesAkaEmToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'x')) {
          *token = kXHeightsAkaExToken;
          return true;
        }
      }
      return false;

    case 'f':
      if (length == 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'r')) {
        *token = kFractionsToken;
        return true;
      }
      return false;
    case 'g':
      if (length == 4 &&
          IsAsciiAlphaCaselessEqual(unit.begin[1], 'r') &&
          IsAsciiAlphaCaselessEqual(unit.begin[2], 'a') &&
          IsAsciiAlphaCaselessEqual(unit.begin[3], 'd')) {
        *token = kGradiansToken;
        return true;
      }
      return false;

    case 'h':
      if (length == 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'z')) {
        *token = kHertzToken;
        return true;
      }
      return false;

    case 'i':
      if (length == 2 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'n')) {
        *token = kInchesToken;
        return true;
      }
      return false;

    case 'k':
      if (length == 3 &&
          IsAsciiAlphaCaselessEqual(unit.begin[1], 'h') &&
          IsAsciiAlphaCaselessEqual(unit.begin[2], 'z')) {
        *token = kKilohertzToken;
        return true;
      }
      return false;

    case 'm':
      if (length == 2) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'm')) {
          *token = kMillimetersToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 's')) {
          *token = kMillisecondsToken;
          return true;
        }
      }
      return false;

    case 'p':
      if (length == 2) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'x')) {
          *token = kPixelsToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 't')) {
          *token = kPointsToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'c')) {
          *token = kPicasToken;
          return true;
        }
      }
      return false;

    case 'r':
      if (length == 3) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'a') &&
            IsAsciiAlphaCaselessEqual(unit.begin[2], 'd')) {
          *token = kRadiansToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'e') &&
            IsAsciiAlphaCaselessEqual(unit.begin[2], 'm')) {
          *token = kRootElementFontSizesAkaRemToken;
          return true;
        }
      }
      return false;

    case 's':
      if (length == 1) {
        *token = kSecondsToken;
        return true;
      }
      return false;

    case 't':
      if (length == 4 &&
          IsAsciiAlphaCaselessEqual(unit.begin[1], 'u') &&
          IsAsciiAlphaCaselessEqual(unit.begin[2], 'r') &&
          IsAsciiAlphaCaselessEqual(unit.begin[3], 'n')) {
        *token = kTurnsToken;
        return true;
      }
      return false;
    case 'v':
      if (length == 2) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'w')) {
          *token = kViewportWidthPercentsAkaVwToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[1], 'h')) {
          *token = kViewportHeightPercentsAkaVhToken;
          return true;
        }
      }
      if (length == 4 && IsAsciiAlphaCaselessEqual(unit.begin[1], 'm')) {
        if (IsAsciiAlphaCaselessEqual(unit.begin[2], 'i') &&
            IsAsciiAlphaCaselessEqual(unit.begin[3], 'n')) {
          *token = kViewportSmallerSizePercentsAkaVminToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(unit.begin[2], 'a') &&
            IsAsciiAlphaCaselessEqual(unit.begin[3], 'x')) {
          *token = kViewportLargerSizePercentsAkaVmaxToken;
          return true;
        }
      }
      return false;

    default:
      return false;
  }
}

inline bool Scanner::DetectAtTokenAndMaybeChangeParsingMode(
    const TrivialStringPiece& name, bool has_escape, Token* at_token) {
  std::size_t length(name.size());
  DCHECK_EQ(name.begin[0], '@');
  DCHECK_GE(length, 2U);

  // charset, font-face, import, media, namespace, page, supports,
  // -webkit-keyframes, and -webkit-mediaquery are not affected by hasEscape.
  switch (ToAsciiLowerUnchecked(name.begin[1])) {
    case 'b':
      if (has_escape) {
        return false;
      }

      switch (length) {
        case 12:
          if (IsEqualToCssIdentifier(name.begin + 2, "ottom-left")) {
            *at_token = kBottomLeftToken;
            return true;
          }
          return false;

        case 13:
          if (IsEqualToCssIdentifier(name.begin + 2, "ottom-right")) {
            *at_token = kBottomRightToken;
            return true;
          }
          return false;

        case 14:
          if (IsEqualToCssIdentifier(name.begin + 2, "ottom-center")) {
            *at_token = kBottomCenterToken;
            return true;
          }
          return false;

        case 19:
          if (IsEqualToCssIdentifier(name.begin + 2, "ottom-left-corner")) {
            *at_token = kBottomLeftCornerToken;
            return true;
          }
          return false;

        case 20:
          if (IsEqualToCssIdentifier(name.begin + 2, "ottom-right-corner")) {
            *at_token = kBottomRightCornerToken;
            return true;
          }
          return false;
      }
      return false;

    case 'c':
      if (length == 8 && IsEqualToCssIdentifier(name.begin + 2, "harset")) {
        *at_token = kCharsetToken;
        return true;
      }
      return false;

    case 'f':
      if (length == 10 && IsEqualToCssIdentifier(name.begin + 2, "ont-face")) {
        *at_token = kFontFaceToken;
        return true;
      }
      return false;

    case 'i':
      if (length == 7 && IsEqualToCssIdentifier(name.begin + 2, "mport")) {
        parsing_mode_ = kMediaQueryMode;
        *at_token = kImportToken;
        return true;
      }
      return false;

    case 'k':
      if (length == 10 && IsEqualToCssIdentifier(name.begin + 2, "eyframes")) {
        *at_token = kKeyframesToken;
        return true;
      }
      return false;

    case 'l':
      if (has_escape) {
        return false;
      }

      if (length == 9) {
        if (IsEqualToCssIdentifier(name.begin + 2, "eft-top")) {
          *at_token = kLeftTopToken;
          return true;
        }
      } else if (length == 12) {
        // Checking the last character first could further reduce
        // the possibile cases.
        if (IsAsciiAlphaCaselessEqual(name.begin[11], 'e') &&
            IsEqualToCssIdentifier(name.begin + 2, "eft-middl")) {
          *at_token = kLeftMiddleToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(name.begin[11], 'm') &&
            IsEqualToCssIdentifier(name.begin + 2, "eft-botto")) {
          *at_token = kLeftBottomToken;
          return true;
        }
      }
      return false;

    case 'm':
      if (length == 6 && IsEqualToCssIdentifier(name.begin + 2, "edia")) {
        parsing_mode_ = kMediaQueryMode;
        *at_token = kMediaToken;
        return true;
      }
      return false;

    case 'n':
      if (length == 10 && IsEqualToCssIdentifier(name.begin + 2, "amespace")) {
        *at_token = kNamespaceToken;
        return true;
      }
      return false;

    case 'p':
      if (length == 5 && IsEqualToCssIdentifier(name.begin + 2, "age")) {
        *at_token = kPageToken;
        return true;
      }
      return false;

    case 'r':
      if (has_escape) {
        return false;
      }

      if (length == 10) {
        if (IsEqualToCssIdentifier(name.begin + 2, "ight-top")) {
          *at_token = kRightTopToken;
          return true;
        }
      } else if (length == 13) {
        // Checking the last character first could further reduce
        // the possibile cases.
        if (IsAsciiAlphaCaselessEqual(name.begin[12], 'e') &&
            IsEqualToCssIdentifier(name.begin + 2, "ight-middl")) {
          *at_token = kRightMiddleToken;
          return true;
        }
        if (IsAsciiAlphaCaselessEqual(name.begin[12], 'm') &&
            IsEqualToCssIdentifier(name.begin + 2, "ight-botto")) {
          *at_token = kRightBottomToken;
          return true;
        }
      }
      return false;

    case 's':
      if (length == 9 && IsEqualToCssIdentifier(name.begin + 2, "upports")) {
        parsing_mode_ = kSupportsMode;
        *at_token = kSupportsToken;
        return true;
      }
      return false;

    case 't':
      if (has_escape) {
        return false;
      }

      switch (length) {
        case 9:
          if (IsEqualToCssIdentifier(name.begin + 2, "op-left")) {
            *at_token = kTopLeftToken;
            return true;
          }
          return false;

        case 10:
          if (IsEqualToCssIdentifier(name.begin + 2, "op-right")) {
            *at_token = kTopRightToken;
            return true;
          }
          return false;

        case 11:
          if (IsEqualToCssIdentifier(name.begin + 2, "op-center")) {
            *at_token = kTopCenterToken;
            return true;
          }
          return false;

        case 16:
          if (IsEqualToCssIdentifier(name.begin + 2, "op-left-corner")) {
            *at_token = kTopLeftCornerToken;
            return true;
          }
          return false;

        case 17:
          if (IsEqualToCssIdentifier(name.begin + 2, "op-right-corner")) {
            *at_token = kTopRightCornerToken;
            return true;
          }
          return false;
      }
      return false;

    case '-':
      *at_token = kOtherBrowserAtBlockToken;
      return true;
  }
  return false;
}

}  // namespace css_parser
}  // namespace cobalt
