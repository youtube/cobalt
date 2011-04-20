// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#import "base/mac/objc_property_releaser.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

// "When I'm alone, I count myself."
//   --Count von Count, http://www.youtube.com/watch?v=FKzszqa9WA4

// The number of CountVonCounts outstanding.
static int ah_ah_ah;

@interface CountVonCount : NSObject<NSCopying>

+ (CountVonCount*)countVonCount;

@end  // @interface CountVonCount

@implementation CountVonCount

+ (CountVonCount*)countVonCount {
  return [[[CountVonCount alloc] init] autorelease];
}

- (id)init {
  ++ah_ah_ah;
  return [super init];
}

- (void)dealloc {
  --ah_ah_ah;
  [super dealloc];
}

- (id)copyWithZone:(NSZone*)zone {
  return [[CountVonCount allocWithZone:zone] init];
}

@end  // @implementation CountVonCount

@interface ObjCPropertyTestBase : NSObject {
 @private
  CountVonCount* cvcBaseRetain_;
  CountVonCount* cvcBaseCopy_;
  CountVonCount* cvcBaseAssign_;
  CountVonCount* cvcBaseNotProperty_;
  base::mac::ObjCPropertyReleaser propertyReleaser_ObjCPropertyTestBase_;
}

@property(retain, nonatomic) CountVonCount* cvcBaseRetain;
@property(copy, nonatomic) CountVonCount* cvcBaseCopy;
@property(assign, nonatomic) CountVonCount* cvcBaseAssign;

- (void)setCvcBaseNotProperty:(CountVonCount*)cvc;

@end  // @interface ObjCPropertyTestBase

@implementation ObjCPropertyTestBase

@synthesize cvcBaseRetain = cvcBaseRetain_;
@synthesize cvcBaseCopy = cvcBaseCopy_;
@synthesize cvcBaseAssign = cvcBaseAssign_;

- (id)init {
  if ((self = [super init])) {
    propertyReleaser_ObjCPropertyTestBase_.Init(
        self, [ObjCPropertyTestBase class]);
  }
  return self;
}

- (void)dealloc {
  [cvcBaseNotProperty_ release];
  [super dealloc];
}

- (void)setCvcBaseNotProperty:(CountVonCount*)cvc {
  if (cvc != cvcBaseNotProperty_) {
    [cvcBaseNotProperty_ release];
    cvcBaseNotProperty_ = [cvc retain];
  }
}

@end  // @implementation ObjCPropertyTestBase

@protocol ObjCPropertyTestProtocol

@property(retain, nonatomic) CountVonCount* cvcProtoRetain;
@property(copy, nonatomic) CountVonCount* cvcProtoCopy;
@property(assign, nonatomic) CountVonCount* cvcProtoAssign;

@end  // @protocol ObjCPropertyTestProtocol

@interface ObjCPropertyTestDerived
    : ObjCPropertyTestBase<ObjCPropertyTestProtocol> {
 @private
  CountVonCount* cvcDerivedRetain_;
  CountVonCount* cvcDerivedCopy_;
  CountVonCount* cvcDerivedAssign_;
  CountVonCount* cvcDerivedNotProperty_;
  CountVonCount* cvcProtoRetain_;
  CountVonCount* cvcProtoCopy_;
  CountVonCount* cvcProtoAssign_;
  base::mac::ObjCPropertyReleaser propertyReleaser_ObjCPropertyTestDerived_;
}

@property(retain, nonatomic) CountVonCount* cvcDerivedRetain;
@property(copy, nonatomic) CountVonCount* cvcDerivedCopy;
@property(assign, nonatomic) CountVonCount* cvcDerivedAssign;

- (void)setCvcDerivedNotProperty:(CountVonCount*)cvc;

@end  // @interface ObjCPropertyTestDerived

@implementation ObjCPropertyTestDerived

@synthesize cvcDerivedRetain = cvcDerivedRetain_;
@synthesize cvcDerivedCopy = cvcDerivedCopy_;
@synthesize cvcDerivedAssign = cvcDerivedAssign_;
@synthesize cvcProtoRetain = cvcProtoRetain_;
@synthesize cvcProtoCopy = cvcProtoCopy_;
@synthesize cvcProtoAssign = cvcProtoAssign_;

- (id)init {
  if ((self = [super init])) {
    propertyReleaser_ObjCPropertyTestDerived_.Init(
        self, [ObjCPropertyTestDerived class]);
  }
  return self;
}

- (void)dealloc {
  [cvcDerivedNotProperty_ release];
  [super dealloc];
}

- (void)setCvcDerivedNotProperty:(CountVonCount*)cvc {
  if (cvc != cvcDerivedNotProperty_) {
    [cvcDerivedNotProperty_ release];
    cvcDerivedNotProperty_ = [cvc retain];
  }
}

@end  // @implementation ObjCPropertyTestDerived

namespace {

TEST(ObjCPropertyReleaserTest, SesameStreet) {
  ObjCPropertyTestDerived* test_object = [[ObjCPropertyTestDerived alloc] init];

  // Assure a clean slate.
  EXPECT_EQ(0, ah_ah_ah);

  CountVonCount* baseAssign = [[CountVonCount alloc] init];
  CountVonCount* derivedAssign = [[CountVonCount alloc] init];
  CountVonCount* protoAssign = [[CountVonCount alloc] init];

  // Make sure that worked before things get more involved.
  EXPECT_EQ(3, ah_ah_ah);

  {
    base::mac::ScopedNSAutoreleasePool pool;

    test_object.cvcBaseRetain = [CountVonCount countVonCount];
    test_object.cvcBaseCopy = [CountVonCount countVonCount];
    test_object.cvcBaseAssign = baseAssign;
    [test_object setCvcBaseNotProperty:[CountVonCount countVonCount]];

    // That added 3 objects, plus 1 more that was copied.
    EXPECT_EQ(7, ah_ah_ah);

    test_object.cvcDerivedRetain = [CountVonCount countVonCount];
    test_object.cvcDerivedCopy = [CountVonCount countVonCount];
    test_object.cvcDerivedAssign = derivedAssign;
    [test_object setCvcDerivedNotProperty:[CountVonCount countVonCount]];

    // That added 3 objects, plus 1 more that was copied.
    EXPECT_EQ(11, ah_ah_ah);

    test_object.cvcProtoRetain = [CountVonCount countVonCount];
    test_object.cvcProtoCopy = [CountVonCount countVonCount];
    test_object.cvcProtoAssign = protoAssign;

    // That added 2 objects, plus 1 more that was copied.
    EXPECT_EQ(14, ah_ah_ah);
  }

  // Now that the autorelease pool has been popped, there should be 12
  // CountVonCounts. The ones that were copied to place into the test objects
  // will now have been deallocated.
  EXPECT_EQ(11, ah_ah_ah);

  [test_object release];

  // The property releaser should have released all of the CountVonCounts
  // associated with properties marked "retain" or "copy". The -dealloc
  // methods in each should have released the single non-property objects in
  // each. Only the CountVonCounts assigned to the properties marked "assign"
  // should remain.
  EXPECT_EQ(3, ah_ah_ah);

  [baseAssign release];
  [derivedAssign release];
  [protoAssign release];

  // Zero! Zero counts! Ah, ah, ah.
  EXPECT_EQ(0, ah_ah_ah);
}

}  // namespace
