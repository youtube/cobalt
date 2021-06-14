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

#ifndef COBALT_CSS_PARSER_TRIVIAL_STRING_PIECE_H_
#define COBALT_CSS_PARSER_TRIVIAL_STRING_PIECE_H_

#include <algorithm>
#include <iostream>
#include <string>

#include "base/logging.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace css_parser {

// A structure that represents a substring. Has trivial constructor and
// destructor in order to allow it to be a member of Bison's YYSTYPE union.
// Used to return a string token value from scanner to parser. All memory
// management is external: strings point either into an input or into a string
// pool injected into the scanner.
struct TrivialStringPiece {
  static TrivialStringPiece FromCString(const char* c_string);

  inline std::size_t size() const {
    DCHECK_LE(begin, end);
    return static_cast<std::size_t>(end - begin);
  }

  inline std::string ToString() const { return std::string(begin, end); }

  const char* begin;
  const char* end;
};

inline TrivialStringPiece TrivialStringPiece::FromCString(
    const char* c_string) {
  TrivialStringPiece string_piece;
  string_piece.begin = c_string;
  string_piece.end = c_string + strlen(c_string);
  return string_piece;
}

// Used by tests.
inline bool operator==(const TrivialStringPiece& lhs, const char* rhs) {
  return strncmp(lhs.begin, rhs, lhs.size()) == 0 &&
         rhs[lhs.size()] == '\0';
}

// Used by tests.
inline bool operator==(const char* lhs, const TrivialStringPiece& rhs) {
  return rhs == lhs;
}

// Used by tests.
inline bool operator!=(const TrivialStringPiece& lhs, const char* rhs) {
  return !(lhs == rhs);
}

// Used by tests.
inline bool operator!=(const char* lhs, const TrivialStringPiece& rhs) {
  return !(rhs == lhs);
}

// Used by tests.
inline std::ostream& operator<<(std::ostream& stream,
                                const TrivialStringPiece& string_piece) {
  stream << "\"";
  for (const char* character_iterator(string_piece.begin);
       character_iterator != string_piece.end; ++character_iterator) {
    stream << *character_iterator;
  }
  stream << "\"";
  return stream;
}

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_TRIVIAL_STRING_PIECE_H_
