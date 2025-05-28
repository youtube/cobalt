// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_sender.h"

class GURL;
@class OpenNewTabCommand;
@class ShowSigninCommand;
@class UIViewController;
namespace password_manager {
enum class PasswordCheckReferrer;
enum class WarningType;
}  // namespace password_manager
namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics
namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer
namespace trusted_vault {
enum class SecurityDomainId;
}  // namespace trusted_vault

// The mode in which the TabGrid should be opened.
enum class TabGridOpeningMode {
  // Don't force any mode, use the current one.
  kDefault,
  // Force to display the incognito mode.
  kIncognito,
  // Force to display the regular mode.
  kRegular,
};

// Protocol for commands that will generally be handled by the application,
// rather than a specific tab; in practice this means the SceneController
// instance.
@protocol ApplicationCommands

// Dismisses all modal dialogs with a completion block that is called when
// modals are dismissed (animations done).
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion;

// Dismisses all modal dialogs (if any) before showing the Password Checkup page
// for `referrer`.
- (void)dismissModalsAndShowPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Opens the Password Issues list displaying compromised, weak or reused
// credentials for `warningType` and `referrer`.
- (void)
    showPasswordIssuesWithWarningType:(password_manager::WarningType)warningType
                             referrer:(password_manager::PasswordCheckReferrer)
                                          referrer;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Settings UI, presenting from `baseViewController`.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the Settings UI, presenting from `baseViewController` and with blue dot
// for default browser settings if specified.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController
              hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot;

// Shows the settings UI for price tracking notifications.
- (void)showPriceTrackingNotificationsSettings;

// Presents the Trusted Vault reauth dialog.
// `baseViewController` presents the sign-in.
// `securityDomainID` Identifies a particular security domain.
// `trigger` UI elements where the trusted vault reauth has been triggered.
// `accessPoint` Identifies where the dialog is initiated from.
- (void)
    showTrustedVaultReauthForFetchKeysFromViewController:
        (UIViewController*)baseViewController
                                        securityDomainID:
                                            (trusted_vault::SecurityDomainId)
                                                securityDomainID
                                                 trigger:
                                                     (syncer::
                                                          TrustedVaultUserActionTriggerForUMA)
                                                         trigger
                                             accessPoint:
                                                 (signin_metrics::AccessPoint)
                                                     accessPoint;

// Presents the Trusted Vault degraded recoverability (to enroll additional
// recovery factors).
// `baseViewController` presents the sign-in.
// `securityDomainID` Identifies a particular security domain.
// `trigger` UI elements where the trusted vault reauth has been triggered.
// `accessPoint` Identifies where the dialog is initiated from.
- (void)
    showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
        (UIViewController*)baseViewController
                                                     securityDomainID:
                                                         (trusted_vault::
                                                              SecurityDomainId)
                                                             securityDomainID
                                                              trigger:
                                                                  (syncer::
                                                                       TrustedVaultUserActionTriggerForUMA)
                                                                      trigger
                                                          accessPoint:
                                                              (signin_metrics::
                                                                   AccessPoint)
                                                                  accessPoint;

// Shows the Safe Browsing settings page presenting from `baseViewController`.
- (void)showSafeBrowsingSettingsFromViewController:
    (UIViewController*)baseViewController;

// Starts a voice search on the current BVC.
- (void)startVoiceSearch;

// Shows the History UI.
- (void)showHistory;

// Closes presented views and opens a URL in a new tab.
- (void)closePresentedViewsAndOpenURL:(OpenNewTabCommand*)command;

// Closes presented views.
- (void)closePresentedViews;

// Prepare to show the TabSwitcher UI.
- (void)prepareTabSwitcher;

// Shows the TabGrid, in the chosen `mode`.
- (void)displayTabGridInMode:(TabGridOpeningMode)mode;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the settings Privacy UI.
- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Report an Issue UI, presenting from `baseViewController`.
- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender;

// Shows the Report an Issue UI, presenting from `baseViewController`, using
// `specificProductData` for additional product data to be sent in the report.
- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData;

// Opens the `command` URL in a new tab.
// TODO(crbug.com/41427539): Check if it is possible to merge it with the
// URLLoader methods.
- (void)openURLInNewTab:(OpenNewTabCommand*)command;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the signin UI, presenting from `baseViewController`.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController;

// Shows the account menu. On scenes with regular width, the account menu
// appears as a popover near the `anchorView`, if provided. If
// `skipIfUINotAvailable` is true, then this command is ignored if there is
// already a UI being presented.
- (void)showAccountMenuWithAnchorView:(UIView*)anchorView
                 skipIfUINotAvailable:(BOOL)skipIfUINotAvailable
                           completion:(void (^)())completion;

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
// Shows the consistency promo UI that allows users to sign in to Chrome using
// the default accounts on the device.
// Redirects to `url` when the sign-in flow is complete.
- (void)showWebSigninPromoFromViewController:
            (UIViewController*)baseViewController
                                         URL:(const GURL&)url;

// Shows a notification with the signed-in user account.
- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController;

// Sets whether the UI is displaying incognito content.
- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible;

// Open a new window with `userActivity`
- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity;

// Closes all open modals and ensures that a non-incognito NTP tab is open. If
// incognito is forced, then it will ensure an incognito NTP tab is open.
- (void)prepareToPresentModal:(ProceduralBlock)completion;

// Opens a debug menu for AI prototyping.
- (void)openAIMenu;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_APPLICATION_COMMANDS_H_
