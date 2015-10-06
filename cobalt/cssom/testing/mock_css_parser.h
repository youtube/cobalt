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

#ifndef CSSOM_TESTING_MOCK_CSS_PARSER_H_
#define CSSOM_TESTING_MOCK_CSS_PARSER_H_

#include "cobalt/cssom/css_parser.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace cssom {
namespace testing {

class MockCSSParser : public CSSParser {
 public:
  MOCK_METHOD2(ParseStyleSheet,
               scoped_refptr<CSSStyleSheet>(const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD2(ParseRule, scoped_refptr<CSSRule>(const std::string&,
                                                 const base::SourceLocation&));
  MOCK_METHOD2(ParseStyleDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD2(ParseFontFaceDeclarationList,
               scoped_refptr<CSSFontFaceDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD4(ParsePropertyIntoDeclarationData,
               void(const std::string&, const std::string&,
                    const base::SourceLocation&, CSSDeclarationData*));
  MOCK_METHOD2(ParseMediaQuery,
               scoped_refptr<MediaQuery>(const std::string&,
                                         const base::SourceLocation&));
};

}  // namespace testing
}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_TESTING_MOCK_CSS_PARSER_H_
