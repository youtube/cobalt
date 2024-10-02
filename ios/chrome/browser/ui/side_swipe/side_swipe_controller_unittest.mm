// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

#import <WebKit/WebKit.h>

#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/common/crw_web_view_content_view.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SideSwipeController (ExposedForTesting)
@property(nonatomic, assign) BOOL leadingEdgeNavigationEnabled;
@property(nonatomic, assign) BOOL trailingEdgeNavigationEnabled;
- (void)updateNavigationEdgeSwipeForWebState:(web::WebState*)webState;
@end

namespace {

class SideSwipeControllerTest : public PlatformTest {
 public:
  SideSwipeControllerTest()
      : web_view_([[WKWebView alloc]
            initWithFrame:scoped_window_.Get().bounds
            configuration:[[WKWebViewConfiguration alloc] init]]),
        content_view_([[CRWWebViewContentView alloc]
            initWithWebView:web_view_
                 scrollView:web_view_.scrollView
            fullscreenState:CrFullscreenState::kNotInFullScreen]) {
    auto original_web_state(std::make_unique<web::FakeWebState>());
    original_web_state->SetView(content_view_);
    CRWWebViewScrollViewProxy* scroll_view_proxy =
        [[CRWWebViewScrollViewProxy alloc] init];
    UIScrollView* scroll_view = [[UIScrollView alloc] init];
    [scroll_view_proxy setScrollView:scroll_view];
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
    original_web_state->SetWebViewProxy(web_view_proxy_mock);

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    browser_->GetWebStateList()->InsertWebState(
        0, std::move(original_web_state), WebStateList::INSERT_NO_FLAGS,
        WebStateOpener());

    side_swipe_controller_ =
        [[SideSwipeController alloc] initWithBrowser:browser_.get()];

    view_ = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 240)];

    [side_swipe_controller_ addHorizontalGesturesToView:view_];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  UIView* view_;
  SideSwipeController* side_swipe_controller_;
  ScopedKeyWindow scoped_window_;
  WKWebView* web_view_ = nil;
  CRWWebViewContentView* content_view_ = nil;
};

TEST_F(SideSwipeControllerTest, TestConstructor) {
  EXPECT_TRUE(side_swipe_controller_);
}

// Tests that pages that need to use Chromium native swipe
TEST_F(SideSwipeControllerTest, TestEdgeNavigationEnabled) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://crash"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://foo"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  item->SetURL(GURL("chrome://version"));
  [side_swipe_controller_
      updateNavigationEdgeSwipeForWebState:fake_web_state.get()];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  // Tests that when webstate is nil calling
  // updateNavigationEdgeSwipeForWebState doesn't change the edge navigation
  // state.
  item->SetURL(GURL("http://wwww.test.com"));
  [side_swipe_controller_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);
  side_swipe_controller_.leadingEdgeNavigationEnabled = YES;
  side_swipe_controller_.trailingEdgeNavigationEnabled = YES;
  [side_swipe_controller_ updateNavigationEdgeSwipeForWebState:nil];
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);
}

// Tests that when the active webState is changed or when the active webState
// finishes navigation, the edge state will be updated accordingly.
TEST_F(SideSwipeControllerTest, ObserversTriggerStateUpdate) {
  ASSERT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  ASSERT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  auto fake_web_state = std::make_unique<web::FakeWebState>();
  fake_web_state->SetView(content_view_);
  CRWWebViewScrollViewProxy* scroll_view_proxy =
      [[CRWWebViewScrollViewProxy alloc] init];
  UIScrollView* scroll_view = [[UIScrollView alloc] init];
  [scroll_view_proxy setScrollView:scroll_view];
  id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
  [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy] scrollViewProxy];
  fake_web_state->SetWebViewProxy(web_view_proxy_mock);
  web::FakeWebState* fake_web_state_ptr = fake_web_state.get();
  auto fake_navigation_manager = std::make_unique<web::FakeNavigationManager>();
  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  fake_navigation_manager->SetVisibleItem(item.get());
  fake_navigation_manager->SetLastCommittedItem(item.get());
  fake_web_state->SetNavigationManager(std::move(fake_navigation_manager));

  // The NTP and chrome://crash should use native swipe.
  item->SetURL(GURL(kChromeUINewTabURL));
  // Insert the WebState and make sure it's active. This should trigger
  // didChangeActiveWebState and update edge navigation state.
  browser_->GetWebStateList()->InsertWebState(1, std::move(fake_web_state),
                                              WebStateList::INSERT_ACTIVATE,
                                              WebStateOpener());
  EXPECT_TRUE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_TRUE(side_swipe_controller_.trailingEdgeNavigationEnabled);

  // Non native URL should have shouldn't be handled by SideSwipeController.
  item->SetURL(GURL("http://wwww.test.test"));
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  // Navigation finish should also update the edge navigation state.
  fake_web_state_ptr->OnNavigationFinished(&context);
  EXPECT_FALSE(side_swipe_controller_.leadingEdgeNavigationEnabled);
  EXPECT_FALSE(side_swipe_controller_.trailingEdgeNavigationEnabled);
}

}  // anonymous namespace
