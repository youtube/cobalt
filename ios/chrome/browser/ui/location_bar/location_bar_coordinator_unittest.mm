// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#import <memory>
#import <string>
#import <vector>

#import "components/omnibox/browser/test_location_bar_model.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_classifier_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using variations::VariationsIdsProvider;

@interface TestOmniboxFocusDelegate : NSObject <OmniboxFocusDelegate>

@end

@implementation TestOmniboxFocusDelegate {
  std::unique_ptr<LocationBarModel> _model;
}

- (void)omniboxDidBecomeFirstResponder {
}
- (void)omniboxDidResignFirstResponder {
}

- (LocationBarModel*)locationBarModel {
  if (!_model) {
    _model = std::make_unique<TestLocationBarModel>();
  }

  return _model.get();
}

@end

namespace {

class LocationBarCoordinatorTest : public PlatformTest {
 protected:
  LocationBarCoordinatorTest()
      : scene_state_([[SceneState alloc] initWithAppState:nil]) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;

    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::AutocompleteClassifierFactory::GetInstance(),
        ios::AutocompleteClassifierFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    FakeUrlLoadingBrowserAgent::InjectForBrowser(browser_.get());

    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetCurrentURL(GURL("http://test/"));
    browser_->GetWebStateList()->InsertWebState(
        0, std::move(web_state), WebStateList::INSERT_FORCE_INDEX,
        WebStateOpener());

    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();

    id mockQrScannerCommandHandler =
        OCMProtocolMock(@protocol(QRScannerCommands));
    [dispatcher startDispatchingToTarget:mockQrScannerCommandHandler
                             forProtocol:@protocol(QRScannerCommands)];
    id mockLensCommandsHandler = OCMProtocolMock(@protocol(LensCommands));
    [dispatcher startDispatchingToTarget:mockLensCommandsHandler
                             forProtocol:@protocol(LensCommands)];
    id mockHelpCommandsHandler = OCMProtocolMock(@protocol(HelpCommands));
    [dispatcher startDispatchingToTarget:mockHelpCommandsHandler
                             forProtocol:@protocol(HelpCommands)];
    id mockToolbarCommandsHandler = OCMProtocolMock(@protocol(ToolbarCommands));
    [dispatcher startDispatchingToTarget:mockToolbarCommandsHandler
                             forProtocol:@protocol(ToolbarCommands)];

    // Set up ApplicationCommands mock. Because ApplicationCommands conforms
    // to ApplicationSettingsCommands, that needs to be mocked and dispatched
    // as well.
    id mockApplicationCommandHandler =
        OCMProtocolMock(@protocol(ApplicationCommands));
    id mockApplicationSettingsCommandHandler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [dispatcher startDispatchingToTarget:mockApplicationCommandHandler
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher
        startDispatchingToTarget:mockApplicationSettingsCommandHandler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    delegate_ = [[TestOmniboxFocusDelegate alloc] init];

    coordinator_ = [[LocationBarCoordinator alloc]

        initWithBrowser:browser_.get()];
    coordinator_.delegate = delegate_;
  }

  void TearDown() override {
    // Started coordinator has to be stopped before WebStateList destruction.
    [coordinator_ stop];

    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  LocationBarCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  SceneState* scene_state_;
  TestOmniboxFocusDelegate* delegate_;
};

TEST_F(LocationBarCoordinatorTest, Stops) {
  EXPECT_TRUE(coordinator_.locationBarViewController == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.locationBarViewController != nil);
  [coordinator_ stop];
  EXPECT_TRUE(coordinator_.locationBarViewController == nil);
}

// Removes the existing WebState to ensure that nothing breaks when there is no
// active WebState.
TEST_F(LocationBarCoordinatorTest, RemoveLastWebState) {
  browser_->GetWebStateList()->CloseWebStateAt(0, 0);
}

// Calls -loadGURLFromLocationBar:transition: with https://www.google.com/ URL.
// Verifies that URLLoader receives correct load request, which also includes
// variations header.
TEST_F(LocationBarCoordinatorTest, LoadGoogleUrl) {
  ASSERT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            VariationsIdsProvider::GetInstance()->ForceVariationIds(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));

  GURL url("https://www.google.com/");
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  WindowOpenDisposition disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url
                                 postContent:nil
                                  transition:transition
                                 disposition:disposition
      destination_url_entered_without_scheme:false];

  FakeUrlLoadingBrowserAgent* url_loader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(browser_.get()));
  EXPECT_EQ(url, url_loader->last_params.web_params.url);
  EXPECT_TRUE(url_loader->last_params.web_params.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault,
            url_loader->last_params.web_params.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      transition, url_loader->last_params.web_params.transition_type));
  EXPECT_FALSE(url_loader->last_params.web_params.is_renderer_initiated);
  ASSERT_EQ(1U, url_loader->last_params.web_params.extra_headers.count);
  EXPECT_GT([url_loader->last_params.web_params.extra_headers[@"X-Client-Data"]
                length],
            0U);
  EXPECT_EQ(disposition, url_loader->last_params.disposition);
}

// Calls -loadGURLFromLocationBar:transition: with https://www.nongoogle.com/
// URL. Verifies that URLLoader receives correct load request without variations
// header.
TEST_F(LocationBarCoordinatorTest, LoadNonGoogleUrl) {
  ASSERT_EQ(VariationsIdsProvider::ForceIdsResult::SUCCESS,
            VariationsIdsProvider::GetInstance()->ForceVariationIds(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));

  GURL url("https://www.nongoogle.com/");
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url
                                 postContent:nil
                                  transition:transition
                                 disposition:disposition
      destination_url_entered_without_scheme:false];

  FakeUrlLoadingBrowserAgent* url_loader =
      FakeUrlLoadingBrowserAgent::FromUrlLoadingBrowserAgent(
          UrlLoadingBrowserAgent::FromBrowser(browser_.get()));

  EXPECT_EQ(url, url_loader->last_params.web_params.url);
  EXPECT_TRUE(url_loader->last_params.web_params.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault,
            url_loader->last_params.web_params.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      transition, url_loader->last_params.web_params.transition_type));
  EXPECT_FALSE(url_loader->last_params.web_params.is_renderer_initiated);
  ASSERT_EQ(0U, url_loader->last_params.web_params.extra_headers.count);
  EXPECT_EQ(disposition, url_loader->last_params.disposition);
}

}  // namespace
