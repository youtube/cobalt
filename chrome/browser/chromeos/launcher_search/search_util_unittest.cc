// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search/search_util.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

TEST(SearchUtilTest, ProviderTypes) {
  const int types = ProviderTypes();
  EXPECT_FALSE(types & AutocompleteProvider::TYPE_DOCUMENT);
  EXPECT_TRUE(types & AutocompleteProvider::TYPE_OPEN_TAB);
}

// Tests result conversion for a default answer result.
TEST(SearchUtilTest, CreateAnswerResult) {
  AutocompleteMatch match;
  match.relevance = 1248;
  match.destination_url = GURL("http://www.example.com/");
  match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST;
  match.contents = u"contents";
  match.description = u"description";

  SuggestionAnswer answer;
  std::string json =
      R"({ "l": [)"
      R"(  { "il": { "t": [{ "t": "text one", "tt": 8 }],)"
      R"(            "at": { "t": "additional one", "tt": 42 } } },)"
      R"(  { "il": { "t": [{ "t": "text two", "tt": 5 }],)"
      R"(            "at": { "t": "additional two", "tt": 6 },)"
      R"(            "al": "a11y label" } })"
      R"(] })";
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(value->GetDict(), u"-1", &answer));
  match.answer = answer;

  const auto result =
      CreateAnswerResult(match, nullptr, u"query", AutocompleteInput());
  EXPECT_EQ(result->type, mojom::SearchResultType::kOmniboxResult);
  EXPECT_EQ(result->relevance, 1248);
  ASSERT_TRUE(result->destination_url.has_value());
  EXPECT_EQ(result->destination_url.value(), GURL("http://www.example.com/"));
  EXPECT_EQ(result->is_omnibox_search,
            mojom::SearchResult::OptionalBool::kTrue);
  EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kTrue);

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"contents");
  ASSERT_TRUE(result->additional_contents.has_value());
  EXPECT_EQ(result->additional_contents.value(), u"additional one");
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"text two");
  ASSERT_TRUE(result->additional_description.has_value());
  EXPECT_EQ(result->additional_description.value(), u"additional two");
}

// Tests result conversion for a rich entity Omnibox result.
TEST(SearchUtilTest, CreateResult) {
  AutocompleteMatch match;
  match.relevance = 300;
  match.destination_url = GURL("http://www.example.com/");
  match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY;
  match.image_url = GURL("http://www.example.com/image.jpeg");

  match.contents = u"contents";
  match.description = u"description";
  match.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::URL)};
  match.description_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  const auto result =
      CreateResult(match, nullptr, nullptr, nullptr, AutocompleteInput());
  EXPECT_EQ(result->type, mojom::SearchResultType::kOmniboxResult);
  EXPECT_EQ(result->relevance, 300);
  ASSERT_TRUE(result->destination_url.has_value());
  EXPECT_EQ(result->destination_url.value(), GURL("http://www.example.com/"));
  EXPECT_EQ(result->is_omnibox_search,
            mojom::SearchResult::OptionalBool::kTrue);
  EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kFalse);
  EXPECT_EQ(result->omnibox_type, mojom::SearchResult::OmniboxType::kSearch);
  ASSERT_TRUE(result->image_url.has_value());
  EXPECT_EQ(result->image_url.value(),
            GURL("http://www.example.com/image.jpeg"));

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"contents");
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"description");

  // The URL text class should be retained, but MATCH should be ignored.
  EXPECT_EQ(result->contents_type, mojom::SearchResult::TextType::kUrl);
  EXPECT_EQ(result->description_type, mojom::SearchResult::TextType::kUnset);
}

// Tests result conversion for a weather Omnibox result.
TEST(SearchUtilTest, CreateWeatherResult) {
  AutocompleteMatch match;
  match.relevance = 1200;
  match.destination_url = GURL("https://www.example.com.au/weather");
  match.type = AutocompleteMatchType::Type::SEARCH_SUGGEST;
  match.contents = u"Weather in Perth";
  match.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  SuggestionAnswer answer;
  const std::string json =
      R"({ "l": [)"
      R"(  { "il": { "t": [ { "t": "weather in perth", "tt": 8 } ] } },)"
      R"(  {)"
      R"(    "il": {)"
      R"(      "al": "Sunny",)"
      R"(      "at": { "t": "Perth WA, Australia", "tt": 19 },)"
      R"(      "i": { "d": "//www.weather.com.au/sunny.png", "t": 3 },)"
      R"(      "t": [ { "t": "16°C", "tt": 18 } ])"
      R"(    })"
      R"(  })"
      R"(] })";
  const absl::optional<base::Value> value = base::JSONReader::Read(json);
  ASSERT_TRUE(value && value->is_dict());
  ASSERT_TRUE(SuggestionAnswer::ParseAnswer(
      value->GetDict(), /* The answer type for 'weather'. */ u"8", &answer));
  match.answer = answer;

  const auto result =
      CreateAnswerResult(match, nullptr, u"query", AutocompleteInput());
  EXPECT_EQ(result->type, mojom::SearchResultType::kOmniboxResult);
  EXPECT_EQ(result->relevance, 1200);
  ASSERT_TRUE(result->destination_url.has_value());
  EXPECT_EQ(result->destination_url.value(),
            GURL("https://www.example.com.au/weather"));
  EXPECT_EQ(result->is_omnibox_search,
            mojom::SearchResult::OptionalBool::kTrue);
  EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kTrue);

  ASSERT_TRUE(result->contents.has_value());
  EXPECT_EQ(result->contents.value(), u"Weather in Perth");
  EXPECT_FALSE(result->additional_contents.has_value());
  ASSERT_TRUE(result->description.has_value());
  EXPECT_EQ(result->description.value(), u"16°C");
  ASSERT_TRUE(result->additional_description.has_value());
  EXPECT_EQ(result->additional_description.value(), u"Perth WA, Australia");
  ASSERT_TRUE(result->description_a11y_label.has_value());
  EXPECT_EQ(result->description_a11y_label.value(), u"Sunny");
  ASSERT_TRUE(result->image_url.has_value());
  EXPECT_EQ(result->image_url.value(),
            GURL("https://www.weather.com.au/sunny.png"));
}

// Tests result conversion for a calculator result. A calculator result can
// either have a description or no description; both possibilities are tested.
TEST(SearchUtilTest, CreateCalculatorResult) {
  // A match with the input in contents and the answer in desc.
  AutocompleteMatch match_desc;
  match_desc.relevance = 300;
  match_desc.type = AutocompleteMatchType::CALCULATOR;
  match_desc.destination_url = GURL("https://www.example.com.au/calc?q=1+2");
  match_desc.contents = u"1+2";
  match_desc.description = u"3";
  match_desc.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};
  match_desc.description_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  // A match with the answer in content and no desc.
  AutocompleteMatch match_no_desc;
  match_no_desc.relevance = 300;
  match_no_desc.type = AutocompleteMatchType::CALCULATOR;
  match_no_desc.destination_url = GURL("https://www.example.com.au/calc?q=1+2");
  match_no_desc.contents = u"3";
  match_no_desc.contents_class = {
      ACMatchClassification(0, ACMatchClassification::Style::MATCH)};

  for (const auto& match : {match_desc, match_no_desc}) {
    const auto result = CreateAnswerResult(match, /*controller=*/nullptr,
                                           u"1+2", AutocompleteInput());
    EXPECT_EQ(result->relevance, 300);
    ASSERT_TRUE(result->destination_url.has_value());
    EXPECT_EQ(result->destination_url.value(),
              GURL("https://www.example.com.au/calc?q=1+2"));
    EXPECT_EQ(result->is_omnibox_search,
              mojom::SearchResult::OptionalBool::kTrue);
    EXPECT_EQ(result->is_answer, mojom::SearchResult::OptionalBool::kTrue);
    EXPECT_EQ(result->answer_type,
              mojom::SearchResult::AnswerType::kCalculator);
    ASSERT_TRUE(result->contents.has_value());
    EXPECT_EQ(result->contents.value(), u"1+2");
    ASSERT_TRUE(result->description.has_value());
    EXPECT_EQ(result->description.value(), u"3");
    EXPECT_EQ(result->contents_type, mojom::SearchResult::TextType::kUnset);
    EXPECT_EQ(result->description_type, mojom::SearchResult::TextType::kUnset);
  }
}

}  // namespace
}  // namespace crosapi
