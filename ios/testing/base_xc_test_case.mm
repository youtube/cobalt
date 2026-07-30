// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/base_xc_test_case.h"

#import "ios/testing/test_expectations.h"

@implementation BaseXCTestCase

- (void)setUp {
  [super setUp];

  // Configure expected failure if defined in the expectations file.
  NSString* className = NSStringFromClass([self class]);
  NSString* methodName = nil;
  NSRange range = [self.name rangeOfString:@" "];
  if (range.location != NSNotFound) {
    methodName = [self.name substringFromIndex:range.location + 1];
    methodName = [methodName
        stringByTrimmingCharactersInSet:
            [NSCharacterSet characterSetWithCharactersInString:@"]"]];
  }

  NSString* failureReason = nil;
  if ([[TestExpectations currentExpectations]
          shouldExpectFailureForTestCase:className
                              methodName:methodName
                               outReason:&failureReason]) {
    XCTExpectedFailureOptions* options =
        [[XCTExpectedFailureOptions alloc] init];
    options.strict = YES;
    XCTExpectFailureWithOptions(failureReason, options);
  }
}

@end
