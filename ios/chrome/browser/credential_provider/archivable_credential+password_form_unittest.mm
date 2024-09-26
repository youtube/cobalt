// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/archivable_credential+password_form.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using password_manager::PasswordForm;
using ArchivableCredentialPasswordFormTest = PlatformTest;

// Tests the creation of a credential from a password form.
TEST_F(ArchivableCredentialPasswordFormTest, Creation) {
  NSString* username = @"username_value";
  NSString* favicon = @"favicon_value";
  NSString* keychainIdentifier = @"keychain_identifier_value";
  NSString* validationIdentifier = @"validation_identifier_value";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";

  PasswordForm passwordForm;
  passwordForm.times_used_in_html_form = 10;
  passwordForm.username_element = u"username_element";
  passwordForm.password_element = u"password_element";
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.encrypted_password = base::SysNSStringToUTF8(keychainIdentifier);
  passwordForm.url = GURL(base::SysNSStringToUTF16(url));
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:passwordForm
                                                 favicon:favicon
                                    validationIdentifier:validationIdentifier];

  EXPECT_TRUE(credential);
  EXPECT_EQ(passwordForm.times_used_in_html_form, credential.rank);
  EXPECT_NSEQ(username, credential.user);
  EXPECT_NSEQ(favicon, credential.favicon);
  EXPECT_NSEQ(validationIdentifier, credential.validationIdentifier);
  EXPECT_NSEQ(keychainIdentifier, credential.keychainIdentifier);
  EXPECT_NSEQ(@"alpha.example.com", credential.serviceName);
  EXPECT_NSEQ(@"http://www.alpha.example.com/path/and?args=8|"
              @"username_element|username_value|password_element|",
              credential.recordIdentifier);
}

// Tests the creation of a credential from a password form.
TEST_F(ArchivableCredentialPasswordFormTest, AndroidCredentialCreation) {
  PasswordForm form;
  form.signon_realm = "android://hash@com.example.my.app";
  form.password_element = u"pwd";
  form.password_value = u"example";

  ArchivableCredential* credentialOnlyRealm =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];

  EXPECT_TRUE(credentialOnlyRealm);
  EXPECT_NSEQ(@"android://hash@com.example.my.app",
              credentialOnlyRealm.serviceName);
  EXPECT_NSEQ(@"android://hash@com.example.my.app",
              credentialOnlyRealm.serviceIdentifier);

  form.app_display_name = "my.app";

  ArchivableCredential* credentialRealmAndAppName =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];

  EXPECT_NSEQ(@"my.app", credentialRealmAndAppName.serviceName);
  EXPECT_NSEQ(@"android://hash@com.example.my.app",
              credentialRealmAndAppName.serviceIdentifier);

  form.affiliated_web_realm = "https://m.app.example.com";

  ArchivableCredential* credentialAffiliatedRealm =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];

  EXPECT_NSEQ(@"app.example.com", credentialAffiliatedRealm.serviceName);
  EXPECT_NSEQ(@"https://m.app.example.com",
              credentialAffiliatedRealm.serviceIdentifier);
}

// Tests the creation of blocked forms is not possible.
TEST_F(ArchivableCredentialPasswordFormTest, BlockedCreation) {
  PasswordForm form;
  form.signon_realm = "android://hash@com.example.my.app";
  form.password_element = u"pwd";
  form.password_value = u"example";
  form.blocked_by_user = true;

  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:form
                                                 favicon:nil
                                    validationIdentifier:nil];

  EXPECT_FALSE(credential);
}

// Tests the creation of a PasswordForm from a Credential.
TEST_F(ArchivableCredentialPasswordFormTest, PasswordFormFromCredential) {
  NSString* username = @"username_value";
  NSString* keychainIdentifier = @"keychain_identifier_value";
  NSString* url = @"http://www.alpha.example.com/path/and?args=8";
  NSString* recordIdentifier = @"recordIdentifier";

  id<Credential> credential =
      [[ArchivableCredential alloc] initWithFavicon:nil
                                 keychainIdentifier:keychainIdentifier
                                               rank:1
                                   recordIdentifier:recordIdentifier
                                  serviceIdentifier:url
                                        serviceName:nil
                                               user:username
                               validationIdentifier:nil
                                               note:nil];
  EXPECT_TRUE(credential);

  PasswordForm passwordForm = PasswordFormFromCredential(credential);
  EXPECT_EQ(passwordForm.times_used_in_html_form, credential.rank);
  EXPECT_EQ(passwordForm.username_value, base::SysNSStringToUTF16(username));
  EXPECT_EQ(passwordForm.encrypted_password,
            base::SysNSStringToUTF8(keychainIdentifier));
  EXPECT_EQ(passwordForm.url, GURL("http://www.alpha.example.com/path/and"));
  EXPECT_EQ(passwordForm.signon_realm, "http://www.alpha.example.com/");
}

// Tests the creation of a credential from a password form (that has a mobile
// prefix).
TEST_F(ArchivableCredentialPasswordFormTest, CreationWithMobileURL) {
  NSString* username = @"username_value";
  NSString* favicon = @"favicon_value";
  NSString* keychainIdentifier = @"keychain_identifier_value";
  NSString* validationIdentifier = @"validation_identifier_value";
  NSString* url = @"http://m.alpha.example.com/path/and?args=8";

  PasswordForm passwordForm;
  passwordForm.times_used_in_html_form = 10;
  passwordForm.username_element = u"username_element";
  passwordForm.password_element = u"password_element";
  passwordForm.username_value = base::SysNSStringToUTF16(username);
  passwordForm.encrypted_password = base::SysNSStringToUTF8(keychainIdentifier);
  passwordForm.url = GURL(base::SysNSStringToUTF16(url));
  ArchivableCredential* credential =
      [[ArchivableCredential alloc] initWithPasswordForm:passwordForm
                                                 favicon:favicon
                                    validationIdentifier:validationIdentifier];

  EXPECT_TRUE(credential);
  EXPECT_EQ(passwordForm.times_used_in_html_form, credential.rank);
  EXPECT_NSEQ(username, credential.user);
  EXPECT_NSEQ(favicon, credential.favicon);
  EXPECT_NSEQ(validationIdentifier, credential.validationIdentifier);
  EXPECT_NSEQ(keychainIdentifier, credential.keychainIdentifier);
  EXPECT_NSEQ(@"alpha.example.com", credential.serviceName);
  EXPECT_NSEQ(@"http://m.alpha.example.com/path/and?args=8|"
              @"username_element|username_value|password_element|",
              credential.recordIdentifier);
}

}  // namespace
