// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <string_view>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
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
static constexpr char kBoolKey[] = "IncognitoActive";
static constexpr char kTimeKey[] = "StartSurfaceSceneEnterIntoBackgroundTime";
static constexpr char kSessionName[] = "D5A906A5-A92C-4729-86B6-DB18F51D63C8";

}  // namespace

class SceneStatePrefsTest : public PlatformTest {
 public:
  SceneStatePrefsTest() = default;

  TestProfileManagerIOS* manager() { return &manager_; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS manager_;
};

// Test that SceneStatePrefs can save/retrieve the values for key.
TEST_F(SceneStatePrefsTest, ReadWritePrefs) {
  const std::string profile_name = manager()->ReserveNewProfileName();
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithProfileManager:manager()
                                          profileName:profile_name
                                    sessionIdentifier:kSessionName
                                         sceneSession:nil];

  // Test that default values are returned if the prefs are not set.
  EXPECT_EQ([prefs boolForKey:kBoolKey], false);
  EXPECT_EQ([prefs timeForKey:kTimeKey], base::Time());

  // Test that after setting a value, it is correctly returned.
  const base::Time time = base::Time::Now();
  [prefs setBool:true forKey:kBoolKey];
  [prefs setTime:time forKey:kTimeKey];

  EXPECT_EQ([prefs boolForKey:kBoolKey], true);
  EXPECT_EQ([prefs timeForKey:kTimeKey], time);
}

// Test that SceneStatePrefs migrate the data from NSUserDefaults.
TEST_F(SceneStatePrefsTest, MigrateFromNSUserDefaults) {
  // Prepare NSUserDefaults with some saved preferences.
  const base::Time time = base::Time::Now();
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:@(kBoolKey)];
  [defaults setObject:time.ToNSDate() forKey:@(kTimeKey)];

  const std::string profile_name = manager()->ReserveNewProfileName();
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithProfileManager:manager()
                                          profileName:profile_name
                                    sessionIdentifier:kSessionName
                                         sceneSession:nil];

  // Test that the value stored in NSUserDefaults have been migrated.
  EXPECT_EQ([prefs boolForKey:kBoolKey], true);
  EXPECT_EQ([prefs timeForKey:kTimeKey], time);

  // The values should have been removed from NSUserDefaults.
  EXPECT_NSEQ([defaults objectForKey:@(kBoolKey)], nil);
  EXPECT_NSEQ([defaults objectForKey:@(kTimeKey)], nil);
}

// Test that SceneStatePrefs migrate the data from UISceneSession.
TEST_F(SceneStatePrefsTest, MigrateFromUISceneSession) {
  SceneStatePrefsWrapper* wrapper = [[SceneStatePrefsWrapper alloc] init];
  id session = CreateMockSessionWithStorage(wrapper);

  // Prepare UISceneSession with some saved preferences.
  const base::Time time = base::Time::Now();
  [session setUserInfo:@{
    @(kBoolKey) : @YES,
    @(kTimeKey) : time.ToNSDate(),
    @("UnrelatedKey") : @"SomeValue"
  }];

  const std::string profile_name = manager()->ReserveNewProfileName();
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithProfileManager:manager()
                                          profileName:profile_name
                                    sessionIdentifier:kSessionName
                                         sceneSession:session];

  // Test that the value stored in NSUserDefaults have been migrated.
  EXPECT_EQ([prefs boolForKey:kBoolKey], true);
  EXPECT_EQ([prefs timeForKey:kTimeKey], time);

  // All values should have been removed from UISceneSession.
  EXPECT_NSEQ([session userInfo], @{});
}

// Test that SceneStatePrefs migration prefers values from UISceneSession.
TEST_F(SceneStatePrefsTest, MigrationPrefersUISceneSession) {
  SceneStatePrefsWrapper* wrapper = [[SceneStatePrefsWrapper alloc] init];
  id session = CreateMockSessionWithStorage(wrapper);

  // Prepare UISceneSession and NSUserDefaults with some saved preferences.
  const base::Time time1 = base::Time::Now();
  [session setUserInfo:@{@(kTimeKey) : time1.ToNSDate()}];

  const base::Time time2 = time1 - base::Seconds(10);
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:@(kBoolKey)];
  [defaults setObject:time2.ToNSDate() forKey:@(kTimeKey)];

  const std::string profile_name = manager()->ReserveNewProfileName();
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithProfileManager:manager()
                                          profileName:profile_name
                                    sessionIdentifier:kSessionName
                                         sceneSession:session];

  // Check that the values stored in NSUserDefaults and UISceneSession
  // are distinct (otherwise it would be difficult to check the source
  // that was used for the migration).
  ASSERT_NE(time1, time2);

  // Test that the value stored in NSUserDefaults have been migrated.
  EXPECT_EQ([prefs boolForKey:kBoolKey], true);
  EXPECT_EQ([prefs timeForKey:kTimeKey], time1);

  // All values should have been removed from UISceneSession.
  EXPECT_NSEQ([session userInfo], @{});

  // The values should have been removed from NSUserDefaults.
  EXPECT_NSEQ([defaults objectForKey:@(kBoolKey)], nil);
  EXPECT_NSEQ([defaults objectForKey:@(kTimeKey)], nil);
}
