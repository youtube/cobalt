// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"

#import <memory>

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test object for tracking calls through the DpendencyInstalling protocol.
@interface TestInstaller : NSObject <DependencyInstalling>

@property(nonatomic) NSInteger installCount;
@property(nonatomic) NSInteger uninstallCount;

@end

@implementation TestInstaller
- (void)installDependencyForWebState:(web::WebState*)webState {
  _installCount++;
}
- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  _uninstallCount++;
}
@end

class WebStateDependencyInstallerBridgeTest : public PlatformTest,
                                              public WebStateListDelegate {
 public:
  WebStateDependencyInstallerBridgeTest()
      : web_state_list_(this), installer_([[TestInstaller alloc] init]) {}
  // WebStateListDelegate.
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}

 protected:
  WebStateList web_state_list_;
  TestInstaller* installer_;
};

// Test that inserting and replacing web states calls the dependency installer
// the expected number of times.
TEST_F(WebStateDependencyInstallerBridgeTest, InsertReplaceAndRemoveWebState) {
  WebStateDependencyInstallerBridge bridge(installer_, &web_state_list_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web_state_list_.InsertWebState(0, std::move(web_state_1),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  EXPECT_EQ(installer_.installCount, 1);
  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_list_.ReplaceWebStateAt(0, std::move(web_state_2));
  EXPECT_EQ(installer_.installCount, 2);
  EXPECT_EQ(installer_.uninstallCount, 1);
}

// Tests that deleting the installer bridge uninstalls all web states.
TEST_F(WebStateDependencyInstallerBridgeTest, UninstallOnBridgeDestruction) {
  auto bridge = std::make_unique<WebStateDependencyInstallerBridge>(
      installer_, &web_state_list_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web_state_list_.InsertWebState(0, std::move(web_state_1),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_list_.InsertWebState(0, std::move(web_state_2),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  EXPECT_EQ(installer_.installCount, 2);
  EXPECT_EQ(installer_.uninstallCount, 0);
  bridge.reset();
  EXPECT_EQ(installer_.uninstallCount, 2);
}
