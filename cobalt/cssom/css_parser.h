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

#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"

namespace cobalt {
namespace cssom {

class CSSStyleDeclaration;
class CSSStyleSheet;
class PropertyValue;

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

  // Parses the entire stylesheet.
  // Always returns non-NULL style sheet, even if an error occurred.
  virtual scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
      const std::string& input, const base::SourceLocation& input_location) = 0;

  // Parses the contents of a HTMLElement.style attribute.
  // Always returns non-NULL declaration, even if an error occurred.
  virtual scoped_refptr<cssom::CSSStyleDeclaration> ParseDeclarationList(
      const std::string& input, const base::SourceLocation& input_location) = 0;

  // Parses the property value.
  // This is a Cobalt's equivalent of a "list of component values".
  // May return NULL which is considered a valid property value.
  virtual scoped_refptr<cssom::PropertyValue> ParsePropertyValue(
      const std::string& property_name, const std::string& property_value,
      const base::SourceLocation& property_location) = 0;

  // TODO(***REMOVED***): Implement other entry points.

 protected:
  ~CSSParser() {}
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_CSS_PARSER_H_
