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

#ifndef CSS_PARSER_PARSER_H_
#define CSS_PARSER_PARSER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/css_style_sheet.h"

namespace cobalt {
namespace css_parser {

// Parser turns a string in UTF-8 encoding into one of CSSOM objects.
// Created CSSOM objects are self-contained and do not depend on input,
// so the arguments can be safely destroyed after one of the Parse...()
// functions returns. By the specification, parser is very tolerant to syntax
// errors. All recoverable syntax errors will be reported as warnings to
// the callback.

typedef base::Callback<void(const std::string& message)> OnMessageCallback;

//
// Parser entry points
// (see http://dev.w3.org/csswg/css-syntax/#parser-entry-points):
//

// Parses entire stylesheet.
scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
    const std::string& file_name, const std::string& input,
    const OnMessageCallback& on_warning_callback,
    const OnMessageCallback& on_error_callback);

// TODO(***REMOVED***): Implement other entry points.

}  // namespace css_parser
}  // namespace cobalt

#endif  // CSS_PARSER_PARSER_H_
