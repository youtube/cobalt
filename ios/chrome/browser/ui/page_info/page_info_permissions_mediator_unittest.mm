// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_permissions_mediator.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#import "ios/web/common/features.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests for Permissions mediator for the page info.
class PageInfoPermissionsTest : public PlatformTest {
 protected:
  PageInfoPermissionsTest() {
    feature_list_.InitAndEnableFeature(web::features::kMediaPermissionsControl);
  }

  ~PageInfoPermissionsTest() override {
    if (@available(iOS 15.0, *)) {
      [mediator_ disconnect];
    }
  }

  void SetUp() override {
    PlatformTest::SetUp();

    if (@available(iOS 15.0, *)) {
      fake_web_state_ = std::make_unique<web::FakeWebState>();
      web::WebState* web_state_ = fake_web_state_.get();

      // Initialize camera state to Allowed but keeps microphone state
      // NotAccessible.
      web_state_->SetStateForPermission(web::PermissionStateAllowed,
                                        web::PermissionCamera);
      web_state_->SetStateForPermission(web::PermissionStateNotAccessible,
                                        web::PermissionMicrophone);

      mediator_ =
          [[PageInfoPermissionsMediator alloc] initWithWebState:web_state_];
    }
  }

  PageInfoPermissionsMediator* mediator() API_AVAILABLE(ios(15.0)) {
    return mediator_;
  }

  web::WebState* web_state() { return fake_web_state_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<web::FakeWebState> fake_web_state_;
  PageInfoPermissionsMediator* mediator_ API_AVAILABLE(ios(15.0));
};

// Verifies that `updateStateForPermission:` updates correctly the web state
// permission.
TEST_F(PageInfoPermissionsTest, TestUpdateStateForPermission) {
  if (@available(iOS 15.0, *)) {
    PermissionInfo* permissionDescription = [[PermissionInfo alloc] init];
    permissionDescription.permission = web::PermissionCamera;
    permissionDescription.state = web::PermissionStateBlocked;

    [mediator() updateStateForPermission:permissionDescription];
    ASSERT_EQ(web_state()->GetStateForPermission(web::PermissionCamera),
              web::PermissionStateBlocked);
  }
}
