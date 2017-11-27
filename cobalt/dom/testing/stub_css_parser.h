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

#ifndef COBALT_DOM_TESTING_STUB_CSS_PARSER_H_
#define COBALT_DOM_TESTING_STUB_CSS_PARSER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/source_location.h"
#include "cobalt/cssom/css_declaration_data.h"
#include "cobalt/cssom/css_declared_style_data.h"
#include "cobalt/cssom/css_font_face_declaration_data.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_list.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace dom {
namespace testing {

class StubCSSParser : public cssom::CSSParser {
  scoped_refptr<cssom::CSSStyleSheet> ParseStyleSheet(
      const std::string& input,
      const base::SourceLocation& input_location) override;

  scoped_refptr<cssom::CSSRule> ParseRule(
      const std::string& input,
      const base::SourceLocation& input_location) override;

  scoped_refptr<cssom::CSSDeclaredStyleData> ParseStyleDeclarationList(
      const std::string& input,
      const base::SourceLocation& input_location) override;

  scoped_refptr<cssom::CSSFontFaceDeclarationData> ParseFontFaceDeclarationList(
      const std::string& input,
      const base::SourceLocation& input_location) override;

  scoped_refptr<cssom::PropertyValue> ParsePropertyValue(
      const std::string& property_name, const std::string& property_value,
      const base::SourceLocation& property_location) override;

  void ParsePropertyIntoDeclarationData(
      const std::string& property_name, const std::string& property_value,
      const base::SourceLocation& property_location,
      cssom::CSSDeclarationData* declaration_data) override;

  scoped_refptr<cssom::MediaQuery> ParseMediaQuery(
      const std::string& media_query,
      const base::SourceLocation& input_location) override;

  scoped_refptr<cssom::MediaList> ParseMediaList(
      const std::string& media_list,
      const base::SourceLocation& input_location) override;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_STUB_CSS_PARSER_H_
