// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/confirm/confirm_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/infobars/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for ConfirmInfobarBannerInteractionHandler.
class ConfirmInfobarBannerInteractionHandlerTest : public PlatformTest {
 public:
  ConfirmInfobarBannerInteractionHandlerTest() {
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &DefaultInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);

    std::unique_ptr<InfoBarIOS> infobar =
        std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeConfirm,
                                     std::make_unique<MockInfobarDelegate>());
    infobar_ = infobar.get();
    InfoBarManagerImpl::FromWebState(&web_state_)
        ->AddInfoBar(std::move(infobar));
  }

  MockInfobarDelegate& mock_delegate() {
    return *static_cast<MockInfobarDelegate*>(
        infobar_->delegate()->AsConfirmInfoBarDelegate());
  }

 protected:
  ConfirmInfobarBannerInteractionHandler handler_;
  web::FakeWebState web_state_;
  InfoBarIOS* infobar_;
};

// Tests MainButtonTapped() calls Accept() on the mock delegate.
TEST_F(ConfirmInfobarBannerInteractionHandlerTest, MainButton) {
  EXPECT_CALL(mock_delegate(), Accept()).WillOnce(testing::Return(true));
  handler_.MainButtonTapped(infobar_);
}

// Tests that BannerVisibilityChanged() InfobarDismissed() on the mock delegate.
TEST_F(ConfirmInfobarBannerInteractionHandlerTest, Presentation) {
  EXPECT_CALL(mock_delegate(), InfoBarDismissed());
  handler_.BannerVisibilityChanged(infobar_, false);
}
