// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_BASE_XC_TEST_CASE_H_
#define IOS_TESTING_BASE_XC_TEST_CASE_H_

#import <XCTest/XCTest.h>

// Base class for all XCTests in iOS.
// Automatically configures expected failures based on the expectations file.
@interface BaseXCTestCase : XCTestCase

// Invoked upon starting each test method in a test case.
- (void)setUp NS_REQUIRES_SUPER;

@end

#endif  // IOS_TESTING_BASE_XC_TEST_CASE_H_
