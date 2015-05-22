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

#include "base/callback.h"
#include "cobalt/cssom/css_parser.h"

namespace cobalt {
namespace css_parser {

class Parser : public cssom::CSSParser {
 public:
  static scoped_ptr<Parser> Create();
  ~Parser();

  scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
      const std::string& input,
      const base::SourceLocation& input_location) OVERRIDE;

  scoped_refptr<cssom::CSSStyleRule> ParseStyleRule(
      const std::string& input,
      const base::SourceLocation& input_location) OVERRIDE;

  scoped_refptr<cssom::CSSStyleDeclarationData> ParseDeclarationList(
      const std::string& input,
      const base::SourceLocation& input_location) OVERRIDE;

  scoped_refptr<cssom::PropertyValue> ParsePropertyValue(
      const std::string& property_name, const std::string& property_value,
      const base::SourceLocation& property_location) OVERRIDE;

  void ParsePropertyIntoStyle(
      const std::string& property_name, const std::string& property_value,
      const base::SourceLocation& property_location,
      cssom::CSSStyleDeclarationData* style_declaration) OVERRIDE;

 private:
  typedef base::Callback<void(const std::string& message)> OnMessageCallback;

  enum MessageVerbosity {
    // With message verbosity set to kShort, expect to see a one line message
    // indicating the error/warning and that is it.  This is useful for testing.
    kShort,

    // With message verbosity set to kVerbose, expect to see information such
    // as the line that caused the error to be output, allong with an arrow
    // pointing to where the error occurred on the line.
    kVerbose,
  };

  Parser(const OnMessageCallback& on_warning_callback,
         const OnMessageCallback& on_error_callback,
         MessageVerbosity message_verbosity);

  const OnMessageCallback on_warning_callback_;
  const OnMessageCallback on_error_callback_;
  MessageVerbosity message_verbosity_;

  friend class ParserImpl;
  friend class ParserTest;
  DISALLOW_COPY_AND_ASSIGN(Parser);
};

}  // namespace css_parser
}  // namespace cobalt

#endif  // CSS_PARSER_PARSER_H_
