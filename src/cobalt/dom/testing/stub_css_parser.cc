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

#include "cobalt/dom/testing/stub_css_parser.h"

namespace cobalt {
namespace dom {
namespace testing {

scoped_refptr<cssom::CSSStyleSheet> StubCSSParser::ParseStyleSheet(
    const std::string& input, const base::SourceLocation& input_location) {
  cssom::CSSStyleSheet* new_style_sheet = new cssom::CSSStyleSheet();
  new_style_sheet->SetOriginClean(true);
  return new_style_sheet;
}

scoped_refptr<cssom::CSSRule> StubCSSParser::ParseRule(
    const std::string& input, const base::SourceLocation& input_location) {
  return NULL;
}

scoped_refptr<cssom::CSSDeclaredStyleData>
StubCSSParser::ParseStyleDeclarationList(
    const std::string& input, const base::SourceLocation& input_location) {
  return new cssom::CSSDeclaredStyleData();
}

scoped_refptr<cssom::CSSFontFaceDeclarationData>
StubCSSParser::ParseFontFaceDeclarationList(
    const std::string& input, const base::SourceLocation& input_location) {
  return new cssom::CSSFontFaceDeclarationData();
}

scoped_refptr<cssom::PropertyValue> StubCSSParser::ParsePropertyValue(
    const std::string& property_name, const std::string& property_value,
    const base::SourceLocation& property_location) {
  return NULL;
}

void StubCSSParser::ParsePropertyIntoDeclarationData(
    const std::string& property_name, const std::string& property_value,
    const base::SourceLocation& property_location,
    cssom::CSSDeclarationData* declaration_data) {
}

scoped_refptr<cssom::MediaList> StubCSSParser::ParseMediaList(
    const std::string& media_list, const base::SourceLocation& input_location) {
  return new cssom::MediaList();
}

scoped_refptr<cssom::MediaQuery> StubCSSParser::ParseMediaQuery(
    const std::string& media_query,
    const base::SourceLocation& input_location) {
  return new cssom::MediaQuery();
}

}  // namespace testing
}  // namespace dom
}  // namespace cobalt
