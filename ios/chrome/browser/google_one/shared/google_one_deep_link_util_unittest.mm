// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/shared/google_one_deep_link_util.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

@interface FakeGoogleOneControllerFactory
    : NSObject <GoogleOneControllerFactory>
@property(nonatomic, assign) BOOL canHandle;
@property(nonatomic, copy) NSString* email;
@end

@implementation FakeGoogleOneControllerFactory
- (id<GoogleOneController>)createControllerWithConfiguration:
    (GoogleOneConfiguration*)configuration {
  return nil;
}
- (BOOL)canHandleURL:(NSURL*)url {
  return self.canHandle;
}
- (NSString*)emailFromURL:(NSURL*)url {
  return self.email;
}
@end

class GoogleOneDeepLinkUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    factory_ = [[FakeGoogleOneControllerFactory alloc] init];
    ios::provider::SetGoogleOneControllerFactory(factory_);
  }

  void TearDown() override {
    ios::provider::SetGoogleOneControllerFactory(nil);
    PlatformTest::TearDown();
  }

  FakeGoogleOneControllerFactory* factory_;
  IOSChromeScopedTestingLocalState testing_local_state_;
};

TEST_F(GoogleOneDeepLinkUtilTest, InvalidUrl) {
  GURL out_url;
  EXPECT_FALSE(IsGoogleOneDeepLinkURL(GURL(), &out_url));
  EXPECT_FALSE(IsGoogleOneDeepLinkURL(GURL("invalid"), &out_url));
  EXPECT_FALSE(IsGoogleOneDeepLinkURL(GURL("https://example.com"), &out_url));
}

TEST_F(GoogleOneDeepLinkUtilTest, ValidUrlHandled) {
  factory_.canHandle = YES;
  GURL out_url;
  EXPECT_TRUE(IsGoogleOneDeepLinkURL(GURL("https://one.google.com/deeplink"),
                                     &out_url));
  EXPECT_EQ(GURL("https://one.google.com/deeplink"), out_url);
}

TEST_F(GoogleOneDeepLinkUtilTest, ValidUrlUnhandled) {
  factory_.canHandle = NO;
  GURL out_url;
  EXPECT_FALSE(IsGoogleOneDeepLinkURL(GURL("https://one.google.com/deeplink"),
                                      &out_url));
}

TEST_F(GoogleOneDeepLinkUtilTest, GoogleOneAccountFromURL) {
  factory_.email = @"test@example.com";
  EXPECT_NSEQ(@"test@example.com",
              GoogleOneAccountFromURL(GURL("https://one.google.com/deeplink")));
  EXPECT_NSEQ(nil, GoogleOneAccountFromURL(GURL()));
}

TEST_F(GoogleOneDeepLinkUtilTest, FindIdentityForGoogleOneAccount) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  manager->AddIdentity(identity);

  EXPECT_EQ(identity, FindIdentityForGoogleOneAccount(identity.userEmail));
  EXPECT_EQ(identity, FindIdentityForGoogleOneAccount(
                          [identity.userEmail uppercaseString]));
  EXPECT_EQ(identity, FindIdentityForGoogleOneAccount(
                          base::SysUTF8ToNSString(identity.gaiaId.ToString())));
  EXPECT_EQ(nil, FindIdentityForGoogleOneAccount(@"nonexistent@example.com"));
  EXPECT_EQ(nil, FindIdentityForGoogleOneAccount(@""));
}

TEST_F(GoogleOneDeepLinkUtilTest, FindGaiaIdForGoogleOneAccount) {
  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager* manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  manager->AddIdentity(identity);

  EXPECT_EQ(identity.gaiaId, FindGaiaIdForGoogleOneAccount(identity.userEmail));
  EXPECT_TRUE(
      FindGaiaIdForGoogleOneAccount(@"nonexistent@example.com").empty());
}
