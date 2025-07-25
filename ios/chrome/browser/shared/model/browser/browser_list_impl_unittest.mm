// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_impl.h"

#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser_list_observer.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class BrowserListImplTest : public PlatformTest {
 protected:
  BrowserListImplTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_list_ =
        BrowserListFactory::GetForBrowserState(chrome_browser_state_.get());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BrowserList* browser_list_;
};

// Test main add/remove logic.
TEST_F(BrowserListImplTest, AddRemoveBrowsers) {
  // Browser list should start empty
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());

  TestBrowser browser_1(chrome_browser_state_.get());

  // Adding a browser should result in it appearing in the list.
  browser_list_->AddBrowser(&browser_1);
  std::set<Browser*> browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());
  auto found_browser = browsers.find(&browser_1);
  EXPECT_EQ(&browser_1, *found_browser);

  TestBrowser browser_2(chrome_browser_state_.get());

  // Removing a browser not in the list is a no-op.
  browser_list_->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // More than one browset can be added to the list.
  browser_list_->AddBrowser(&browser_2);
  EXPECT_EQ(2UL, browser_list_->AllRegularBrowsers().size());

  // Removing a browser works -- the list gets smaller, and the removed browser
  // isn't on it.
  browser_list_->RemoveBrowser(&browser_2);
  browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());
  found_browser = browsers.find(&browser_2);
  EXPECT_EQ(browsers.end(), found_browser);

  // Removing a browser a second time does nothing.
  browser_list_->RemoveBrowser(&browser_2);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // Removing the last browser, even multiple times, works as expected.
  browser_list_->RemoveBrowser(&browser_1);
  browser_list_->RemoveBrowser(&browser_1);
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
}

// Test regular/incognito interactions
TEST_F(BrowserListImplTest, AddRemoveIncognitoBrowsers) {
  // Verify that the "OTR browser list" is the same as the regular one.
  BrowserList* otr_browser_list = BrowserListFactory::GetForBrowserState(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  EXPECT_EQ(otr_browser_list, browser_list_);

  // Incognito browser list starts empty.
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());

  TestBrowser browser_1(chrome_browser_state_.get());

  ChromeBrowserState* incognito_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  TestBrowser incognito_browser_1(incognito_browser_state);

  // Adding a regular browser doesn't affect the incognito list.
  browser_list_->AddBrowser(&browser_1);
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());

  // Adding an incognito browser doesn't affect the regular list.
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(1UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // An added incognito browser is in the list.
  std::set<Browser*> browsers = browser_list_->AllIncognitoBrowsers();
  auto found_browser = browsers.find(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, *found_browser);

  // Removing browsers from the wrong list has no effect.
  browser_list_->RemoveBrowser(&incognito_browser_1);
  browser_list_->RemoveIncognitoBrowser(&browser_1);
  EXPECT_EQ(1UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

  // Removing browsers from the correct lists works as expected.
  browser_list_->RemoveBrowser(&browser_1);
  browser_list_->RemoveIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
}

// Test that destroyed browsers are auto-removed.
TEST_F(BrowserListImplTest, AutoRemoveBrowsers) {
  {
    // Create and add scoped browsers
    TestBrowser browser_1(chrome_browser_state_.get());
    browser_list_->AddBrowser(&browser_1);
    EXPECT_EQ(1UL, browser_list_->AllRegularBrowsers().size());

    ChromeBrowserState* incognito_browser_state =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    TestBrowser incognito_browser_1(incognito_browser_state);
    browser_list_->AddIncognitoBrowser(&incognito_browser_1);
    EXPECT_EQ(1UL, browser_list_->AllIncognitoBrowsers().size());
  }

  // Expect that the browsers going out of scope will have triggered removal.
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
  EXPECT_EQ(0UL, browser_list_->AllIncognitoBrowsers().size());
}

// Test that values returned from AllRegularBrowsers and AllIncognitoBrowsers
// aren't affected by subsequent changes to the browser list.
TEST_F(BrowserListImplTest, AllBrowserValuesDontChange) {
  TestBrowser browser_1(chrome_browser_state_.get());

  // Add a browser and get the current set of browsers.
  browser_list_->AddBrowser(&browser_1);
  std::set<Browser*> browsers = browser_list_->AllRegularBrowsers();
  EXPECT_EQ(1UL, browsers.size());

  // Remove the browser.
  browser_list_->RemoveBrowser(&browser_1);
  EXPECT_EQ(0UL, browser_list_->AllRegularBrowsers().size());
}

// Check that an observer is informed of additions and removals to both the
// regular and incognito browser lists.
TEST_F(BrowserListImplTest, BrowserListObserver) {
  TestBrowserListObserver observer;
  browser_list_->AddObserver(&observer);

  TestBrowser browser_1(chrome_browser_state_.get());
  ChromeBrowserState* incognito_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  TestBrowser incognito_browser_1(incognito_browser_state);

  // Check that a regular addition is observed.
  browser_list_->AddBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastAddedBrowser());
  EXPECT_EQ(1UL, observer.GetLastBrowsers().size());

  // Check that a no-op  removal is *not* observed.
  browser_list_->RemoveBrowser(&incognito_browser_1);
  EXPECT_EQ(nullptr, observer.GetLastRemovedBrowser());

  // Check that a regular removal is observed.
  browser_list_->RemoveBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastRemovedBrowser());
  EXPECT_EQ(0UL, observer.GetLastBrowsers().size());

  // Check that an incognito addition is observed.
  browser_list_->AddIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, observer.GetLastAddedIncognitoBrowser());
  EXPECT_EQ(1UL, observer.GetLastIncognitoBrowsers().size());

  // Check that an incognito removal is observed.
  browser_list_->RemoveIncognitoBrowser(&incognito_browser_1);
  EXPECT_EQ(&incognito_browser_1, observer.GetLastRemovedIncognitoBrowser());
  EXPECT_EQ(0UL, observer.GetLastIncognitoBrowsers().size());

  browser_list_->RemoveObserver(&observer);
}

// Check that deleting the ChromeBrowserState, destroy the BrowserList and
// informs the observer. TestBrowserListObserver knows to remove itself as
// an Observer when BrowserList::OnBrowserListShutdown() is called.
TEST_F(BrowserListImplTest, DeleteBrowserState) {
  TestBrowserListObserver observer;
  browser_list_->AddObserver(&observer);
  TestBrowser browser_1(chrome_browser_state_.get());
  browser_list_->AddBrowser(&browser_1);

  // Now delete the browser state. Nothing should explode.
  chrome_browser_state_.reset();
}

// Checks that the BrowserList is still functional after the destruction of
// the off-the-record ChromeBrowserState (since this happen during normal
// use of the application).
TEST_F(BrowserListImplTest, ShutdownOTRBrowserState) {
  TestBrowserListObserver observer;
  browser_list_->AddObserver(&observer);

  TestBrowser browser_1(chrome_browser_state_.get());

  // Use a block to ensure that the Browser pointing to the off-the-record
  // ChromeBrowserState does not outlive the object (which would cause an
  // use-after-free access in when BrowserList is informed of the Browser
  // destruction).
  {
    ChromeBrowserState* incognito_browser_state =
        chrome_browser_state_->GetOffTheRecordChromeBrowserState();
    TestBrowser incognito_browser_1(incognito_browser_state);
    browser_list_->AddIncognitoBrowser(&incognito_browser_1);

    // Check that adding/removing incognito is observed.
    EXPECT_EQ(&incognito_browser_1, observer.GetLastAddedIncognitoBrowser());
    EXPECT_EQ(1UL, observer.GetLastIncognitoBrowsers().size());

    browser_list_->AddBrowser(&browser_1);
    // Check that a regular addition is observed.
    EXPECT_EQ(&browser_1, observer.GetLastAddedBrowser());
    EXPECT_EQ(1UL, observer.GetLastBrowsers().size());
  }

  // Destroy the off-the-record ChromeBrowserState.
  chrome_browser_state_->DestroyOffTheRecordChromeBrowserState();
  ASSERT_FALSE(chrome_browser_state_->HasOffTheRecordChromeBrowserState());

  TestBrowser browser_2(chrome_browser_state_.get());
  browser_list_->AddBrowser(&browser_2);
  // Check that another regular addition is observed.
  EXPECT_EQ(&browser_2, observer.GetLastAddedBrowser());
  EXPECT_EQ(2UL, observer.GetLastBrowsers().size());

  // Check that a regular removal is observed.
  browser_list_->RemoveBrowser(&browser_1);
  EXPECT_EQ(&browser_1, observer.GetLastRemovedBrowser());
  EXPECT_EQ(1UL, observer.GetLastBrowsers().size());

  browser_list_->RemoveObserver(&observer);
}
