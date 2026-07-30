// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/at_memory_bottom_sheet_delegate_android.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/ui/mock_autofill_suggestion_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class AtMemoryBottomSheetDelegateAndroidTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient client_;
};

TEST_F(AtMemoryBottomSheetDelegateAndroidTest, OnDismissedHidesSuggestions) {
  testing::NiceMock<MockAutofillSuggestionDelegate> mock_suggestion_delegate;
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate.GetWeakPtr(), /*suggestions=*/{});
  ON_CALL(mock_suggestion_delegate, GetMainFillingProduct)
      .WillByDefault(testing::Return(FillingProduct::kAtMemory));
  client_.ShowAutofillSuggestions(AutofillClient::PopupOpenArgs(),
                                  mock_suggestion_delegate.GetWeakPtr());

  delegate.OnDismissed();

  EXPECT_EQ(client_.popup_hiding_reason(),
            SuggestionHidingReason::kUserAborted);
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest, OnQuerySubmittedCallsDelegate) {
  testing::NiceMock<MockAutofillSuggestionDelegate> mock_suggestion_delegate;
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate.GetWeakPtr(), /*suggestions=*/{});

  EXPECT_CALL(mock_suggestion_delegate,
              OnSearchSubmitted(std::u16string(u"query")));
  delegate.OnQuerySubmitted(u"query");
}

TEST_F(AtMemoryBottomSheetDelegateAndroidTest,
       OnSuggestionSelectedCallsDelegate) {
  testing::NiceMock<MockAutofillSuggestionDelegate> mock_suggestion_delegate;
  std::vector<Suggestion> suggestions = {
      Suggestion(u"first", SuggestionType::kAddressEntry),
      Suggestion(u"second", SuggestionType::kAddressEntry)};
  AtMemoryBottomSheetDelegateAndroid delegate(
      &client_, mock_suggestion_delegate.GetWeakPtr(), suggestions);

  EXPECT_CALL(mock_suggestion_delegate, DidAcceptSuggestion);
  delegate.OnSuggestionSelected(1);
}

}  // namespace autofill
