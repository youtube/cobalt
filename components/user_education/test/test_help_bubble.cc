// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_help_bubble.h"

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education::test {

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestHelpBubble)
DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestHelpBubbleFactory)

// static
constexpr int TestHelpBubble::kNoButtonWithTextIndex;

TestHelpBubble::TestHelpBubble(ui::TrackedElement* element,
                               HelpBubbleParams params)
    : element_(element), params_(std::move(params)) {
  element_hidden_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
          element->identifier(), element->context(),
          base::BindRepeating(&TestHelpBubble::OnElementHidden,
                              base::Unretained(this)));
}

TestHelpBubble::~TestHelpBubble() {
  // Needs to be called here while we still have access to derived class
  // methods.
  Close();
}

bool TestHelpBubble::ToggleFocusForAccessibility() {
  ++focus_count_;
  return true;
}

// Simulates the user dismissing the bubble.
void TestHelpBubble::SimulateDismiss() {
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  std::move(params_.dismiss_callback).Run();
  if (weak_ptr)
    weak_ptr->Close();
}

// Simulates the bubble timing out.
void TestHelpBubble::SimulateTimeout() {
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  std::move(params_.timeout_callback).Run();
  if (weak_ptr)
    weak_ptr->Close();
}

// Simualtes the user pressing one of the bubble buttons.
void TestHelpBubble::SimulateButtonPress(int button_index) {
  CHECK_LT(button_index, static_cast<int>(params_.buttons.size()));
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  std::move(params_.buttons[button_index].callback).Run();
  if (weak_ptr)
    weak_ptr->Close();
}

// Provides the index of a button with a given string value as its text
// property. If one does not exist, returns -1.
int TestHelpBubble::GetIndexOfButtonWithText(std::u16string text) {
  for (size_t i = 0; i < params_.buttons.size(); i++) {
    if (params_.buttons[i].text == text) {
      return static_cast<int>(i);
    }
  }
  return kNoButtonWithTextIndex;
}

void TestHelpBubble::CloseBubbleImpl() {
  element_ = nullptr;
  element_hidden_subscription_ = base::CallbackListSubscription();
}

ui::ElementContext TestHelpBubble::GetContext() const {
  return element_ ? element_->context() : ui::ElementContext();
}

void TestHelpBubble::OnElementHidden(ui::TrackedElement* element) {
  if (element == element_) {
    if (is_open()) {
      Close();
    } else {
      element_ = nullptr;
      element_hidden_subscription_ = base::CallbackListSubscription();
    }
  }
}

std::unique_ptr<HelpBubble> TestHelpBubbleFactory::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  return std::make_unique<TestHelpBubble>(element, std::move(params));
}

bool TestHelpBubbleFactory::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<ui::test::TestElement>();
}

}  // namespace user_education::test
