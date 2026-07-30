// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_delegate.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/platform_test.h"

@interface BestFeaturesFirstRunScreenTestDelegate
    : NSObject <FirstRunScreenDelegate>

@property(nonatomic, strong) ChromeCoordinator* stoppedCoordinator;

@end

@implementation BestFeaturesFirstRunScreenTestDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
  self.stoppedCoordinator = coordinator;
}

@end

class BestFeaturesScreenCoordinatorTest : public PlatformTest {
 protected:
  BestFeaturesScreenCoordinatorTest() {
    scoped_feature_list_.InitAndEnableFeature(
        first_run::kBestFeaturesScreenInFirstRun);

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    navigation_controller_ = [[UINavigationController alloc] init];
    delegate_ = [[BestFeaturesFirstRunScreenTestDelegate alloc] init];

    coordinator_ = [[BestFeaturesScreenCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()
                                delegate:delegate_];
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UINavigationController* navigation_controller_;
  BestFeaturesFirstRunScreenTestDelegate* delegate_;
  BestFeaturesScreenCoordinator* coordinator_;
};

// Tests that when a detail screen completes, the coordinator stops the detail
// coordinator and notifies its FirstRunScreenDelegate.
TEST_F(BestFeaturesScreenCoordinatorTest, DetailScreenCompletion) {
  [coordinator_ start];

  BestFeaturesItem* item =
      [[BestFeaturesItem alloc] initWithType:BestFeaturesItemType::kLensSearch];

  id<BestFeaturesDelegate> best_features_delegate =
      (id<BestFeaturesDelegate>)coordinator_;
  [best_features_delegate didTapBestFeaturesItem:item];

  id<FirstRunScreenDelegate> screen_delegate =
      (id<FirstRunScreenDelegate>)coordinator_;
  ChromeCoordinator* detail_coordinator =
      [coordinator_ valueForKey:@"_detailScreenCoordinator"];
  ASSERT_NE(detail_coordinator, nil);

  [screen_delegate
      firstRunScreenCoordinatorWantsToBeStopped:detail_coordinator];

  EXPECT_EQ(delegate_.stoppedCoordinator, coordinator_);

  [coordinator_ stop];
}
