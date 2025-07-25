// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class FakeOmniboxPedal : public OmniboxPedal {
 public:
  FakeOmniboxPedal(OmniboxPedalId id, LabelStrings strings, GURL url)
      : OmniboxPedal(id, strings, url) {}

 private:
  ~FakeOmniboxPedal() override = default;
};

}  // namespace

TEST(AutocompleteMatchTypeTest, AccessibilityLabelHistory) {
  const std::u16string& kTestUrl = u"https://www.chromium.org";
  const std::u16string& kTestTitle = u"The Chromium Projects";

  // Test plain url.
  AutocompleteMatch match;
  match.type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
  match.description = kTestTitle;
  EXPECT_EQ(kTestUrl + u", 2 of 9",
            AutocompleteMatchType::ToAccessibilityLabel(match, kTestUrl, 1, 9));

  // Decorated with title and match type.
  match.type = AutocompleteMatchType::HISTORY_URL;
  EXPECT_EQ(kTestTitle + u" " + kTestUrl + u" location from history, 2 of 3",
            AutocompleteMatchType::ToAccessibilityLabel(match, kTestUrl, 1, 3));
}

TEST(AutocompleteMatchTypeTest, AccessibilityLabelSearch) {
  const std::u16string& kSearch = u"gondola";
  const std::u16string& kSearchDesc = u"Google Search";

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.description = kSearchDesc;
  EXPECT_EQ(kSearch + u" search, 6 of 8",
            AutocompleteMatchType::ToAccessibilityLabel(match, kSearch, 5, 8));

  // Make sure there's no suffix if |total_matches| is 0, regardless of the
  // |match_index| value.
  EXPECT_EQ(kSearch + u" search",
            AutocompleteMatchType::ToAccessibilityLabel(match, kSearch, 5, 0));
}

TEST(AutocompleteMatchTypeTest, AccessibilityLabelPedal) {
  const std::u16string& kPedal = u"clear browsing data";
  const std::u16string& kAccessibilityHint =
      u"Clear your chrome browsing history, cookies, and cache";

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::PEDAL;
  const OmniboxAction::LabelStrings label_strings(
      /*hint=*/u"", /*suggestion_contents=*/u"", /*accessibility_suffix=*/u"",
      /*accessibility_hint=*/kAccessibilityHint);
  match.takeover_action = base::MakeRefCounted<FakeOmniboxPedal>(
      OmniboxPedalId::CLEAR_BROWSING_DATA, label_strings, GURL());

  // Ensure that the accessibility hint is present in the a11y label for pedal
  // suggestions.
  EXPECT_EQ(kAccessibilityHint + u", 2 of 5",
            AutocompleteMatchType::ToAccessibilityLabel(match, kPedal, 1, 5));
}

namespace {

bool ParseAnswer(const std::string& answer_json, SuggestionAnswer* answer) {
  absl::optional<base::Value> value = base::JSONReader::Read(answer_json);
  if (!value || !value->is_dict())
    return false;

  // ParseAnswer previously did not change the default answer type of -1, so
  // here we keep the same behavior by explicitly supplying default value.
  return SuggestionAnswer::ParseAnswer(value->GetDict(), u"-1", answer);
}

}  // namespace

TEST(AutocompleteMatchTypeTest, AccessibilityLabelAnswer) {
  const std::u16string& kSearch = u"weather";
  const std::u16string& kSearchDesc = u"Google Search";

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
  match.description = kSearchDesc;
  std::string answer_json =
      "{ \"l\": ["
      "  { \"il\": { \"t\": [{ \"t\": \"text\", \"tt\": 8 }] } }, "
      "  { \"il\": { \"t\": [{ \"t\": \"sunny with a chance of hail\", \"tt\": "
      "5 }] } }] }";
  SuggestionAnswer answer;
  ASSERT_TRUE(ParseAnswer(answer_json, &answer));
  match.answer = answer;

  EXPECT_EQ(kSearch + u", answer, sunny with a chance of hail, 4 of 6",
            AutocompleteMatchType::ToAccessibilityLabel(match, kSearch, 3, 6));
}
