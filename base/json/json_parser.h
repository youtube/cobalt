// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_JSON_JSON_PARSER_H_
#define BASE_JSON_JSON_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/json/json_common.h"
#include "base/strings/string_piece.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

class Value;

namespace internal {

class JSONParserTest;

// The implementation behind the JSONReader interface. This class is not meant
// to be used directly; it encapsulates logic that need not be exposed publicly.
//
// This parser guarantees O(n) time through the input string. Iteration happens
// on the byte level, with the functions ConsumeChars() and ConsumeChar(). The
// conversion from byte to JSON token happens without advancing the parser in
// GetNextToken/ParseToken, that is tokenization operates on the current parser
// position without advancing.
//
// Built on top of these are a family of Consume functions that iterate
// internally. Invariant: on entry of a Consume function, the parser is wound
// to the first byte of a valid JSON token. On exit, it is on the first byte
// after the token that was just consumed, which would likely be the first byte
// of the next token.
class BASE_EXPORT JSONParser {
 public:
  // Error codes during parsing.
  enum JsonParseError {
    JSON_NO_ERROR = base::ValueDeserializer::kErrorCodeNoError,
    JSON_SYNTAX_ERROR = base::ValueDeserializer::kErrorCodeInvalidFormat,
    JSON_INVALID_ESCAPE,
    JSON_UNEXPECTED_TOKEN,
    JSON_TRAILING_COMMA,
    JSON_TOO_MUCH_NESTING,
    JSON_UNEXPECTED_DATA_AFTER_ROOT,
    JSON_UNSUPPORTED_ENCODING,
    JSON_UNQUOTED_DICTIONARY_KEY,
    JSON_UNREPRESENTABLE_NUMBER,
    JSON_PARSE_ERROR_COUNT
  };

  // String versions of parse error codes.
  static const char kSyntaxError[];
  static const char kInvalidEscape[];
  static const char kUnexpectedToken[];
  static const char kTrailingComma[];
  static const char kTooMuchNesting[];
  static const char kUnexpectedDataAfterRoot[];
  static const char kUnsupportedEncoding[];
  static const char kUnquotedDictionaryKey[];
  static const char kUnrepresentableNumber[];

  explicit JSONParser(int options, size_t max_depth = kAbsoluteMaxDepth);

  JSONParser(const JSONParser&) = delete;
  JSONParser& operator=(const JSONParser&) = delete;

  ~JSONParser();

  // Parses the input string according to the set options and returns the
  // result as a Value.
  // Wrap this in base::FooValue::From() to check the Value is of type Foo and
  // convert to a FooValue at the same time.
  absl::optional<Value> Parse(StringPiece input);

  // Returns the error code.
  JsonParseError error_code() const;

  // Returns the human-friendly error message.
  std::string GetErrorMessage() const;

  // Returns the error line number if parse error happened. Otherwise always
  // returns 0.
  int error_line() const;

  // Returns the error column number if parse error happened. Otherwise always
  // returns 0.
  int error_column() const;

 private:
  enum Token {
    T_OBJECT_BEGIN,           // {
    T_OBJECT_END,             // }
    T_ARRAY_BEGIN,            // [
    T_ARRAY_END,              // ]
    T_STRING,
    T_NUMBER,
    T_BOOL_TRUE,              // true
    T_BOOL_FALSE,             // false
    T_NULL,                   // null
    T_LIST_SEPARATOR,         // ,
    T_OBJECT_PAIR_SEPARATOR,  // :
    T_END_OF_INPUT,
    T_INVALID_TOKEN,
  };

  // A helper class used for parsing strings. One optimization performed is to
  // create base::Value with a StringPiece to avoid unnecessary std::string
  // copies. This is not possible if the input string needs to be decoded from
  // UTF-16 to UTF-8, or if an escape sequence causes characters to be skipped.
  // This class centralizes that logic.
  class StringBuilder {
   public:
    // Empty constructor. Used for creating a builder with which to assign to.
    StringBuilder();

    // |pos| is the beginning of an input string, excluding the |"|.
    explicit StringBuilder(const char* pos);

    ~StringBuilder();

    StringBuilder& operator=(StringBuilder&& other);

    // Appends the Unicode code point |point| to the string, either by
    // increasing the |length_| of the string if the string has not been
    // converted, or by appending the UTF8 bytes for the code point.
    void Append(base_icu::UChar32 point);

    // Converts the builder from its default StringPiece to a full std::string,
    // performing a copy. Once a builder is converted, it cannot be made a
    // StringPiece again.
    void Convert();

    // Returns the builder as a string, invalidating all state. This allows
    // the internal string buffer representation to be destructively moved
    // in cases where the builder will not be needed any more.
    std::string DestructiveAsString();

   private:
    // The beginning of the input string.
    const char* pos_;

    // Number of bytes in |pos_| that make up the string being built.
    size_t length_;

    // The copied string representation. Will be unset until Convert() is
    // called.
    absl::optional<std::string> string_;
  };

  // Returns the next |count| bytes of the input stream, or nullopt if fewer
  // than |count| bytes remain.
  absl::optional<StringPiece> PeekChars(size_t count);

  // Calls PeekChars() with a |count| of 1.
  absl::optional<char> PeekChar();

  // Returns the next |count| bytes of the input stream, or nullopt if fewer
  // than |count| bytes remain, and advances the parser position by |count|.
  absl::optional<StringPiece> ConsumeChars(size_t count);

  // Calls ConsumeChars() with a |count| of 1.
  absl::optional<char> ConsumeChar();

  // Returns a pointer to the current character position.
  const char* pos();

  // Skips over whitespace and comments to find the next token in the stream.
  // This does not advance the parser for non-whitespace or comment chars.
  Token GetNextToken();

  // Consumes whitespace characters and comments until the next non-that is
  // encountered.
  void EatWhitespaceAndComments();
  // Helper function that consumes a comment, assuming that the parser is
  // currently wound to a '/'.
  bool EatComment();

  // Calls GetNextToken() and then ParseToken().
  absl::optional<Value> ParseNextToken();

  // Takes a token that represents the start of a Value ("a structural token"
  // in RFC terms) and consumes it, returning the result as a Value.
  absl::optional<Value> ParseToken(Token token);

  // Assuming that the parser is currently wound to '{', this parses a JSON
  // object into a Value.
  absl::optional<Value> ConsumeDictionary();

  // Assuming that the parser is wound to '[', this parses a JSON list into a
  // Value.
  absl::optional<Value> ConsumeList();

  // Calls through ConsumeStringRaw and wraps it in a value.
  absl::optional<Value> ConsumeString();

  // Assuming that the parser is wound to a double quote, this parses a string,
  // decoding any escape sequences and converts UTF-16 to UTF-8. Returns true on
  // success and places result into |out|. Returns false on failure with
  // error information set.
  bool ConsumeStringRaw(StringBuilder* out);
  // Helper function for ConsumeStringRaw() that consumes the next four or 10
  // bytes (parser is wound to the first character of a HEX sequence, with the
  // potential for consuming another \uXXXX for a surrogate). Returns true on
  // success and places the code point |out_code_point|, and false on failure.
  bool DecodeUTF16(base_icu::UChar32* out_code_point);

  // Assuming that the parser is wound to the start of a valid JSON number,
  // this parses and converts it to either an int or double value.
  absl::optional<Value> ConsumeNumber();
  // Helper that reads characters that are ints. Returns true if a number was
  // read and false on error.
  bool ReadInt(bool allow_leading_zeros);

  // Consumes the literal values of |true|, |false|, and |null|, assuming the
  // parser is wound to the first character of any of those.
  absl::optional<Value> ConsumeLiteral();

  // Helper function that returns true if the byte squence |match| can be
  // consumed at the current parser position. Returns false if there are fewer
  // than |match|-length bytes or if the sequence does not match, and the
  // parser state is unchanged.
  bool ConsumeIfMatch(StringPiece match);

  // Sets the error information to |code| at the current column, based on
  // |index_| and |index_last_line_|, with an optional positive/negative
  // adjustment by |column_adjust|.
  void ReportError(JsonParseError code, int column_adjust);

  // Given the line and column number of an error, formats one of the error
  // message contants from json_reader.h for human display.
  static std::string FormatErrorMessage(int line, int column,
                                        const std::string& description);

  // base::JSONParserOptions that control parsing.
  const int options_;

  // Maximum depth to parse.
  const size_t max_depth_;

  // The input stream being parsed. Note: Not guaranteed to NUL-terminated.
  StringPiece input_;

  // The index in the input stream to which the parser is wound.
  size_t index_;

  // The number of times the parser has recursed (current stack depth).
  size_t stack_depth_;

  // The line number that the parser is at currently.
  int line_number_;

  // The last value of |index_| on the previous line.
  size_t index_last_line_;

  // Error information.
  JsonParseError error_code_;
  int error_line_;
  int error_column_;

  friend class JSONParserTest;
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, NextChar);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ConsumeDictionary);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ConsumeList);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ConsumeString);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ConsumeLiterals);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ConsumeNumbers);
  FRIEND_TEST_ALL_PREFIXES(JSONParserTest, ErrorMessages);
};

// Used when decoding and an invalid utf-8 sequence is encountered.
BASE_EXPORT extern const char kUnicodeReplacementString[];

}  // namespace internal
}  // namespace base

#endif  // BASE_JSON_JSON_PARSER_H_
