// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_protection_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/passwords/password_constants.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordProtectionViewController

#pragma mark - Public

- (void)viewDidLoad {
  self.image = [UIImage imageNamed:@"legacy_password_breach_illustration"];
  self.helpButtonAvailable = NO;
  self.titleString = l10n_util::GetNSString(
      IDS_PAGE_INFO_CHANGE_PASSWORD_SAVED_PASSWORD_SUMMARY);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PAGE_INFO_CHECK_PASSWORDS_BUTTON);
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kPasswordProtectionViewAccessibilityIdentifier;
}

@end
