// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSS_PARSER_SCANNER_H_
#define COBALT_CSS_PARSER_SCANNER_H_

#include <deque>
#include <stack>
#include <string>

#include "base/basictypes.h"
#include "cobalt/css_parser/grammar.h"
#include "cobalt/cssom/media_feature_names.h"
#include "third_party/icu/source/common/unicode/umachine.h"

namespace cobalt {
namespace css_parser {

class StringPool;
typedef int Token;
struct TrivialStringPiece;

// Being initialized with a zero-terminated input string in UTF-8 encoding,
// a scanner produces a series of CSS tokens, ending with kEndOfFileToken.
// The scanner is designed to be resilient to errors in the input and is
// supposed to reach the end of the input in any case.
//
// The scanner tries to minimize the amount of string copying, with most
// returned strings referencing the input. If a string copy is needed because
// a string contains an escape sequence, the copy will be allocated on an
// injected string pool.
//
// This implementation is an adaptation of the hand-written CSS scanner from
// WebKit that has shown a 2x performance improvement over the one generated
// by Flex. See https://bugs.webkit.org/show_bug.cgi?id=70107.
//
// Detailed information on the CSS grammar can be found at
// https://www.w3.org/TR/css-syntax-3/.
class Scanner {
 public:
  Scanner(const char* input_iterator, StringPool* string_pool);

  // Recognizes a token under the input iterator.
  // Returns a token, initializes its value (if any) and advances the input
  // iterator accordingly. Guaranteed to succeed because unrecognized characters
  // are returned as is (and are expected to trigger a parser error).
  Token Scan(TokenValue* token_value, YYLTYPE* token_location);

  // Token injection, used by the parser to choose an entry point.
  void PrependToken(Token token);
  bool DetectPropertyNameToken(const std::string& property_name,
                               Token* property_name_token) const;

 private:
  // Parsing modes are the equivalent of Flex start conditions:
  // http://flex.sourceforge.net/manual/Start-Conditions.html
  enum ParsingMode {
    // A scanner is initialized in this mode and spends most of the time in it.
    kNormalMode,

    // A scanner enters this mode after seeing "@media" or "@import" and exits
    // it after ";" or "{". The mode is needed to recognize "and", "not", "only"
    // as keywords (which are treated as identifiers or function names in
    // normal mode).
    kMediaQueryMode,

    // A scanner enters this mode after seeing "@supports" and exits it after
    // ";" or "{". The mode is needed to recognize "and", "not", "or" as
    // keywords (which are treated as identifiers or function names in normal
    // mode).
    kSupportsMode,

    // A scanner enters this mode after seeing "nth-child" or similar function
    // and exits it after ")". The mode is needed to recognize "an+b"
    // microsyntax: https://www.w3.org/TR/css-syntax-3/#anb
    kNthChildMode
  };

  // ScanFrom*() methods are called by a main scanner loop when an input
  // iterator is pointing at a character of a corresponding type. These methods
  // are guaranteed to succeed. They always return a token, initialize its value
  // (if any) and advance the input iterator accordingly.
  Token ScanFromCaselessU(TokenValue* token_value);
  Token ScanFromIdentifierStart(TokenValue* token_value);
  Token ScanFromDot(TokenValue* token_value);
  Token ScanFromNumber(TokenValue* token_value);
  Token ScanFromDash(TokenValue* token_value);
  Token ScanFromOtherCharacter();
  Token ScanFromNull();  // Does not advance input_iterator_ beyond
                         // the end of input.
  Token ScanFromWhitespace();
  Token ScanFromEndMediaQueryOrSupports();
  Token ScanFromEndNthChild();
  Token ScanFromQuote(TokenValue* token_value);
  Token ScanFromExclamationMark(TokenValue* token_value);
  Token ScanFromHashmark(TokenValue* token_value);
  Token ScanFromSlash();
  Token ScanFromDollar();
  Token ScanFromAsterisk();
  Token ScanFromPlus(TokenValue* token_value);
  Token ScanFromLess();
  Token ScanFromAt(TokenValue* token_value);
  Token ScanFromBackSlash(TokenValue* token_value);
  Token ScanFromXor();
  Token ScanFromVerticalBar();
  Token ScanFromTilde();

  // Scan*() methods are much like ScanFrom*() except they recognize only one
  // type of tokens.
  //
  // TryScan*() methods also recognize only one type of tokens but are not
  // guaranteed to succeed. If they fail to recognize a token, they return false
  // and restore a position of the input iterator.
  //
  // Detect*() methods check whether a previously scanned identifier
  // matches one of CSS keywords. If they they succeed, they initialize a token
  // type and return true, otherwise they return false and leave a token type
  // intact.
  //
  // Detect*AndMaybeChangeParsingMode() methods may change a parsing mode
  // if they match an appropriate CSS keyword. The parsing mode may stay intact
  // even if a method succeeds.
  //
  // TryScanAndMaybeCopy*() methods are intended to be called twice: first time
  // to try scanning a token through a fast track that does not require copying,
  // second time through a slower copying track if an escape sequence is found
  // during the first pass.
  bool TryScanUnicodeRange(TrivialIntPair* value);
  void ScanIdentifier(TrivialStringPiece* value, bool* has_escape);
  bool IsInputIteratorAtIdentifierStart() const;
  template <bool copy>
  bool TryScanAndMaybeCopyIdentifier(TrivialStringPiece* value,
                                     std::string* value_copy);
  UChar32 ScanEscape();  // Escape is not a token, it is a part of string.
  bool DetectPropertyNameToken(const TrivialStringPiece& name,
                               Token* property_name_token) const;
  bool DetectPropertyValueToken(const TrivialStringPiece& name,
                                Token* property_value_token) const;
  bool DetectPseudoClassNameToken(const TrivialStringPiece& name,
                                  Token* pseudo_class_name_token) const;
  bool DetectPseudoElementNameToken(const TrivialStringPiece& name,
                                    Token* pseudo_element_name_token) const;
  bool DetectSupportsToken(const TrivialStringPiece& name,
                           Token* supports_token) const;
  bool DetectKnownFunctionTokenAndMaybeChangeParsingMode(
      const TrivialStringPiece& name, Token* known_function_token);
  bool DetectMediaQueryToken(const TrivialStringPiece& name,
                             Token* media_query_token,
                             cssom::MediaFeatureName* media_feature_name) const;
  bool TryScanUri(TrivialStringPiece* uri);
  bool FindUri(const char** uri_start, const char** uri_end, char* quote) const;
  template <bool copy>
  bool TryScanAndMaybeCopyUri(char quote, TrivialStringPiece* uri,
                              std::string* uri_copy);
  void ScanString(char quote, TrivialStringPiece* value);
  template <bool copy>
  bool TryScanAndMaybeCopyString(char quote, TrivialStringPiece* value,
                                 std::string* value_copy);
  bool TryScanNthChild(TrivialStringPiece* nth);
  bool TryScanNthChildExtra(TrivialStringPiece* nth);
  bool DetectUnitToken(const TrivialStringPiece& unit, Token* token) const;
  bool DetectAtTokenAndMaybeChangeParsingMode(const TrivialStringPiece& name,
                                              bool has_escape, Token* at_token);

  bool DetectMediaFeatureNamePrefix(Token* token);
  void ScanUnrecognizedAtRule();
  void HandleBraceIfExists(char character);

  const char* input_iterator_;
  StringPool* const string_pool_;

  ParsingMode parsing_mode_;

  int line_number_;
  const char* line_start_;

  std::deque<Token> prepended_tokens_;

  // Used to cache the open braces and close them if no matching at the end of
  // input.
  std::stack<char> open_braces_;

  DISALLOW_COPY_AND_ASSIGN(Scanner);
};

// By Bison's convention, a function that returns next token, should
// be named yylex().
inline Token yylex(TokenValue* token_value, YYLTYPE* token_location,
                   Scanner* scanner) {
  return scanner->Scan(token_value, token_location);
}

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_SCANNER_H_
