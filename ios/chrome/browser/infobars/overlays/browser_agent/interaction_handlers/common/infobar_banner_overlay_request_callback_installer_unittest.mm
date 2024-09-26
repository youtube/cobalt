// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/test/mock_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for InfobarBannerOverlayRequestCallbackInstaller.
class InfobarBannerOverlayRequestCallbackInstallerTest : public PlatformTest {
 public:
  InfobarBannerOverlayRequestCallbackInstallerTest()
      : installer_(InfobarOverlayRequestConfig::RequestSupport(),
                   &mock_handler_) {
    std::unique_ptr<OverlayRequest> request =
        OverlayRequest::CreateWithConfig<InfobarOverlayRequestConfig>(
            &infobar_, InfobarOverlayType::kBanner, infobar_.high_priority());
    request_ = request.get();
    queue()->AddRequest(std::move(request));
    installer_.InstallCallbacks(request_);
  }

  OverlayCallbackManager* callback_manager() const {
    return request_->GetCallbackManager();
  }
  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner);
  }

 protected:
  FakeInfobarIOS infobar_;
  web::FakeWebState web_state_;
  OverlayRequest* request_ = nullptr;
  MockInfobarBannerInteractionHandler mock_handler_;
  InfobarBannerOverlayRequestCallbackInstaller installer_;
};

// Tests that a dispatched InfobarBannerMainActionResponse calls
// InfobarBannerInteractionHandler::MainButtonTapped().
TEST_F(InfobarBannerOverlayRequestCallbackInstallerTest, MainAction) {
  EXPECT_CALL(mock_handler_, MainButtonTapped(&infobar_));
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarBannerMainActionResponse>());
}

// Tests that a dispatched InfobarBannerShowModalResponse calls
// InfobarBannerInteractionHandler::ShowModalButtonTapped().
TEST_F(InfobarBannerOverlayRequestCallbackInstallerTest, ShowModal) {
  EXPECT_CALL(mock_handler_, ShowModalButtonTapped(&infobar_, &web_state_));
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<InfobarBannerShowModalResponse>());
}

// Tests that a dispatched InfobarBannerShowModalResponse calls
// InfobarBannerInteractionHandler::BannerDismissedByUser().
TEST_F(InfobarBannerOverlayRequestCallbackInstallerTest,
       UserInitiatedDismissal) {
  EXPECT_CALL(mock_handler_, BannerDismissedByUser(&infobar_));
  callback_manager()->DispatchResponse(
      OverlayResponse::CreateWithInfo<
          InfobarBannerUserInitiatedDismissalResponse>());
}
