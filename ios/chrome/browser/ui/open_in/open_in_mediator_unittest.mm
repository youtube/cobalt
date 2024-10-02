// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/open_in/open_in_mediator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/ui/open_in/open_in_toolbar.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing OpenInMediator class.
class OpenInMediatorTest : public PlatformTest {
 protected:
  OpenInMediatorTest()
      : browser_state_(TestChromeBrowserState::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(browser_state_.get())),
        mediator_([[OpenInMediator alloc]
            initWithBaseViewController:nil
                               browser:browser_.get()]) {}

  std::unique_ptr<web::FakeWebState> CreateWebStateWithView() {
    auto web_state = std::make_unique<web::FakeWebState>();
    CGRect web_view_frame = CGRectMake(0, 0, 100, 100);
    UIView* web_state_view = [[UIView alloc] initWithFrame:web_view_frame];
    web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy_] scrollViewProxy];
    web_state->SetWebViewProxy(web_view_proxy_mock);

    web_state->SetView(web_state_view);
    web_state->SetBrowserState(browser_state_.get());
    return web_state;
  }

  // Returns the potential OpenInToolbar subview of the given view.
  OpenInToolbar* ToolbarInView(UIView* view) {
    for (UIView* subview in view.subviews) {
      if ([subview isKindOfClass:[OpenInToolbar class]]) {
        return base::mac::ObjCCastStrict<OpenInToolbar>(subview);
      }
    }
    return nil;
  }

  // Whether the given view has an OpenInToolbar as subview.
  bool ViewContainsToolbar(UIView* view) { return ToolbarInView(view) != nil; }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  id web_view_proxy_mock;
  CRWWebViewScrollViewProxy* scroll_view_proxy_;
  OpenInMediator* mediator_;
};

// Tests that enabling openIn adds openInToolBar to the WebState view.
TEST_F(OpenInMediatorTest, EnableForWebState) {
  auto web_state = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];

  EXPECT_TRUE(ViewContainsToolbar(web_state->GetView()));
  [mediator_ disableAll];
}

// Tests that disabling openIn removes openInToolBar from the WebState view.
TEST_F(OpenInMediatorTest, DisableForWebState) {
  auto web_state = CreateWebStateWithView();

  [mediator_ enableOpenInForWebState:web_state.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];
  ASSERT_TRUE(ViewContainsToolbar(web_state->GetView()));
  [mediator_ disableOpenInForWebState:web_state.get()];
  EXPECT_FALSE(ViewContainsToolbar(web_state->GetView()));
  [mediator_ disableAll];
}

// Tests that OpenInMediator can handle multiple WebStates correctly.
TEST_F(OpenInMediatorTest, MultipleWebStates) {
  auto web_state_1 = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state_1.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];
  EXPECT_TRUE(ViewContainsToolbar(web_state_1->GetView()));

  auto web_state_2 = CreateWebStateWithView();
  [mediator_ enableOpenInForWebState:web_state_2.get()
                     withDocumentURL:GURL::EmptyGURL()
                   suggestedFileName:@""];
  EXPECT_TRUE(ViewContainsToolbar(web_state_2->GetView()));

  EXPECT_NSNE(ToolbarInView(web_state_1->GetView()),
              ToolbarInView(web_state_2->GetView()));

  // Verify that destroy will detach the OpenIn view from the WebState.
  [mediator_ destroyOpenInForWebState:web_state_1.get()];
  EXPECT_FALSE(ViewContainsToolbar(web_state_1->GetView()));

  // Verify that destroying OpenIn for `web_state_1` doesn't affect
  // `web_state_2`.
  EXPECT_TRUE(ViewContainsToolbar(web_state_2->GetView()));

  // Verify that calling disableAll remove any remaining views.
  [mediator_ disableAll];
  EXPECT_FALSE(ViewContainsToolbar(web_state_2->GetView()));
}
