// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_notification_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <utility>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
bool SigninNotificationInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    ChromeBrowserState* browser_state,
    id<ApplicationSettingsCommands> dispatcher,
    UIViewController* view_controller) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      std::make_unique<SigninNotificationInfoBarDelegate>(
          browser_state, dispatcher, view_controller));
  std::unique_ptr<infobars::InfoBar> infobar =
      CreateHighPriorityConfirmInfoBar(std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

SigninNotificationInfoBarDelegate::SigninNotificationInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<ApplicationSettingsCommands> dispatcher,
    UIViewController* view_controller)
    : dispatcher_(dispatcher), base_view_controller_(view_controller) {
  DCHECK(!browser_state->IsOffTheRecord());

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  DCHECK(auth_service);
  id<SystemIdentity> identity =
      auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);
  UIImage* image = accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Regular);
  icon_ = gfx::Image(CircularImageFromImage(image, image.size.width));

  title_ = base::SysNSStringToUTF16(l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_ACCOUNT_NOTIFICATION_TITLE_WITH_USERNAME,
      base::SysNSStringToUTF16(identity.userGivenName)));
  message_ = base::SysNSStringToUTF16(identity.userEmail);
  button_text_ =
      base::SysNSStringToUTF16(l10n_util::GetNSString(IDS_IOS_SETTINGS_TITLE));
}

SigninNotificationInfoBarDelegate::~SigninNotificationInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
SigninNotificationInfoBarDelegate::GetIdentifier() const {
  return RE_SIGN_IN_INFOBAR_DELEGATE_IOS;
}

std::u16string SigninNotificationInfoBarDelegate::GetTitleText() const {
  return title_;
}

std::u16string SigninNotificationInfoBarDelegate::GetMessageText() const {
  return message_;
}

int SigninNotificationInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string SigninNotificationInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

ui::ImageModel SigninNotificationInfoBarDelegate::GetIcon() const {
  return ui::ImageModel::FromImage(icon_);
}

bool SigninNotificationInfoBarDelegate::UseIconBackgroundTint() const {
  return false;
}

bool SigninNotificationInfoBarDelegate::Accept() {
  [dispatcher_ showAccountsSettingsFromViewController:base_view_controller_];
  base::RecordAction(base::UserMetricsAction(
      "Settings.GoogleServices.FromSigninNotificationInfobar"));
  return true;
}

bool SigninNotificationInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  // Due to the redirects that occur when navigating to a Google subdomain from
  // the Omnibox, Chrome ensures that the sign-in infobar is displayed after
  // restoring Gaia cookies regardless of the navigation type.
  return false;
}
