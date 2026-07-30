// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_TEST_EXPECTATIONS_H_
#define IOS_TESTING_TEST_EXPECTATIONS_H_

#import <Foundation/Foundation.h>

typedef NS_OPTIONS(NSUInteger, TestExpectationType) {
  TestExpectationTypeNone = 0,
  TestExpectationTypeFailure = 1 << 0,
  TestExpectationTypePass = 1 << 1,
  TestExpectationTypeSkip = 1 << 2,
  TestExpectationTypeCrash = 1 << 3,
};

@interface TestExpectationEntry : NSObject
@property(nonatomic, copy) NSString* bug;
@property(nonatomic, assign) TestExpectationType type;
@end

// Helper class to manage and check expected failures for XCTests.
@interface TestExpectations : NSObject

// Designated initializer. Parses expectations from the given string content.
- (instancetype)initWithContent:(NSString*)content NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Convenience initializer. Loads and parses expectations from the given file
// path. Returns nil if the file could not be read.
+ (instancetype)expectationsWithFilePath:(NSString*)path;

// Returns the current expectations instance (if set).
+ (instancetype)currentExpectations;

// Sets the current expectations instance.
+ (void)setCurrentExpectations:(TestExpectations*)expectations;

// Returns the expectation entry for the given test case and method, or nil if
// no expectation exists.
- (TestExpectationEntry*)expectationEntryForTestCase:(NSString*)testClassName
                                          methodName:(NSString*)methodName;

@end

// Testing-only category.
@interface TestExpectations (Testing)

// Sets the active tags to override the default system tags during testing.
- (void)setOverrideActiveTagsForTesting:(NSSet<NSString*>*)tags;

@end

#endif  // IOS_TESTING_TEST_EXPECTATIONS_H_
