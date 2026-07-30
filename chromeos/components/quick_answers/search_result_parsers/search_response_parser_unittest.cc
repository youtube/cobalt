// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/search_response_parser.h"

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quick_answers {

TEST(SearchResponseParserTest, ProcessResponseSuccessFirstResult) {
  constexpr char kSearchResponse[] = R"()]}'
    {
      "results": [
        {
          "oneNamespaceType": 13668,
          "unitConversionResult": {
            "source": {
              "valueAndUnit": {
                "rawText": "23 centimeters"
              }
            },
            "destination": {
              "valueAndUnit": {
                "rawText": "9.055 inches"
              }
            },
            "category": "Length",
            "sourceAmount": 23
          }
        }
      ]
    }
  )";
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse(kSearchResponse);
  ASSERT_TRUE(quick_answers_session);
  ASSERT_TRUE(quick_answers_session->quick_answer);
  EXPECT_EQ("9.055 inches",
            GetQuickAnswerTextForTesting(
                quick_answers_session->quick_answer->first_answer_row));
}

TEST(SearchResponseParserTest, ProcessResponseSuccessMultipleResults) {
  constexpr char kSearchResponse[] = R"()]}'
    {
      "results": [
        { "oneNamespaceType": 13666 },
        { "oneNamespaceType": 13667 },
        {
          "oneNamespaceType": 13668,
          "unitConversionResult": {
            "source": {
              "valueAndUnit": {
                "rawText": "23 centimeters"
              }
            },
            "destination": {
              "valueAndUnit": {
                "rawText": "9.055 inches"
              }
            },
            "category": "Length",
            "sourceAmount": 23
          }
        }
      ]
    }
  )";
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse(kSearchResponse);
  ASSERT_TRUE(quick_answers_session);
  ASSERT_TRUE(quick_answers_session->quick_answer);
  EXPECT_EQ("9.055 inches",
            GetQuickAnswerTextForTesting(
                quick_answers_session->quick_answer->first_answer_row));
}

TEST(SearchResponseParserTest, ProcessResponseNoResults) {
  // The empty line between the response body and XSSI prefix is intentional to
  // keep it consistent with the actual response we got from the server.
  constexpr char kSearchResponse[] = R"()]}'

    {}
  )";
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse(kSearchResponse);
  EXPECT_EQ(nullptr, quick_answers_session);
}

TEST(SearchResponseParserTest, ProcessResponseEmptyResults) {
  constexpr char kSearchResponse[] = R"()]}'

    { "results": [] }
  )";
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse(kSearchResponse);
  EXPECT_EQ(nullptr, quick_answers_session);
}

TEST(SearchResponseParserTest, ProcessResponseInvalidResponse) {
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse("results {}");
  EXPECT_EQ(nullptr, quick_answers_session);
}

TEST(SearchResponseParserTest, ProcessResponseInvalidXssiPrefix) {
  constexpr char kSearchResponse[] = R"()]'

    {}
  )";
  std::unique_ptr<QuickAnswersSession> quick_answers_session =
      ParseSearchResponse(kSearchResponse);
  EXPECT_EQ(nullptr, quick_answers_session);
}

}  // namespace quick_answers
