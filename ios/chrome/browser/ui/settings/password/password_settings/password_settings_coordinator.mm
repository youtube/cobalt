// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"

#import <UIKit/UIKit.h>

#import "components/google/core/common/google_util.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_export_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Methods to update state in response to actions taken in the Export
// ActivityViewController.
@protocol ExportActivityViewControllerDelegate <NSObject>

// Used to reset the export state when the activity view disappears.
- (void)resetExport;

@end

// Convenience wrapper around ActivityViewController for presenting share sheet
// at the end of the export flow. We do not use completionWithItemsHandler
// because it fails sometimes; see crbug.com/820053.
@interface ExportActivityViewController : UIActivityViewController

- (instancetype)initWithActivityItems:(NSArray*)activityItems
                             delegate:(id<ExportActivityViewControllerDelegate>)
                                          delegate;

@end

@implementation ExportActivityViewController {
  __weak id<ExportActivityViewControllerDelegate> _weakDelegate;
}

- (instancetype)initWithActivityItems:(NSArray*)activityItems
                             delegate:(id<ExportActivityViewControllerDelegate>)
                                          delegate {
  self = [super initWithActivityItems:activityItems applicationActivities:nil];
  if (self) {
    _weakDelegate = delegate;
  }

  return self;
}

- (void)viewDidDisappear:(BOOL)animated {
  [_weakDelegate resetExport];
  [super viewDidDisappear:animated];
}

@end

@interface PasswordSettingsCoordinator () <
    ExportActivityViewControllerDelegate,
    PasswordExportHandler,
    PasswordSettingsPresentationDelegate,
    PasswordsInOtherAppsCoordinatorDelegate,
    PopoverLabelViewControllerDelegate,
    SettingsNavigationControllerDelegate> {
  // Service which gives us a view on users' saved passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;

  // Alert informing the user that passwords are being prepared for
  // export.
  UIAlertController* _preparingPasswordsAlert;
}

// Main view controller for this coordinator.
@property(nonatomic, strong)
    PasswordSettingsViewController* passwordSettingsViewController;

// The presented SettingsNavigationController containing
// `passwordSettingsViewController`.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The coupled mediator.
@property(nonatomic, strong) PasswordSettingsMediator* mediator;

// Command dispatcher.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// Module handling reauthentication before accessing sensitive data.
@property(nonatomic, strong) ReauthenticationModule* reauthModule;

// Coordinator for the "Passwords in Other Apps" screen.
@property(nonatomic, strong)
    PasswordsInOtherAppsCoordinator* passwordsInOtherAppsCoordinator;

@end

@implementation PasswordSettingsCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  self.reauthModule =
      ScopedPasswordSettingsReauthModuleOverride::instance
          ? ScopedPasswordSettingsReauthModuleOverride::instance->module
          : [[ReauthenticationModule alloc] init];

  _savedPasswordsPresenter =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          IOSChromeAffiliationServiceFactory::GetForBrowserState(browserState),
          IOSChromePasswordStoreFactory::GetForBrowserState(
              browserState, ServiceAccessType::EXPLICIT_ACCESS),
          IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
              browserState, ServiceAccessType::EXPLICIT_ACCESS));

  self.mediator = [[PasswordSettingsMediator alloc]
      initWithReauthenticationModule:self.reauthModule
             savedPasswordsPresenter:_savedPasswordsPresenter.get()
                       exportHandler:self
                         prefService:browserState->GetPrefs()
                     identityManager:IdentityManagerFactory::GetForBrowserState(
                                         browserState)
                         syncService:SyncServiceFactory::GetForBrowserState(
                                         browserState)];

  self.dispatcher = static_cast<id<ApplicationCommands>>(
      self.browser->GetCommandDispatcher());

  self.passwordSettingsViewController =
      [[PasswordSettingsViewController alloc] init];

  self.passwordSettingsViewController.presentationDelegate = self;

  self.settingsNavigationController = [[SettingsNavigationController alloc]
      initWithRootViewController:self.passwordSettingsViewController
                         browser:self.browser
                        delegate:self];

  self.mediator.consumer = self.passwordSettingsViewController;
  self.passwordSettingsViewController.delegate = self.mediator;

  [self.baseViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  // If the parent coordinator is stopping `self` while the UI is still being
  // presented, dismiss without animation. Dismissals due to user actions(e.g,
  // swipe or tap on Done) are animated.
  if (self.baseViewController.presentedViewController ==
      self.settingsNavigationController) {
    [self.baseViewController dismissViewControllerAnimated:NO completion:nil];
  }

  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;

  self.passwordSettingsViewController = nil;
  self.settingsNavigationController = nil;
  _preparingPasswordsAlert = nil;

  [self.mediator disconnect];
}

#pragma mark - PasswordSettingsPresentationDelegate

- (void)startExportFlow {
  UIAlertController* exportConfirmation = [UIAlertController
      alertControllerWithTitle:nil
                       message:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleActionSheet];
  exportConfirmation.view.accessibilityIdentifier =
      kPasswordSettingsExportConfirmViewId;

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action){
                             }];
  [exportConfirmation addAction:cancelAction];

  __weak PasswordSettingsCoordinator* weakSelf = self;
  UIAlertAction* exportAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                PasswordSettingsCoordinator* strongSelf = weakSelf;
                if (!strongSelf) {
                  return;
                }
                [strongSelf.mediator userDidStartExportFlow];
              }];

  [exportConfirmation addAction:exportAction];

  exportConfirmation.popoverPresentationController.sourceView =
      [self.passwordSettingsViewController sourceViewForPasswordExportAlerts];
  exportConfirmation.popoverPresentationController.sourceRect =
      [self.passwordSettingsViewController sourceRectForPasswordExportAlerts];

  [self.passwordSettingsViewController presentViewController:exportConfirmation
                                                    animated:YES
                                                  completion:nil];
}

- (void)showManagedPrefInfoForSourceView:(UIButton*)sourceView {
  // EnterpriseInfoPopoverViewController automatically handles reenabling the
  // `sourceView`, so we don't need to add any dismiss handlers or delegation,
  // just present the bubble.
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = sourceView;
  bubbleViewController.popoverPresentationController.sourceRect =
      sourceView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self.passwordSettingsViewController
      presentViewController:bubbleViewController
                   animated:YES
                 completion:nil];
}

- (void)showPasswordsInOtherAppsScreen {
  DCHECK(!self.passwordsInOtherAppsCoordinator);
  self.passwordsInOtherAppsCoordinator =
      [[PasswordsInOtherAppsCoordinator alloc]
          initWithBaseNavigationController:self.settingsNavigationController
                                   browser:self.browser];
  self.passwordsInOtherAppsCoordinator.delegate = self;
  [self.passwordsInOtherAppsCoordinator start];
}

- (void)showOnDeviceEncryptionSetUp {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kOnDeviceEncryptionOptInURL),
      GetApplicationContext()->GetApplicationLocale());
  BlockToOpenURL(self.passwordSettingsViewController, self.dispatcher)(url);
}

- (void)showOnDeviceEncryptionHelp {
  GURL url = GURL(kOnDeviceEncryptionLearnMoreURL);
  BlockToOpenURL(self.passwordSettingsViewController, self.dispatcher)(url);
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self.dispatcher
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:net::GURLWithNSURL(URL)
                                       inIncognito:NO]];
}

#pragma mark - PasswordExportHandler

- (void)showActivityViewWithActivityItems:(NSArray*)activityItems
                        completionHandler:(void (^)(NSString* activityType,
                                                    BOOL completed,
                                                    NSArray* returnedItems,
                                                    NSError* activityError))
                                              completionHandler {
  ExportActivityViewController* activityViewController =
      [[ExportActivityViewController alloc] initWithActivityItems:activityItems
                                                         delegate:self];
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeAirDrop,
    UIActivityTypeCopyToPasteboard, UIActivityTypeOpenInIBooks,
    UIActivityTypePostToFacebook, UIActivityTypePostToFlickr,
    UIActivityTypePostToTencentWeibo, UIActivityTypePostToTwitter,
    UIActivityTypePostToVimeo, UIActivityTypePostToWeibo, UIActivityTypePrint
  ];
  [activityViewController setExcludedActivityTypes:excludedActivityTypes];

  [activityViewController setCompletionWithItemsHandler:completionHandler];

  UIView* sourceView =
      [self.passwordSettingsViewController sourceViewForPasswordExportAlerts];
  CGRect sourceRect =
      [self.passwordSettingsViewController sourceRectForPasswordExportAlerts];

  activityViewController.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController.popoverPresentationController.sourceView = sourceView;
  activityViewController.popoverPresentationController.sourceRect = sourceRect;
  activityViewController.popoverPresentationController
      .permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIPopoverArrowDirectionDown;

  [self presentViewControllerForExportFlow:activityViewController];
}

- (void)showExportErrorAlertWithLocalizedReason:(NSString*)localizedReason {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_EXPORT_PASSWORDS_FAILED_ALERT_TITLE)
                       message:localizedReason
                preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  [self presentViewControllerForExportFlow:alertController];
}

- (void)showPreparingPasswordsAlert {
  _preparingPasswordsAlert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_PREPARING_ALERT_TITLE)
                       message:nil
                preferredStyle:UIAlertControllerStyleAlert];
  __weak PasswordSettingsCoordinator* weakSelf = self;
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_EXPORT_PASSWORDS_CANCEL_BUTTON)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               [weakSelf.mediator userDidCancelExportFlow];
                             }];
  [_preparingPasswordsAlert addAction:cancelAction];
  [self.passwordSettingsViewController
      presentViewController:_preparingPasswordsAlert
                   animated:YES
                 completion:nil];
}

- (void)showSetPasscodeDialog {
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_EXPORT_PASSWORDS_SET_UP_SCREENLOCK_CONTENT)
                preferredStyle:UIAlertControllerStyleAlert];

  void (^blockOpenURL)(const GURL&) =
      BlockToOpenURL(self.passwordSettingsViewController, self.dispatcher);
  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                blockOpenURL(GURL(kPasscodeArticleURL));
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;
  [self.passwordSettingsViewController presentViewController:alertController
                                                    animated:YES
                                                  completion:nil];
}

#pragma mark - ExportActivityViewControllerDelegate

- (void)resetExport {
  [self.mediator userDidCompleteExportFlow];
}

#pragma mark - PasswordsInOtherAppsCoordinatorDelegate

- (void)passwordsInOtherAppsCoordinatorDidRemove:
    (PasswordsInOtherAppsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordsInOtherAppsCoordinator, coordinator);
  [self.passwordsInOtherAppsCoordinator stop];
  self.passwordsInOtherAppsCoordinator.delegate = nil;
  self.passwordsInOtherAppsCoordinator = nil;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  // Dismiss UI and notify parent coordinator.
  auto* __weak weakSelf = self;
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                [weakSelf settingsWasDismissed];
                                              }];
}

- (void)settingsWasDismissed {
  [self.delegate passwordSettingsCoordinatorDidRemove:self];
}

- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings {
  NOTREACHED();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED();
  return nil;
}

#pragma mark - Private

// Helper method for presenting several ViewControllers used in the export flow.
// Ensures that the "Preparing passwords" alert is dismissed when something is
// ready to replace it.
- (void)presentViewControllerForExportFlow:(UIViewController*)viewController {
  if (_preparingPasswordsAlert.beingPresented) {
    __weak PasswordSettingsCoordinator* weakSelf = self;
    [_preparingPasswordsAlert
        dismissViewControllerAnimated:YES
                           completion:^{
                             [weakSelf.passwordSettingsViewController
                                 presentViewController:viewController
                                              animated:YES
                                            completion:nil];
                           }];
  } else {
    [self.passwordSettingsViewController presentViewController:viewController
                                                      animated:YES
                                                    completion:nil];
  }
}

@end
