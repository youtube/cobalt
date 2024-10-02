// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace autofill {

namespace {

NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";
NSString* const kCreditCardItemsTouchId = @"CREDIT-CARD-ITEMS";

}  // namespace

class CreditCardAutofillTouchBarControllerUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    touch_bar_controller_.reset([[CreditCardAutofillTouchBarController alloc]
        initWithController:&autofill_popup_controller_]);
  }

  void SetSuggestions(std::vector<Suggestion> suggestions) {
    autofill_popup_controller_.set_suggestions(std::move(suggestions));
  }

  void SetSuggestions(const std::vector<int>& frontends_ids) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(frontends_ids.size());
    for (int frontend_id : frontends_ids) {
      suggestions.emplace_back("", "", "", frontend_id);
    }
    SetSuggestions(std::move(suggestions));
  }

  base::scoped_nsobject<CreditCardAutofillTouchBarController>
      touch_bar_controller_;

 private:
  MockAutofillPopupController autofill_popup_controller_;
};

// Tests to check if the touch bar shows up properly.
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBar) {
  // Touch bar shouldn't appear if the popup is not for credit cards.
  [touch_bar_controller_ setIsCreditCardPopup:false];
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  // Touch bar shouldn't appear if the popup is empty.
  [touch_bar_controller_ setIsCreditCardPopup:true];
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions({1, 1});
  NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
  EXPECT_TRUE(touch_bar);
  EXPECT_TRUE([[touch_bar customizationIdentifier]
      isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);
  EXPECT_EQ(1UL, [[touch_bar itemIdentifiers] count]);
}

// Tests to check that the touch bar doesn't show more than 3 items
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBarCardLimit) {
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions({1, 1, 1, 1});
  NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
  EXPECT_TRUE(touch_bar);
  EXPECT_TRUE([[touch_bar customizationIdentifier]
      isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);

  NSTouchBarItem* item = [touch_bar_controller_
                   touchBar:touch_bar
      makeItemForIdentifier:ui::GetTouchBarItemId(kCreditCardAutofillTouchBarId,
                                                  kCreditCardItemsTouchId)];
  NSGroupTouchBarItem* groupItem = static_cast<NSGroupTouchBarItem*>(item);

  EXPECT_EQ(3UL, [[[groupItem groupTouchBar] itemIdentifiers] count]);
}

// Tests for for the credit card button.
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, CreditCardButtonCheck) {
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions({Suggestion("bufflehead", "canvasback", "goldeneye", 1)});
  NSButton* button = [touch_bar_controller_ createCreditCardButtonAtRow:0];
  EXPECT_TRUE(button);
  EXPECT_EQ(0, [button tag]);
  EXPECT_EQ("bufflehead canvasback", base::SysNSStringToUTF8([button title]));
}

}  // namespace autofill
