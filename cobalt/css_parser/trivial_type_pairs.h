// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSS_PARSER_TRIVIAL_TYPE_PAIRS_H_
#define COBALT_CSS_PARSER_TRIVIAL_TYPE_PAIRS_H_

namespace cobalt {
namespace css_parser {

// Contains template for type pairs with trivial constructors and destructors
// in order to allow them to be members of Bison's YYSTYPE union. Used to return
// the value for tokens that represent a pairs of types from scanner to parser.

template <typename A, typename B>
struct TrivialPair {
  A first;
  B second;
};

typedef TrivialPair<int, int> TrivialIntPair;

}  // namespace css_parser
}  // namespace cobalt

#endif  // COBALT_CSS_PARSER_TRIVIAL_TYPE_PAIRS_H_
