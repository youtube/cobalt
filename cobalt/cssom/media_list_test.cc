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

#include "cobalt/cssom/media_list.h"

#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/testing/mock_css_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace cobalt {
namespace cssom {
namespace {

TEST(MediaListTest, MediaTextSetter) {
  testing::MockCSSParser css_parser;
  scoped_refptr<MediaList> media_list(new MediaList(&css_parser));
  const std::string media_query = "screen, (width: 1024px)";

  scoped_refptr<MediaList> parsed_media_list(new MediaList(&css_parser));
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(true));
  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(true));
  parsed_media_list->Append(media_query_1);
  parsed_media_list->Append(media_query_2);

  EXPECT_CALL(css_parser, ParseMediaList(media_query, _))
      .WillOnce(Return(parsed_media_list));
  media_list->set_media_text(media_query);
  EXPECT_EQ(2, media_list->length());
}

TEST(MediaListTest, ItemAccessAndLength) {
  testing::MockCSSParser css_parser;
  scoped_refptr<MediaList> media_list(new MediaList(&css_parser));
  EXPECT_EQ(0, media_list->length());
  EXPECT_TRUE(media_list->Item(0).empty());

  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  media_list->Append(media_query);
  EXPECT_EQ(1, media_list->length());
  // The returned string is empty, because MediaQuery serialization is not
  // implemented yet.
  EXPECT_TRUE(media_list->Item(0).empty());
  EXPECT_TRUE(media_list->Item(1).empty());
}

TEST(MediaListTest, AppendMedium) {
  testing::MockCSSParser css_parser;
  scoped_refptr<MediaList> media_list(new MediaList(&css_parser));
  EXPECT_EQ(0, media_list->length());
  const std::string media_query = "screen";

  EXPECT_CALL(css_parser, ParseMediaQuery(media_query, _))
      .WillOnce(Return(scoped_refptr<MediaQuery>(new MediaQuery(true))));
  media_list->AppendMedium(media_query);
  EXPECT_EQ(1, media_list->length());
}

TEST(MediaListTest, EvaluateMediaQueryFalse) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(false));
  scoped_refptr<MediaList> media_list(new MediaList());

  media_list->Append(media_query);
  EXPECT_FALSE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaListTest, EvaluateMediaQueryTrue) {
  scoped_refptr<MediaQuery> media_query(new MediaQuery(true));
  EXPECT_TRUE(media_query->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query);
  EXPECT_TRUE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaListTest, EvaluateMediaQueriesFalseAndFalse) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(false));
  EXPECT_FALSE(media_query_1->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(false));
  EXPECT_FALSE(media_query_2->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_FALSE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaListTest, EvaluateMediaQueriesFalseAndTrue) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(false));
  EXPECT_FALSE(media_query_1->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(true));
  EXPECT_TRUE(media_query_2->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaListTest, EvaluateMediaQueriesTrueAndFalse) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(true));
  EXPECT_TRUE(media_query_1->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(false));
  EXPECT_FALSE(media_query_2->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

TEST(MediaListTest, EvaluateMediaQueriesTrueAndTrue) {
  scoped_refptr<MediaQuery> media_query_1(new MediaQuery(true));
  EXPECT_TRUE(media_query_1->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaQuery> media_query_2(new MediaQuery(true));
  EXPECT_TRUE(media_query_2->EvaluateConditionValue(ViewportSize(0, 0)));

  scoped_refptr<MediaList> media_list(new MediaList());
  media_list->Append(media_query_1);
  media_list->Append(media_query_2);
  EXPECT_TRUE(media_list->EvaluateConditionValue(ViewportSize(0, 0)));
}

}  // namespace
}  // namespace cssom
}  // namespace cobalt
