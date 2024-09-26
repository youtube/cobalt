// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FormInputAccessoryViewHandlerTest : public PlatformTest {
 public:
  FormInputAccessoryViewHandlerTest() {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
  }

 protected:
  web::WebState* web_state() { return web_state_.get(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
};

// Tests that trying to programmatically dismiss the keyboard when it isn't
// visible doesn't crash the browser.
TEST_F(FormInputAccessoryViewHandlerTest, FormInputAccessoryViewHandler) {
  FormInputAccessoryViewHandler* accessory_view_delegate =
      [[FormInputAccessoryViewHandler alloc] init];
  ASSERT_TRUE(accessory_view_delegate);
  [accessory_view_delegate closeKeyboardWithoutButtonPress];
  accessory_view_delegate.webState = web_state();
  [accessory_view_delegate closeKeyboardWithoutButtonPress];
}
