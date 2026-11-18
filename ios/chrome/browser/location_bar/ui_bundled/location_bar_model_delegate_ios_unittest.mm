// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_model_delegate_ios.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

@interface FakeLocationBarModelDelegateWebStateProvider
    : NSObject <LocationBarModelDelegateWebStateProvider>
@property(nonatomic, assign) web::WebState* webState;
@end

@implementation FakeLocationBarModelDelegateWebStateProvider
- (web::WebState*)webStateForLocationBarModelDelegate:
    (const LocationBarModelDelegateIOS*)locationBarModelDelegate {
  return self.webState;
}
@end

class LocationBarModelDelegateIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    provider_ = [[FakeLocationBarModelDelegateWebStateProvider alloc] init];
    delegate_ = std::make_unique<LocationBarModelDelegateIOS>(provider_,
                                                              profile_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeLocationBarModelDelegateWebStateProvider* provider_;
  std::unique_ptr<LocationBarModelDelegateIOS> delegate_;
};

// Tests that IsOfflinePage() returns false.
TEST_F(LocationBarModelDelegateIOSTest, IsOfflinePage) {
  EXPECT_FALSE(delegate_->IsOfflinePage());
}
