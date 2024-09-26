// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/crashpad/crashpad/client/annotation_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests that CrashReporterBreadcrumbObserver attaches observed breadcrumb
// events to crash reports.
class CrashReporterBreadcrumbObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    // Ensure the CrashReporterBreadcrumbObserver singleton is created
    // and registered.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    TestChromeBrowserState::Builder test_cbs_builder_2;
    chrome_browser_state_2_ = test_cbs_builder_2.Build();
  }

  void TearDown() override {
    crash_helper::SetEnabled(false);
    PlatformTest::TearDown();
  }

  std::string BreadcrumbAnnotations() {
    auto* annotations = crashpad::AnnotationList::Get();
    if (!annotations || annotations->begin() == annotations->end()) {
      EXPECT_TRUE(false);
      return "";
    }

    for (const crashpad::Annotation* annotation : *annotations) {
      if (!annotation->is_set())
        continue;

      if (annotation->type() != crashpad::Annotation::Type::kString)
        continue;

      const std::string kBreadcrumbs("breadcrumbs");
      if (annotation->name() != kBreadcrumbs)
        continue;

      base::StringPiece cp_value(static_cast<const char*>(annotation->value()),
                                 annotation->size());
      return std::string(cp_value);
    }
    EXPECT_TRUE(false);
    return "";
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_2_;
};

// Tests that breadcrumb events logged to a single BreadcrumbManagerKeyedService
// are collected by the CrashReporterBreadcrumbObserver and attached to crash
// reports.
TEST_F(CrashReporterBreadcrumbObserverTest, EventsAttachedToCrashReport) {
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
      chrome_browser_state_.get())
      ->AddEvent("Breadcrumb Event");
  const auto& events =
      breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
  std::string expected_breadcrumbs;
  for (const auto& event : events) {
    expected_breadcrumbs += event + "\n";
  }
  EXPECT_EQ(BreadcrumbAnnotations(), expected_breadcrumbs);
}

// Tests that breadcrumbs string is cut when it exceeds the max allowed length.
TEST_F(CrashReporterBreadcrumbObserverTest, ProductDataOverflow) {
  crash_helper::SetEnabled(true);

  // Build a sample breadcrumbs string greater than the maximum allowed size.
  NSMutableString* breadcrumbs = [[NSMutableString alloc] init];
  while (breadcrumbs.length < breadcrumbs::kMaxDataLength) {
    [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  }
  [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  ASSERT_GT([breadcrumbs length], breadcrumbs::kMaxDataLength);

  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
      chrome_browser_state_.get())
      ->AddEvent(base::SysNSStringToUTF8(breadcrumbs));
  EXPECT_EQ(BreadcrumbAnnotations().size(), breadcrumbs::kMaxDataLength);
}

// Tests that breadcrumb events logged to multiple BreadcrumbManagerKeyedService
// instances are collected by the CrashReporterBreadcrumbObserver and attached
// to crash reports.
TEST_F(CrashReporterBreadcrumbObserverTest,
       MultipleBrowserStatesAttachedToCrashReport) {
  crash_helper::SetEnabled(true);

  const std::string event = "Breadcrumb Event";
  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* otr_breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_->GetOffTheRecordChromeBrowserState());
  otr_breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_2 =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_2_.get());
  breadcrumb_service_2->AddEvent(event);

  std::string breadcrumbs = BreadcrumbAnnotations();
  // 1 incognito
  EXPECT_NE(breadcrumbs.find("I Breadcrumb Event"), std::string::npos);
  // 3 total
  auto iter = breadcrumbs.find(event);
  int count = 0;
  while (iter != std::string::npos) {
    count++;
    iter = breadcrumbs.find(event, iter + 1);
  }
  EXPECT_EQ(count, 3);
}
