// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class TestRegisteredWebStateListObserver : public WebStateListObserver {
 public:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override {
    switch (change.type()) {
      case WebStateListChange::Type::kStatusOnly:
        // Do nothing when a WebState is selected and its status is updated.
        break;
      case WebStateListChange::Type::kDetach:
        // Do nothing when a WebState is detached.
        break;
      case WebStateListChange::Type::kMove:
        // Do nothing when a WebState is moved.
        break;
      case WebStateListChange::Type::kReplace:
        // Do nothing when a WebState is replaced.
        break;
      case WebStateListChange::Type::kInsert:
        insertion_count_++;
        break;
    }
  }
  int insertion_count_;
};

class AllWebStateListObservationRegistrarTest : public PlatformTest {
 protected:
  AllWebStateListObservationRegistrarTest()
      : owned_observer_(std::make_unique<TestRegisteredWebStateListObserver>()),
        observer_(owned_observer_.get()) {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_list_ =
        BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  }

  void AppendNewWebState(Browser* browser) {
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    browser->GetWebStateList()->InsertWebState(
        WebStateList::kInvalidIndex, std::move(fake_web_state),
        WebStateList::INSERT_NO_FLAGS, WebStateOpener());
  }

  base::test::TaskEnvironment task_environment_;
  // Unique pointer to an observer moved into the registrar under test.
  std::unique_ptr<TestRegisteredWebStateListObserver> owned_observer_;
  // Weak pointer to the the moved observer
  TestRegisteredWebStateListObserver* observer_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BrowserList* browser_list_;
};

// Test
TEST_F(AllWebStateListObservationRegistrarTest, RegisterAllLists) {
  TestBrowser regular_browser_0(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(browser_list_,
                                                std::move(owned_observer_));
  // Should observe both insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(2, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(3, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);

  // Remove a regular browser
  browser_list_->RemoveBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);

  // Remove an incognito browser
  browser_list_->RemoveIncognitoBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(4, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, RegisterRegularLists) {
  TestBrowser regular_browser_0(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(
      browser_list_, std::move(owned_observer_),
      AllWebStateListObservationRegistrar::Mode::REGULAR);
  // Should observe only the reugular insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_0);
  // Expect no observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, RegisterIncognitoLists) {
  TestBrowser regular_browser_0(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(
      browser_list_, std::move(owned_observer_),
      AllWebStateListObservationRegistrar::Mode::INCOGNITO);
  // Should observe only the incognito insertions.
  AppendNewWebState(&regular_browser_0);
  EXPECT_EQ(0, observer_->insertion_count_);
  AppendNewWebState(&incognito_browser_0);
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second regular browser and add it.
  TestBrowser regular_browser_1(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);
  // Expect no observed insertion.
  EXPECT_EQ(1, observer_->insertion_count_);

  // Create a second incognito  browser and add it.
  TestBrowser incognito_browser_1(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  AppendNewWebState(&incognito_browser_0);
  // Expect observed insertion.
  EXPECT_EQ(2, observer_->insertion_count_);
}

TEST_F(AllWebStateListObservationRegistrarTest, DeleteWithObservers) {
  // Test that deleting a registrar with active observers is safe.
  TestBrowser regular_browser_0(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_0);

  {
    AllWebStateListObservationRegistrar registrar(browser_list_,
                                                  std::move(owned_observer_));
  }
}

// Tests that deleting the browser state is safe.
TEST_F(AllWebStateListObservationRegistrarTest, DeleteBrowserState) {
  // Create some browsers and a registrar, as above.
  TestBrowser regular_browser_0(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_0);
  TestBrowser incognito_browser_0(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  browser_list_->AddIncognitoBrowser(&incognito_browser_0);

  AllWebStateListObservationRegistrar registrar(browser_list_,
                                                std::move(owned_observer_));
  AppendNewWebState(&regular_browser_0);
  AppendNewWebState(&incognito_browser_0);
  TestBrowser regular_browser_1(chrome_browser_state_.get());
  browser_list_->AddBrowser(&regular_browser_1);
  AppendNewWebState(&regular_browser_1);

  // Now delete the browser state. Nothing should explode.
  chrome_browser_state_.reset();
}
