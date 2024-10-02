// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state_observer_bridge.h"

#import "base/memory/ptr_util.h"
#import "base/scoped_observation.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/test/fakes/crw_fake_web_state_observer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "net/http/http_response_headers.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
const char kRawResponseHeaders[] =
    "HTTP/1.1 200 OK\0"
    "Content-Length: 450\0"
    "Connection: keep-alive\0";
}  // namespace

// Test fixture to test WebStateObserverBridge class.
class WebStateObserverBridgeTest : public PlatformTest {
 protected:
  WebStateObserverBridgeTest()
      : observer_([[CRWFakeWebStateObserver alloc] init]),
        observer_bridge_(observer_),
        response_headers_(new net::HttpResponseHeaders(
            std::string(kRawResponseHeaders, sizeof(kRawResponseHeaders)))) {
    scoped_observation_.Observe(&fake_web_state_);
  }

  web::FakeWebState fake_web_state_;
  CRWFakeWebStateObserver* observer_;
  WebStateObserverBridge observer_bridge_;
  base::ScopedObservation<WebState, WebStateObserver> scoped_observation_{
      &observer_bridge_};
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

// Tests `webStateWasShown:` forwarding.
TEST_F(WebStateObserverBridgeTest, WasShown) {
  ASSERT_FALSE([observer_ wasShownInfo]);

  observer_bridge_.WasShown(&fake_web_state_);
  ASSERT_TRUE([observer_ wasShownInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ wasShownInfo]->web_state);
}

// Tests `webStateWasHidden:` forwarding.
TEST_F(WebStateObserverBridgeTest, WasHidden) {
  ASSERT_FALSE([observer_ wasHiddenInfo]);

  observer_bridge_.WasHidden(&fake_web_state_);
  ASSERT_TRUE([observer_ wasHiddenInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ wasHiddenInfo]->web_state);
}

// Tests `webState:didStartNavigation:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidStartNavigation) {
  ASSERT_FALSE([observer_ didStartNavigationInfo]);

  GURL url("https://chromium.test/");
  std::unique_ptr<web::NavigationContext> context =
      web::NavigationContextImpl::CreateNavigationContext(
          &fake_web_state_, url, /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*is_renderer_initiated=*/false);
  observer_bridge_.DidStartNavigation(&fake_web_state_, context.get());

  ASSERT_TRUE([observer_ didStartNavigationInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ didStartNavigationInfo]->web_state);
  web::NavigationContext* actual_context =
      [observer_ didStartNavigationInfo]->context.get();
  ASSERT_TRUE(actual_context);
  EXPECT_EQ(&fake_web_state_, actual_context->GetWebState());
  EXPECT_EQ(context->IsSameDocument(), actual_context->IsSameDocument());
  EXPECT_EQ(context->GetError(), actual_context->GetError());
  EXPECT_EQ(context->GetUrl(), actual_context->GetUrl());
  EXPECT_EQ(context->HasUserGesture(), actual_context->HasUserGesture());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
      actual_context->GetPageTransition()));
  EXPECT_EQ(context->GetResponseHeaders(),
            actual_context->GetResponseHeaders());
}

// Tests `webState:didRedirectNavigation:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidRedirectNavigation) {
  ASSERT_FALSE([observer_ didRedirectNavigationInfo]);

  GURL url("https://chromium.test/");
  std::unique_ptr<web::NavigationContext> context =
      web::NavigationContextImpl::CreateNavigationContext(
          &fake_web_state_, url, /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*is_renderer_initiated=*/false);
  observer_bridge_.DidRedirectNavigation(&fake_web_state_, context.get());

  ASSERT_TRUE([observer_ didRedirectNavigationInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ didRedirectNavigationInfo]->web_state);
  web::NavigationContext* actual_context =
      [observer_ didRedirectNavigationInfo]->context.get();
  ASSERT_TRUE(actual_context);
  EXPECT_EQ(&fake_web_state_, actual_context->GetWebState());
  EXPECT_EQ(context->IsSameDocument(), actual_context->IsSameDocument());
  EXPECT_EQ(context->GetError(), actual_context->GetError());
  EXPECT_EQ(context->GetUrl(), actual_context->GetUrl());
  EXPECT_EQ(context->HasUserGesture(), actual_context->HasUserGesture());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
      actual_context->GetPageTransition()));
  EXPECT_EQ(context->GetResponseHeaders(),
            actual_context->GetResponseHeaders());
}

// Tests `webState:didFinishNavigation:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidFinishNavigation) {
  ASSERT_FALSE([observer_ didFinishNavigationInfo]);

  GURL url("https://chromium.test/");
  std::unique_ptr<web::NavigationContext> context =
      web::NavigationContextImpl::CreateNavigationContext(
          &fake_web_state_, url, /*has_user_gesture=*/true,
          ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR,
          /*is_renderer_initiated=*/false);
  observer_bridge_.DidFinishNavigation(&fake_web_state_, context.get());

  ASSERT_TRUE([observer_ didFinishNavigationInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ didFinishNavigationInfo]->web_state);
  web::NavigationContext* actual_context =
      [observer_ didFinishNavigationInfo]->context.get();
  ASSERT_TRUE(actual_context);
  EXPECT_EQ(&fake_web_state_, actual_context->GetWebState());
  EXPECT_EQ(context->IsSameDocument(), actual_context->IsSameDocument());
  EXPECT_EQ(context->GetError(), actual_context->GetError());
  EXPECT_EQ(context->GetUrl(), actual_context->GetUrl());
  EXPECT_EQ(context->HasUserGesture(), actual_context->HasUserGesture());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      ui::PageTransition::PAGE_TRANSITION_FROM_ADDRESS_BAR,
      actual_context->GetPageTransition()));
  EXPECT_EQ(context->GetResponseHeaders(),
            actual_context->GetResponseHeaders());
}

// Tests `webState:webStateDidStartLoading:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidStartLoading) {
  ASSERT_FALSE([observer_ startLoadingInfo]);

  observer_bridge_.DidStartLoading(&fake_web_state_);
  ASSERT_TRUE([observer_ startLoadingInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ startLoadingInfo]->web_state);
}

// Tests `webState:webStateDidStopLoading:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidStopLoading) {
  ASSERT_FALSE([observer_ stopLoadingInfo]);

  observer_bridge_.DidStopLoading(&fake_web_state_);
  ASSERT_TRUE([observer_ stopLoadingInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ stopLoadingInfo]->web_state);
}

// Tests `webState:didLoadPageWithSuccess:` forwarding.
TEST_F(WebStateObserverBridgeTest, PageLoaded) {
  ASSERT_FALSE([observer_ loadPageInfo]);

  // Forwarding PageLoadCompletionStatus::SUCCESS.
  observer_bridge_.PageLoaded(&fake_web_state_,
                              PageLoadCompletionStatus::SUCCESS);
  ASSERT_TRUE([observer_ loadPageInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ loadPageInfo]->web_state);
  EXPECT_TRUE([observer_ loadPageInfo]->success);

  // Forwarding PageLoadCompletionStatus::FAILURE.
  observer_bridge_.PageLoaded(&fake_web_state_,
                              PageLoadCompletionStatus::FAILURE);
  ASSERT_TRUE([observer_ loadPageInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ loadPageInfo]->web_state);
  EXPECT_FALSE([observer_ loadPageInfo]->success);
}

// Tests `webState:didChangeLoadingProgress:` forwarding.
TEST_F(WebStateObserverBridgeTest, LoadProgressChanged) {
  ASSERT_FALSE([observer_ changeLoadingProgressInfo]);

  const double kTestLoadProgress = 0.75;
  observer_bridge_.LoadProgressChanged(&fake_web_state_, kTestLoadProgress);
  ASSERT_TRUE([observer_ changeLoadingProgressInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ changeLoadingProgressInfo]->web_state);
  EXPECT_EQ(kTestLoadProgress, [observer_ changeLoadingProgressInfo]->progress);
}

// Tests `webStateDidChangeBackForwardState:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidChangeBackForwardState) {
  ASSERT_FALSE([observer_ changeBackForwardStateInfo]);

  observer_bridge_.DidChangeBackForwardState(&fake_web_state_);
  ASSERT_TRUE([observer_ changeBackForwardStateInfo]);
  EXPECT_EQ(&fake_web_state_,
            [observer_ changeBackForwardStateInfo]->web_state);
}

// Tests `webStateDidChangeTitle:` forwarding.
TEST_F(WebStateObserverBridgeTest, TitleWasSet) {
  ASSERT_FALSE([observer_ titleWasSetInfo]);

  observer_bridge_.TitleWasSet(&fake_web_state_);
  ASSERT_TRUE([observer_ titleWasSetInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ titleWasSetInfo]->web_state);
}

// Tests `webStateDidChangeVisibleSecurityState:` forwarding.
TEST_F(WebStateObserverBridgeTest, DidChangeVisibleSecurityState) {
  ASSERT_FALSE([observer_ didChangeVisibleSecurityStateInfo]);

  observer_bridge_.DidChangeVisibleSecurityState(&fake_web_state_);
  ASSERT_TRUE([observer_ didChangeVisibleSecurityStateInfo]);
  EXPECT_EQ(&fake_web_state_,
            [observer_ didChangeVisibleSecurityStateInfo]->web_state);
}

// Tests `webState:didUpdateFaviconURLCandidates:` forwarding.
TEST_F(WebStateObserverBridgeTest, FaviconUrlUpdated) {
  ASSERT_FALSE([observer_ updateFaviconUrlCandidatesInfo]);

  web::FaviconURL url(GURL("https://chromium.test/"),
                      web::FaviconURL::IconType::kTouchIcon, {gfx::Size(5, 6)});

  observer_bridge_.FaviconUrlUpdated(&fake_web_state_, {url});
  ASSERT_TRUE([observer_ updateFaviconUrlCandidatesInfo]);
  EXPECT_EQ(&fake_web_state_,
            [observer_ updateFaviconUrlCandidatesInfo]->web_state);
  ASSERT_EQ(1U, [observer_ updateFaviconUrlCandidatesInfo]->candidates.size());
  const web::FaviconURL& actual_url =
      [observer_ updateFaviconUrlCandidatesInfo]->candidates[0];
  EXPECT_EQ(url.icon_url, actual_url.icon_url);
  EXPECT_EQ(url.icon_type, actual_url.icon_type);
  ASSERT_EQ(url.icon_sizes.size(), actual_url.icon_sizes.size());
  EXPECT_EQ(url.icon_sizes[0].width(), actual_url.icon_sizes[0].width());
  EXPECT_EQ(url.icon_sizes[0].height(), actual_url.icon_sizes[0].height());
}

// Tests `webState:didChangeStateForPermission:` forwarding.
TEST_F(WebStateObserverBridgeTest, PermissionStateChanged) {
  if (@available(iOS 15.0, *)) {
    ASSERT_FALSE([observer_ permissionStateChangedInfo]);
    // Test PermissionMicrophone state changed.
    observer_bridge_.PermissionStateChanged(&fake_web_state_,
                                            web::PermissionMicrophone);
    ASSERT_TRUE([observer_ permissionStateChangedInfo]);
    EXPECT_EQ(&fake_web_state_,
              [observer_ permissionStateChangedInfo]->web_state);
    EXPECT_EQ(web::PermissionMicrophone,
              [observer_ permissionStateChangedInfo]->permission);
    // Test PermissionCamera state changed.
    observer_bridge_.PermissionStateChanged(&fake_web_state_,
                                            web::PermissionCamera);
    EXPECT_EQ(web::PermissionCamera,
              [observer_ permissionStateChangedInfo]->permission);
  }
}

// Tests `renderProcessGoneForWebState:` forwarding.
TEST_F(WebStateObserverBridgeTest, RenderProcessGone) {
  ASSERT_FALSE([observer_ renderProcessGoneInfo]);

  observer_bridge_.RenderProcessGone(&fake_web_state_);
  ASSERT_TRUE([observer_ renderProcessGoneInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ renderProcessGoneInfo]->web_state);
}

// Tests `webStateRealized:` forwarding.
TEST_F(WebStateObserverBridgeTest, WebStateRealized) {
  ASSERT_FALSE([observer_ webStateRealizedInfo]);

  observer_bridge_.WebStateRealized(&fake_web_state_);
  ASSERT_TRUE([observer_ webStateRealizedInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ webStateRealizedInfo]->web_state);
}

// Tests `webStateDestroyed:` forwarding.
TEST_F(WebStateObserverBridgeTest, WebStateDestroyed) {
  ASSERT_FALSE([observer_ webStateDestroyedInfo]);

  observer_bridge_.WebStateDestroyed(&fake_web_state_);
  ASSERT_TRUE([observer_ webStateDestroyedInfo]);
  EXPECT_EQ(&fake_web_state_, [observer_ webStateDestroyedInfo]->web_state);
}

}  // namespace web
