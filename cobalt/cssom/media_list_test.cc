/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/media_list.h"

#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/property_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;

class PropertyValue;

class MockCSSParser : public CSSParser {
 public:
  MOCK_METHOD2(ParseStyleSheet,
               scoped_refptr<CSSStyleSheet>(const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD2(ParseStyleRule,
               scoped_refptr<CSSStyleRule>(const std::string&,
                                           const base::SourceLocation&));
  MOCK_METHOD2(ParseDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD4(ParsePropertyIntoStyle,
               void(const std::string&, const std::string&,
                    const base::SourceLocation&,
                    CSSStyleDeclarationData* style_declaration));
  MOCK_METHOD2(ParseMediaQuery,
               scoped_refptr<MediaQuery>(const std::string&,
                                         const base::SourceLocation&));
};


TEST(MediaListTest, ItemAccess) {
  MockCSSParser css_parser;
  scoped_refptr<MediaList> media_list(new MediaList(&css_parser));
  ASSERT_EQ(0, media_list->length());
  ASSERT_TRUE(media_list->Item(0).empty());

  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  media_list->Append(media_query);
  ASSERT_EQ(1, media_list->length());
  // The returned string is empty, because MediaQuery serialization is not
  // implemented yet.
  ASSERT_TRUE(media_list->Item(0).empty());
  ASSERT_TRUE(media_list->Item(1).empty());
}

TEST(MediaListTest, AppendMedium) {
  MockCSSParser css_parser;
  scoped_refptr<MediaList> media_list(new MediaList(&css_parser));
  const std::string media_query = "screen";

  EXPECT_CALL(css_parser, ParseMediaQuery(media_query, _))
      .WillOnce(testing::Return(scoped_refptr<MediaQuery>()));
  media_list->AppendMedium(media_query);
}

TEST(MediaListTest, EvaluateMediaQueryFalse) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(false));
  scoped_refptr<MediaList> media_list(new MediaList());

  media_list->Append(media_query);
  EXPECT_FALSE(media_list->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaListTest, EvaluateMediaQueryTrue) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  EXPECT_TRUE(media_query->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query);
  EXPECT_TRUE(media_list->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaListTest, EvaluateMediaQueriesFalseAndFalse) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(false));
  EXPECT_FALSE(media_query_1->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(false));
  EXPECT_FALSE(media_query_2->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_FALSE(media_list->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaListTest, EvaluateMediaQueriesFalseAndTrue) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(false));
  EXPECT_FALSE(media_query_1->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(true));
  EXPECT_TRUE(media_query_2->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaListTest, EvaluateMediaQueriesTrueAndFalse) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(true));
  EXPECT_TRUE(media_query_1->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(false));
  EXPECT_FALSE(media_query_2->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(NULL, NULL));
}

TEST(MediaListTest, EvaluateMediaQueriesTrueAndTrue) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(true));
  EXPECT_TRUE(media_query_1->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(true));
  EXPECT_TRUE(media_query_2->EvaluateConditionValue(NULL, NULL));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(NULL, NULL));
}

}  // namespace cssom
}  // namespace cobalt
