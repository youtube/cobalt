// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/activity_reporter.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActivityReporterTest = PlatformTest;

// Tests that the ActivityReporter init and reporting APIs do not crash.
TEST_F(ActivityReporterTest, SmokeTest) {
  ActivityReporter* reporter =
      [[ActivityReporter alloc] initWithDomain:ActivityReportDomainTest];
  EXPECT_NE(reporter, nil);
  [reporter reportActive];
  [reporter reportInactive];
}

// Tests that the ActivityReporterWithIncognito init and reporting APIs do not
// crash.
TEST_F(ActivityReporterTest, WithIncognitoSmokeTest) {
  ActivityReporterWithIncognito* reporter =
      [[ActivityReporterWithIncognito alloc]
          initWithDomain:ActivityReportDomainTestWithIncognito];
  EXPECT_NE(reporter, nil);
  [reporter reportActiveWithIncognito:YES];
  [reporter reportActiveWithIncognito:NO];
  [reporter reportInactive];
}
