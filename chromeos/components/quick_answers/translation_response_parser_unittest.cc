// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/translation_response_parser.h"

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

TEST(TranslationResponseParserTest, ProcessResponseSuccess) {
  constexpr char kTranslationResponse[] = R"(
    {
      "data": {
        "translations": [
          {
            "translatedText": "translated text"
          }
        ]
      }
    }
  )";
  std::unique_ptr<TranslationResult> translation_result =
      ParseTranslationResponse(kTranslationResponse);
  ASSERT_TRUE(translation_result);
  EXPECT_EQ("translated text", translation_result->translated_text);
}

TEST(TranslationResponseParserTest,
     ProcessResponseWithAmpersandCharacterCodes) {
  constexpr char kTranslationResponse[] = R"(
    {
      "data": {
        "translations": [
          {
            "translatedText": "don&#39;t mess with me"
          }
        ]
      }
    }
  )";
  std::unique_ptr<TranslationResult> translation_result =
      ParseTranslationResponse(kTranslationResponse);
  ASSERT_TRUE(translation_result);
  // Should correctly unescape ampersand character codes.
  EXPECT_EQ("don't mess with me", translation_result->translated_text);
}

TEST(TranslationResponseParserTest, ProcessResponseNoResults) {
  constexpr char kTranslationResponse[] = R"(
    {}
  )";
  std::unique_ptr<TranslationResult> translation_result =
      ParseTranslationResponse(kTranslationResponse);
  EXPECT_FALSE(translation_result);
}

TEST(TranslationResponseParserTest, ProcessResponseInvalidResponse) {
  std::unique_ptr<TranslationResult> translation_result =
      ParseTranslationResponse("results {}");
  EXPECT_FALSE(translation_result);
}

}  // namespace quick_answers
