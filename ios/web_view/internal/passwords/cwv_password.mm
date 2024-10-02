// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/passwords/cwv_password_internal.h"

#import <objc/runtime.h>

#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#import "ios/web_view/internal/utils/nsobject_description_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVPassword {
  password_manager::PasswordForm _passwordForm;
}

- (instancetype)initWithPasswordForm:
    (const password_manager::PasswordForm&)passwordForm {
  self = [super init];
  if (self) {
    _passwordForm = passwordForm;
    auto name_and_link =
        password_manager::GetShownOriginAndLinkUrl(_passwordForm);
    _title = base::SysUTF8ToNSString(name_and_link.first);
    _site = base::SysUTF8ToNSString(name_and_link.second.spec());
  }
  return self;
}

#pragma mark - Public

- (BOOL)isBlocked {
  return _passwordForm.blocked_by_user;
}

- (NSString*)username {
  if (self.blocked) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.username_value);
}

- (NSString*)password {
  if (self.blocked) {
    return nil;
  }
  return base::SysUTF16ToNSString(_passwordForm.password_value);
}

- (NSString*)keychainIdentifier {
  if (self.blocked) {
    return nil;
  }
  // On iOS, the LoginDatabase uses Keychain API to store passwords. The
  // "encrypted" version of the password is a unique ID (UUID) that is
  // stored as an attribute along with the password in the keychain.
  // See login_database_ios.cc for more info.
  return base::SysUTF8ToNSString(_passwordForm.encrypted_password);
}

#pragma mark - NSObject

- (NSString*)debugDescription {
  NSString* debugDescription = [super debugDescription];
  return [debugDescription
      stringByAppendingFormat:@"\n%@", CWVPropertiesDescription(self)];
}

#pragma mark - Internal

- (password_manager::PasswordForm*)internalPasswordForm {
  return &_passwordForm;
}

@end
