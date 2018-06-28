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

#ifndef COBALT_CSSOM_CHARACTER_CLASSIFICATION_H_
#define COBALT_CSSOM_CHARACTER_CLASSIFICATION_H_

namespace cobalt {
namespace cssom {

// Checks whether a character is affected by a CSS white space processing.
//   https://www.w3.org/TR/css3-text/#white-space-rules
inline bool IsWhiteSpace(char character) {
  // Histogram from Apple's page load test combined with some ad hoc browsing
  // some other test suites.
  //
  //     82%: 216330 non-space characters, all > U+0020
  //     11%:  30017 plain space characters, U+0020
  //      5%:  12099 newline characters, U+000A
  //      2%:   5346 tab characters, U+0009
  //
  // No other characters seen. No U+000C or U+000D, and no other control
  // characters. Accordingly, we check for non-spaces first, then space,
  // then newline, then tab, then the other characters.

  return character <= ' ' &&
         (character == ' ' || character == '\n' || character == '\t' ||
          character == '\r' || character == '\f');
}

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_CHARACTER_CLASSIFICATION_H_
