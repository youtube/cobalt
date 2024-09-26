// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/passwords/test/mock_ios_chrome_save_passwords_infobar_delegate.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for InfobarBannerInteractionHandler.
class InfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  InfobarBannerInteractionHandlerTest()
      : handler_(PasswordInfobarBannerOverlayRequestConfig::RequestSupport()) {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfoBarManagerImpl::CreateForWebState(&web_state_);
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);

    std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypePasswordSave,
        MockIOSChromeSavePasswordInfoBarDelegate::Create(@"username",
                                                         @"password"));
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  MockIOSChromeSavePasswordInfoBarDelegate& mock_delegate() {
    return *static_cast<MockIOSChromeSavePasswordInfoBarDelegate*>(
        infobar_->delegate());
  }

 protected:
  InfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_;
};

// Tests that pressing the modal button calls adds an OverlayRequest for the
// modal UI to the WebState's queue at OverlayModality::kInfobarModal.
TEST_F(InfobarBannerInteractionHandlerTest, ShowModal) {
  OverlayRequestQueue* queue = OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kInfobarModal);
  ASSERT_EQ(0U, queue->size());
  handler_.ShowModalButtonTapped(infobar_, &web_state_);

  OverlayRequest* modal_request = queue->front_request();
  EXPECT_TRUE(modal_request);
  EXPECT_EQ(infobar_, GetOverlayRequestInfobar(modal_request));
}

// Tests that BannerVisibilityChanged() calls InfobarDismissed() on the mock
// delegate.
TEST_F(InfobarBannerInteractionHandlerTest, UserInitiatedDismissal) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerDismissedByUser(infobar_);
}
