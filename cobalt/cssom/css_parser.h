/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CSSOM_CSS_PARSER_H_
#define CSSOM_CSS_PARSER_H_

#include <string>

#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace cssom {

// An abstraction of CSS parser. The parser turns a string in UTF-8 encoding
// into one of CSSOM objects. Created CSSOM objects should be self-contained
// and should not depend on the input, so the arguments can be safely destroyed
// after one of the Parse...() methods returns. Similarly to Chromium, parser
// should always succeed. Errors, if any, should be reported to logs.
//
// This class is not a part of any specification.
class CSSParser {
 public:
  //
  // Parser entry points
  // (see http://dev.w3.org/csswg/css-syntax/#parser-entry-points):
  //

  // Parses entire stylesheet.
  virtual scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
      const std::string& file_name, const std::string& input) = 0;
  virtual scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheetWithBeginLine(
      const std::string& file_name, const std::string& input,
      int begin_line) = 0;

  // TODO(***REMOVED***): Implement other entry points.

 protected:
  ~CSSParser() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_PARSER_H_
