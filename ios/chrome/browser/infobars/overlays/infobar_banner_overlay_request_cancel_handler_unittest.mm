// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/infobar_banner_overlay_request_cancel_handler.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/fake_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_delegate.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// Test fixture for InfobarBannerOverlayRequestCancelHandler.
class InfobarBannerOverlayRequestCancelHandlerTest : public PlatformTest {
 public:
  InfobarBannerOverlayRequestCancelHandlerTest() {
    // Set up WebState and InfoBarManager.
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    InfobarOverlayRequestInserter::CreateForWebState(
        &web_state_, &FakeInfobarOverlayRequestFactory);
    InfoBarManagerImpl::CreateForWebState(&web_state_);
  }

  OverlayRequestQueue* queue() {
    return OverlayRequestQueue::FromWebState(&web_state_,
                                             OverlayModality::kInfobarBanner);
  }
  InfoBarManager* manager() {
    return InfoBarManagerImpl::FromWebState(&web_state_);
  }
  InfobarOverlayRequestInserter* inserter() {
    return InfobarOverlayRequestInserter::FromWebState(&web_state_);
  }

  // Returns the InfoBar used to create the front request in queue().
  InfoBarIOS* GetFrontRequestInfobar() {
    OverlayRequest* front_request = queue()->front_request();
    return front_request ? GetOverlayRequestInfobar(front_request) : nullptr;
  }

 protected:
  web::FakeWebState web_state_;
};

// Tests that the request is replaced if its corresponding InfoBar is replaced
// in its manager.
TEST_F(InfobarBannerOverlayRequestCancelHandlerTest,
       CancelForInfobarReplacement) {
  // Create an InfoBarIOS, add it to the InfoBarManager, and insert its
  // corresponding banner request.
  std::unique_ptr<InfoBarIOS> first_passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBarIOS* first_infobar = first_passed_infobar.get();
  manager()->AddInfoBar(std::move(first_passed_infobar));
  InsertParams params(first_infobar);
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(first_infobar, GetFrontRequestInfobar());
  // Replace with a new infobar and verify that the request has been replaced.
  std::unique_ptr<InfoBarIOS> second_passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBarIOS* second_infobar = second_passed_infobar.get();
  manager()->ReplaceInfoBar(first_infobar, std::move(second_passed_infobar));
  EXPECT_EQ(second_infobar, GetFrontRequestInfobar());
  EXPECT_EQ(1U, queue()->size());
}

// Tests that the request is cancelled after all modals originating from the
// banner have been completed.
TEST_F(InfobarBannerOverlayRequestCancelHandlerTest, CancelForModalCompletion) {
  // Create an InfoBarIOS, add it to the InfoBarManager, and insert its
  // corresponding banner request.
  std::unique_ptr<InfoBarIOS> passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBarIOS* infobar = passed_infobar.get();
  manager()->AddInfoBar(std::move(passed_infobar));
  InsertParams params(infobar);
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(infobar, GetFrontRequestInfobar());
  // Insert a modal request for the infobar, then cancel it, verifying that the
  // banner request is cancelled alongside it.
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kBanner;
  inserter()->InsertOverlayRequest(params);
  OverlayRequestQueue::FromWebState(&web_state_, OverlayModality::kInfobarModal)
      ->CancelAllRequests();

  EXPECT_FALSE(queue()->front_request());
}

// Test that the request is not cancelled if a modal that isn't presented from
// the banner completes.
TEST_F(InfobarBannerOverlayRequestCancelHandlerTest,
       NoCancelForDifferentModalCompletion) {
  // Create an InfoBarIOS, add it to the InfoBarManager, and insert its
  // corresponding banner request.
  std::unique_ptr<InfoBarIOS> passed_infobar =
      std::make_unique<FakeInfobarIOS>();
  InfoBarIOS* infobar = passed_infobar.get();
  manager()->AddInfoBar(std::move(passed_infobar));
  InsertParams params(infobar);
  params.overlay_type = InfobarOverlayType::kBanner;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarManager;
  inserter()->InsertOverlayRequest(params);
  ASSERT_EQ(infobar, GetFrontRequestInfobar());

  // Insert a modal request for a source that isn't the banner, then cancel it.
  params.overlay_type = InfobarOverlayType::kModal;
  params.insertion_index = 0;
  params.source = InfobarOverlayInsertionSource::kInfoBarDelegate;
  inserter()->InsertOverlayRequest(params);
  OverlayRequestQueue::FromWebState(&web_state_, OverlayModality::kInfobarModal)
      ->CancelAllRequests();

  EXPECT_TRUE(queue()->front_request());
}
