// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <string_view>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// A wrapper around an NSDictionary<NSString*, id> that is used to mock
// the userInfo property of UISceneSession in read/write mode.
@interface SceneStatePrefsWrapper : NSObject
@property(nonatomic, strong) NSDictionary<NSString*, id>* dict;
@end

@implementation SceneStatePrefsWrapper

- (instancetype)init {
  return [self initWithDictionary:nil];
}

- (instancetype)initWithDictionary:(NSDictionary<NSString*, id>*)dictionary {
  if ((self = [super init])) {
    _dict = [dictionary copy];
  }
  return self;
}

@end

namespace {

// Returns a mock UISceneSession that uses `wrapper` to store its `userInfo`.
// The `wrapper` object is owned by the returned mock UISceneSession.
id CreateMockSessionWithStorage(SceneStatePrefsWrapper* wrapper) {
  id mock_session = OCMClassMock([UISceneSession class]);
  OCMStub([mock_session userInfo]).andDo(^(NSInvocation* invocation) {
    id returnValue = wrapper.dict;
    [invocation setReturnValue:&returnValue];
  });
  OCMStub([mock_session setUserInfo:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained id dict = nil;
        [invocation getArgument:&dict atIndex:2];
        wrapper.dict = dict;
      });
  return mock_session;
}

// Constants used for test.
static NSString* const kBoolKey = @"boolValue";
static NSString* const kTimeKey = @"timeValue";
static constexpr char kSessionName[] = "session";
static constexpr char kProfileName[] = "profile";

}  // namespace

class SceneStatePrefsTest : public PlatformTest {
 public:
  SceneStatePrefsTest() {
    // Records the keys of all values stored in NSUserDefaults.
    keys_ = [NSSet setWithArray:[[[NSUserDefaults standardUserDefaults]
                                    dictionaryRepresentation] allKeys]];
  }

  ~SceneStatePrefsTest() override {
    // Clears any keys that were added in NSUserDefaults.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    for (NSString* key in [[defaults dictionaryRepresentation] allKeys]) {
      if (![keys_ containsObject:key]) {
        [defaults removeObjectForKey:key];
      }
    }
  }

 private:
  NSSet<NSString*>* keys_;
};

// Test that SceneStatePrefs stores the values in the NSUserDefaults if the
// session object is nil.
TEST_F(SceneStatePrefsTest, StorePrefsInNSUserDefaultsIfNoSession) {
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithSessionIdentifier:kSessionName
                                             profileName:kProfileName
                                            sceneSession:nil];

  // Pre-conditions: no value set in either NSUserDefaults.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], nil);
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(std::nullopt));
  EXPECT_EQ([prefs timeForKey:kTimeKey],
            std::optional<base::Time>(std::nullopt));

  // Set bool and time preferences.
  const base::Time time = base::Time::Now();
  [prefs setBool:true forKey:kBoolKey];
  [prefs setTime:time forKey:kTimeKey];

  // Check that the prefs can be read, and that they are stored in userInfo.
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(true));
  EXPECT_EQ([prefs timeForKey:kTimeKey], std::optional<base::Time>(time));
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], @YES);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], time.ToNSDate());
}

// Test that SceneStatePrefs stores the values in the UISceneSession userInfo
// (and only there) if the session object is not nil.
TEST_F(SceneStatePrefsTest, StorePrefsInUISceneSessionIfNotNil) {
  SceneStatePrefsWrapper* wrapper = [[SceneStatePrefsWrapper alloc] init];
  id session = CreateMockSessionWithStorage(wrapper);
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithSessionIdentifier:kSessionName
                                             profileName:kProfileName
                                            sceneSession:session];

  // Pre-conditions: no value set in either NSUserDefaults or userInfo.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], nil);
  EXPECT_NSEQ([wrapper.dict objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([wrapper.dict objectForKey:kTimeKey], nil);
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(std::nullopt));
  EXPECT_EQ([prefs timeForKey:kTimeKey],
            std::optional<base::Time>(std::nullopt));

  // Set bool and time preferences.
  const base::Time time = base::Time::Now();
  [prefs setBool:true forKey:kBoolKey];
  [prefs setTime:time forKey:kTimeKey];

  // Check that the prefs can be read, and that they are stored in userInfo.
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(true));
  EXPECT_EQ([prefs timeForKey:kTimeKey], std::optional<base::Time>(time));
  EXPECT_NSEQ([wrapper.dict objectForKey:kBoolKey], @YES);
  EXPECT_NSEQ([wrapper.dict objectForKey:kTimeKey], time.ToNSDate());
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], nil);
}

// Test that SceneStatePrefs migrates the values from NSUserDefaults if the
// session object is not nil and the prefs exists in NSUserDefaults.
TEST_F(SceneStatePrefsTest, MigratePrefsFromNSUserDefaults) {
  SceneStatePrefsWrapper* wrapper = [[SceneStatePrefsWrapper alloc] init];
  id session = CreateMockSessionWithStorage(wrapper);
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithSessionIdentifier:kSessionName
                                             profileName:kProfileName
                                            sceneSession:session];

  // Pre-conditions: set value in NSUserDefaults.
  const base::Time time1 = base::Time::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:@YES forKey:kBoolKey];
  [defaults setObject:time1.ToNSDate() forKey:kTimeKey];

  // Check that the prefs can be read.
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(true));
  EXPECT_EQ([prefs timeForKey:kTimeKey], std::optional<base::Time>(time1));

  // Check that preferences have not yet been migrated.
  EXPECT_NSEQ([wrapper.dict objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([wrapper.dict objectForKey:kTimeKey], nil);
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], @YES);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], time1.ToNSDate());

  // Change the preferences values.
  const base::Time time2 = time1 + base::Minutes(1);
  [prefs setBool:false forKey:kBoolKey];
  [prefs setTime:time2 forKey:kTimeKey];

  // Check that preferences have not yet been migrated.
  EXPECT_NSEQ([wrapper.dict objectForKey:kBoolKey], @NO);
  EXPECT_NSEQ([wrapper.dict objectForKey:kTimeKey], time2.ToNSDate());
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], @YES);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], time1.ToNSDate());

  // Check that the preferences are migrated when read.
  EXPECT_EQ([prefs boolForKey:kBoolKey], std::optional<bool>(false));
  EXPECT_EQ([prefs timeForKey:kTimeKey], std::optional<base::Time>(time2));

  EXPECT_NSEQ([wrapper.dict objectForKey:kBoolKey], @NO);
  EXPECT_NSEQ([wrapper.dict objectForKey:kTimeKey], time2.ToNSDate());
  EXPECT_NSEQ([defaults objectForKey:kBoolKey], nil);
  EXPECT_NSEQ([defaults objectForKey:kTimeKey], nil);
}
