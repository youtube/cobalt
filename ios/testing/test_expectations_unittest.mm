// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/test_expectations.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TestExpectationsTest = PlatformTest;

TEST_F(TestExpectationsTest, ParseSimpleExpectation) {
  NSString* content = @"MyTestCase/testMethod [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  NSString* reason = nil;
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:&reason]);
  EXPECT_NSEQ(@"Expected failure", reason);

  EXPECT_FALSE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                 methodName:@"otherMethod"
                                                  outReason:nil]);
}

TEST_F(TestExpectationsTest, ParseWithBugIdentifier) {
  NSString* content = @"crbug.com/12345 MyTestCase/testMethod [ Failure ]\n"
                      @"b/98765 MyTestCase/testOtherMethod [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  NSString* reason = nil;
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:&reason]);
  EXPECT_NSEQ(@"crbug.com/12345", reason);

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testOtherMethod"
                                                 outReason:&reason]);
  EXPECT_NSEQ(@"b/98765", reason);
}

TEST_F(TestExpectationsTest, ParseWithMatchingTags) {
  NSString* content = @"[ iOS26 Simulator ] MyTestCase/testMethod [ Failure ]\n"
                      @"[ iOS18 ] MyTestCase/testOtherMethod [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS", @"iOS26",
                                                            @"Simulator", nil]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:nil]);

  // iOS18 tag doesn't match active tags (iOS26), so the expectation is ignored.
  EXPECT_FALSE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                 methodName:@"testOtherMethod"
                                                  outReason:nil]);
}

TEST_F(TestExpectationsTest, ClassLevelExpectation) {
  NSString* content = @"MyTestCase [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:nil]);
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"anotherMethod"
                                                 outReason:nil]);
}

TEST_F(TestExpectationsTest, Normalization) {
  NSString* content = @"MyTestCase.testMethod [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:nil]);
}

TEST_F(TestExpectationsTest, CommentsAndBlankLines) {
  NSString* content =
      @"# This is a comment\n"
      @"\n"
      @"crbug.com/123 MyTestCase/testMethod [ Failure ] # inline comment\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations setOverrideActiveTagsForTesting:[NSSet setWithObject:@"iOS"]];

  NSString* reason = nil;
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod"
                                                 outReason:&reason]);
  EXPECT_NSEQ(@"crbug.com/123", reason);
}

// Verify that tags, build numbers, and the word 'Failure' are matched
// case-insensitively.
TEST_F(TestExpectationsTest, CaseInsensitiveMatching) {
  NSString* content =
      @"[ ios26 simulator ] MyTestCase/testMethod1 [ failure ]\n"
      @"[ 23f5067a ] MyTestCase/testMethod2 [ FAILURE ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS26",
                                                            @"Simulator",
                                                            @"23F5067a", nil]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod1"
                                                 outReason:nil]);
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod2"
                                                 outReason:nil]);
}

TEST_F(TestExpectationsTest, MinorAndPatchOSVersionMatching) {
  NSString* content = @"[ iOS18.2 ] MyTestCase/testMethod1 [ Failure ]\n"
                      @"[ iOS18.2.1 ] MyTestCase/testMethod2 [ Failure ]\n"
                      @"[ iOS18.3 ] MyTestCase/testMethod3 [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS", @"iOS18",
                                                            @"iOS18.2",
                                                            @"iOS18.2.1", nil]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod1"
                                                 outReason:nil]);
  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod2"
                                                 outReason:nil]);
  EXPECT_FALSE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                 methodName:@"testMethod3"
                                                  outReason:nil]);
}

TEST_F(TestExpectationsTest, BuildNumberMatching) {
  NSString* content = @"[ 17F42 ] MyTestCase/testMethod1 [ Failure ]\n"
                      @"[ 18A5301 ] MyTestCase/testMethod2 [ Failure ]\n";
  TestExpectations* expectations =
      [[TestExpectations alloc] initWithContent:content];
  [expectations
      setOverrideActiveTagsForTesting:[NSSet setWithObjects:@"iOS26", @"17F42",
                                                            nil]];

  EXPECT_TRUE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                methodName:@"testMethod1"
                                                 outReason:nil]);
  EXPECT_FALSE([expectations shouldExpectFailureForTestCase:@"MyTestCase"
                                                 methodName:@"testMethod2"
                                                  outReason:nil]);
}
