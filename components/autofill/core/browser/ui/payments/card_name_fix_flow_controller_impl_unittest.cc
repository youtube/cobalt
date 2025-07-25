// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_controller_impl.h"

#include <stddef.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class TestCardNameFixFlowView : public CardNameFixFlowView {
 public:
  void Show() override {}
  void ControllerGone() override {}
};

class CardNameFixFlowControllerImplGenericTest {
 public:
  CardNameFixFlowControllerImplGenericTest() {}

  CardNameFixFlowControllerImplGenericTest(
      const CardNameFixFlowControllerImplGenericTest&) = delete;
  CardNameFixFlowControllerImplGenericTest& operator=(
      const CardNameFixFlowControllerImplGenericTest&) = delete;

  void ShowPromptWithInferredName() {
    inferred_name_ = u"John Doe";
    ShowPrompt();
  }

  void ShowPromptWithoutInferredName() {
    inferred_name_ = u"";
    ShowPrompt();
  }

  void AcceptWithInferredName() { controller_->OnNameAccepted(inferred_name_); }

  void AcceptWithEditedName() { controller_->OnNameAccepted(u"Edited Name"); }

  void OnDialogClosed() { controller_->OnConfirmNameDialogClosed(); }

 protected:
  std::unique_ptr<TestCardNameFixFlowView> test_card_name_fix_flow_view_;
  std::unique_ptr<CardNameFixFlowControllerImpl> controller_;
  std::u16string inferred_name_;
  std::u16string accepted_name_;
  base::WeakPtrFactory<CardNameFixFlowControllerImplGenericTest>
      weak_ptr_factory_{this};

 private:
  void OnNameAccepted(const std::u16string& name) { accepted_name_ = name; }

  void ShowPrompt() {
    controller_->Show(
        test_card_name_fix_flow_view_.get(), inferred_name_,
        base::BindOnce(
            &CardNameFixFlowControllerImplGenericTest::OnNameAccepted,
            weak_ptr_factory_.GetWeakPtr()));
  }
};

class CardNameFixFlowControllerImplTest
    : public CardNameFixFlowControllerImplGenericTest,
      public testing::Test {
 public:
  CardNameFixFlowControllerImplTest() {}

  CardNameFixFlowControllerImplTest(const CardNameFixFlowControllerImplTest&) =
      delete;
  CardNameFixFlowControllerImplTest& operator=(
      const CardNameFixFlowControllerImplTest&) = delete;

  ~CardNameFixFlowControllerImplTest() override {}

  void SetUp() override {
    test_card_name_fix_flow_view_ = std::make_unique<TestCardNameFixFlowView>();
    controller_ = std::make_unique<CardNameFixFlowControllerImpl>();
  }
};

TEST_F(CardNameFixFlowControllerImplTest, LogShown) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();

  histogram_tester.ExpectUniqueSample(
      "Autofill.CardholderNameFixFlowPrompt.Events",
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_SHOWN, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogPrefilled) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();

  histogram_tester.ExpectBucketCount("Autofill.SaveCardCardholderNamePrefilled",
                                     true, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogNotPrefilled) {
  base::HistogramTester histogram_tester;
  ShowPromptWithoutInferredName();

  histogram_tester.ExpectBucketCount("Autofill.SaveCardCardholderNamePrefilled",
                                     false, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogAccepted) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();
  AcceptWithInferredName();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardholderNameFixFlowPrompt.Events",
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_ACCEPTED, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogUserAcceptedInferredName) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();
  AcceptWithInferredName();

  histogram_tester.ExpectBucketCount("Autofill.SaveCardCardholderNameWasEdited",
                                     false, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogUserAcceptedEditedName) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();
  AcceptWithEditedName();

  histogram_tester.ExpectBucketCount("Autofill.SaveCardCardholderNameWasEdited",
                                     true, 1);
}

TEST_F(CardNameFixFlowControllerImplTest, LogIgnored) {
  base::HistogramTester histogram_tester;
  ShowPromptWithInferredName();
  ShowPromptWithInferredName();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardholderNameFixFlowPrompt.Events",
      AutofillMetrics::
          CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION,
      1);

  OnDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardholderNameFixFlowPrompt.Events",
      AutofillMetrics::
          CARDHOLDER_NAME_FIX_FLOW_PROMPT_CLOSED_WITHOUT_INTERACTION,
      2);
}

TEST_F(CardNameFixFlowControllerImplTest, LogDismissed) {
  base::HistogramTester histogram_tester;
  controller_->OnDismissed();

  histogram_tester.ExpectBucketCount(
      "Autofill.CardholderNameFixFlowPrompt.Events",
      AutofillMetrics::CARDHOLDER_NAME_FIX_FLOW_PROMPT_DISMISSED, 1);
}

}  // namespace autofill
