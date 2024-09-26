// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"

#import <memory>
#import <set>

#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// DependencyInstaller which simply tracks which WebStates have been passed to
// the install/uninstall methods.
class FakeDependencyInstaller : public DependencyInstaller {
 public:
  void InstallDependency(web::WebState* web_state) override {
    installed_.insert(web_state);
  }
  void UninstallDependency(web::WebState* web_state) override {
    uninstalled_.insert(web_state);
  }
  ~FakeDependencyInstaller() override {}

  bool WasInstalled(web::WebState* web_state) const {
    return installed_.count(web_state) > 0;
  }

  bool WasUninstalled(web::WebState* web_state) const {
    return uninstalled_.count(web_state) > 0;
  }

  size_t InstallCount(web::WebState* web_state) const {
    return installed_.count(web_state);
  }

  size_t UninstallCount(web::WebState* web_state) const {
    return uninstalled_.count(web_state);
  }

 private:
  std::set<web::WebState*> installed_;
  std::set<web::WebState*> uninstalled_;
};

class WebStateDependencyInstallationObserverTest : public PlatformTest,
                                                   public WebStateListDelegate {
 public:
  WebStateDependencyInstallationObserverTest() : web_state_list_(this) {}

  // WebStateListDelegate.
  void WillAddWebState(web::WebState* web_state) override {}
  void WebStateDetached(web::WebState* web_state) override {}

 protected:
  WebStateList web_state_list_;
  FakeDependencyInstaller installer_;
};

// Verifies that the WebStateDependencyInstallationObserver triggers the
// appropriate install/uninstall methods when a WebState is inserted, replaced,
// or removed.
TEST_F(WebStateDependencyInstallationObserverTest,
       InsertReplaceAndRemoveWebState) {
  WebStateDependencyInstallationObserver observer(&web_state_list_,
                                                  &installer_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();

  EXPECT_FALSE(installer_.WasInstalled(web_state_1_raw));
  web_state_list_.InsertWebState(0, std::move(web_state_1),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));
  EXPECT_FALSE(installer_.WasUninstalled(web_state_1_raw));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_2_raw = web_state_2.get();
  web_state_list_.ReplaceWebStateAt(0, std::move(web_state_2));
  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_raw));
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_raw));
}

// Verifies that the WebStateDependencyInstallationObserver triggers the
// appropriate install method for any WebStates that were already in the
// WebStateList prior to its construction.
TEST_F(WebStateDependencyInstallationObserverTest,
       RespectsPreexistingWebState) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_raw = web_state.get();
  web_state_list_.InsertWebState(
      0, std::move(web_state), WebStateList::INSERT_ACTIVATE, WebStateOpener());
  WebStateDependencyInstallationObserver observer(&web_state_list_,
                                                  &installer_);
  EXPECT_TRUE(installer_.WasInstalled(web_state_raw));
}

// Verifies that unrealized web states don't get dependencies installed.
TEST_F(WebStateDependencyInstallationObserverTest, UnrealizedWebStates) {
  WebStateDependencyInstallationObserver observer(&web_state_list_,
                                                  &installer_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();
  web_state_list_.InsertWebState(0, std::move(web_state_1),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));

  auto web_state_2 = std::make_unique<web::FakeWebState>();
  web_state_2->SetIsRealized(false);
  web::WebState* web_state_2_raw = web_state_2.get();

  // Insert the unrealized webstate but don't have it activate (since that
  // forces realization).
  web_state_list_.InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_NO_FLAGS,
                                 WebStateOpener());
  // The unrealized webstate should not have dependencies installed.
  EXPECT_FALSE(installer_.WasInstalled(web_state_2_raw));
  // Once realized, dependencioes should be installed.
  web_state_2_raw->ForceRealized();
  EXPECT_TRUE(installer_.WasInstalled(web_state_2_raw));

  auto web_state_3 = std::make_unique<web::FakeWebState>();
  web_state_3->SetIsRealized(false);
  web::WebState* web_state_3_raw = web_state_3.get();

  // Insert the unrealized webstate and activate it, forcing realization.
  web_state_list_.InsertWebState(2, std::move(web_state_3),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  // The formerly unrealized webstate should have dependencies installed.
  // (The webstate should also be realized, but that's not the responsibility
  // of the code under test).
  EXPECT_TRUE(installer_.WasInstalled(web_state_3_raw));
  // Dependencies should have been installed only once
  EXPECT_EQ(1u, installer_.InstallCount(web_state_3_raw));
}

// Verifies that destroying the observer triggers uninstallation.
TEST_F(WebStateDependencyInstallationObserverTest, Disconnect) {
  auto observer = std::make_unique<WebStateDependencyInstallationObserver>(
      &web_state_list_, &installer_);
  auto web_state_1 = std::make_unique<web::FakeWebState>();
  web::WebState* web_state_1_raw = web_state_1.get();

  web_state_list_.InsertWebState(0, std::move(web_state_1),
                                 WebStateList::INSERT_ACTIVATE,
                                 WebStateOpener());
  EXPECT_TRUE(installer_.WasInstalled(web_state_1_raw));
  observer.reset();

  EXPECT_TRUE(installer_.WasUninstalled(web_state_1_raw));
}
