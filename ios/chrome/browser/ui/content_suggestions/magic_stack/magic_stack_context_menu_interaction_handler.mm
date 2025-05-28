// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_context_menu_interaction_handler.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

/// `YES` if this container should show a context menu when the user performs a
/// long-press gesture.
BOOL AllowsLongPressForModuleType(ContentSuggestionsModuleType type) {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
    case ContentSuggestionsModuleType::kSafetyCheck:
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kSetUpListDocking:
    case ContentSuggestionsModuleType::kSetUpListAddressBar:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kParcelTracking:
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kSendTabPromo:
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
    case ContentSuggestionsModuleType::kMostVisited:
      return YES;
    default:
      return NO;
  }
}

/// Title string for the context menu of this container.
NSString* GetContextMenuTitleForType(ContentSuggestionsModuleType type) {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSafetyCheck:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kParcelTracking:
      return l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_TITLE);
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
    case ContentSuggestionsModuleType::kSendTabPromo:
      return @"";
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      return l10n_util::GetNSString(
          IDS_IOS_MAGIC_STACK_TIP_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kMostVisited:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_CONTEXT_MENU_DESCRIPTION);
    default:
      NOTREACHED();
  }
}

/// Descriptor string for hide action of the context menu of this container.
NSString* GetContextMenuHideDescriptionForType(
    ContentSuggestionsModuleType type) {
  switch (type) {
    case ContentSuggestionsModuleType::kTabResumption:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_RESUMPTION_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSafetyCheck:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_CONTEXT_MENU_DESCRIPTION);
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kSetUpListNotifications:
    case ContentSuggestionsModuleType::kSetUpListDocking:
    case ContentSuggestionsModuleType::kSetUpListAddressBar:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
      return l10n_util::GetNSStringF(
          IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_DESCRIPTION,
          l10n_util::GetStringUTF16(
              content_suggestions::SetUpListTitleStringID()));
    case ContentSuggestionsModuleType::kParcelTracking:
      return l10n_util::GetNSStringF(
          IDS_IOS_PARCEL_TRACKING_CONTEXT_MENU_DESCRIPTION,
          base::SysNSStringToUTF16(l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PARCEL_TRACKING_MODULE_TITLE)));
    case ContentSuggestionsModuleType::kPriceTrackingPromo:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_PRICE_TRACKING_PROMO_HIDE_CARD);
    case ContentSuggestionsModuleType::kSendTabPromo:
      return l10n_util::GetNSStringF(
          IDS_IOS_SEND_TAB_TO_SELF_HIDE_CONTEXT_MENU_DESCRIPTION,
          base::SysNSStringToUTF16(
              l10n_util::GetNSString(IDS_IOS_SEND_TAB_PROMO_TITLE)));
    case ContentSuggestionsModuleType::kTipsWithProductImage:
    case ContentSuggestionsModuleType::kTips:
      return l10n_util::GetNSStringF(
          IDS_IOS_MAGIC_STACK_TIP_CONTEXT_MENU_HIDE_CHROME_TIPS,
          base::SysNSStringToUTF16(
              l10n_util::GetNSString(IDS_IOS_MAGIC_STACK_TIP_TITLE)));
    case ContentSuggestionsModuleType::kMostVisited:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_HIDE_CARD);
    default:
      NOTREACHED();
  }
}

}  // namespace

@interface MagicStackContextMenuInteractionHandler ()

/// Type of magic stack module being handled.
@property(nonatomic, assign) ContentSuggestionsModuleType type;

/// Whether the magic stack module should be hidden when the context menu
/// finishes presentation.
@property(nonatomic, assign) BOOL shouldHide;

@end

@implementation MagicStackContextMenuInteractionHandler

- (instancetype)initWithType:(ContentSuggestionsModuleType)type {
  self = [super init];
  if (self) {
    _shouldHide = NO;
  }
  return self;
}

- (void)configureWithType:(ContentSuggestionsModuleType)type {
  self.type = type;
}

- (void)reset {
  self.shouldHide = NO;
}

/// Returns the list of actions for the long-press/context menu.
- (NSArray<UIMenuElement*>*)menuElements {
  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];

  BOOL canShowTipsNotificationsOptIn =
      IsIOSTipsNotificationsEnabled() &&
      (IsSetUpListModuleType(self.type) || IsTipsModuleType(self.type));

  BOOL canShowSafetyCheckNotificationsOptIn =
      self.type == ContentSuggestionsModuleType::kSafetyCheck &&
      IsSafetyCheckNotificationsEnabled();

  if (canShowTipsNotificationsOptIn || canShowSafetyCheckNotificationsOptIn) {
    [actions addObject:[self toggleNotificationsActionForModuleType:self.type]];
  }

  [actions addObject:[self hideAction]];

  [actions addObject:[self customizeCardAction]];

  return actions;
}

- (void)notifyContextMenuInteractionEndWithAnimator:
    (id<UIContextMenuInteractionAnimating>)animator {
  if (self.shouldHide) {
    __weak __typeof(self) weakSelf = self;
    [animator addCompletion:^{
      [weakSelf.delegate neverShowModuleType:weakSelf.type];
    }];
  }
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  if (!AllowsLongPressForModuleType(self.type)) {
    return nil;
  }
  __weak __typeof(self) weakSelf = self;
  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        return [UIMenu menuWithTitle:GetContextMenuTitleForType(weakSelf.type)
                            children:[weakSelf menuElements]];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  if (configuration) {
    [self notifyContextMenuInteractionEndWithAnimator:animator];
  }
}

#pragma mark - Helpers

/// Returns the menu action to hide this module type.
- (UIAction*)hideAction {
  __weak __typeof(self) weakSelf = self;

  UIAction* hideAction = [UIAction
      actionWithTitle:GetContextMenuHideDescriptionForType(self.type)
                image:DefaultSymbolWithPointSize(kHideActionSymbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                weakSelf.shouldHide = YES;
              }];

  hideAction.attributes = UIMenuElementAttributesDestructive;

  return hideAction;
}

/// Returns the menu action to show the card customization settings.
- (UIAction*)customizeCardAction {
  __weak __typeof(self) weakSelf = self;
  UIAction* customizeCardAction = [UIAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_MAGIC_STACK_CONTEXT_MENU_CUSTOMIZE_CARDS_TITLE)
                image:DefaultSymbolWithPointSize(kSliderHorizontalSymbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf.delegate customizeCardsWasTapped];
              }];
  return customizeCardAction;
}

/// Returns the menu action to opt-in to Tips Notifications.
- (UIAction*)toggleNotificationsActionForModuleType:
    (ContentSuggestionsModuleType)moduleType {
  const PushNotificationClientId clientId =
      [self pushNotificationClientId:moduleType];

  BOOL optedIn = [self optedInToNotificationsForClient:clientId];

  __weak __typeof(self) weakSelf = self;

  NSString* title;
  NSString* symbol;

  int featureTitle = [self pushNotificationTitleMessageId:moduleType];

  if (optedIn) {
    title = l10n_util::GetNSStringF(
        IDS_IOS_TIPS_NOTIFICATIONS_CONTEXT_MENU_ITEM_OFF,
        l10n_util::GetStringUTF16(featureTitle));
    symbol = kBellSlashSymbol;
  } else {
    title =
        l10n_util::GetNSStringF(IDS_IOS_TIPS_NOTIFICATIONS_CONTEXT_MENU_ITEM,
                                l10n_util::GetStringUTF16(featureTitle));
    symbol = kBellSymbol;
  }

  return [UIAction
      actionWithTitle:title
                image:DefaultSymbolWithPointSize(symbol, 18)
           identifier:nil
              handler:^(UIAction* action) {
                if (optedIn) {
                  [weakSelf.delegate disableNotifications:weakSelf.type
                                           viaContextMenu:YES];
                } else {
                  [weakSelf.delegate enableNotifications:weakSelf.type
                                          viaContextMenu:YES];
                }
              }];
}

/// Returns the `PushNotificationClientId` associated with the specified `type`.
/// Currently, push notifications are exclusively supported by the Set Up List
/// and Safety Check modules.
- (PushNotificationClientId)pushNotificationClientId:
    (ContentSuggestionsModuleType)type {
  /// This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) || IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return PushNotificationClientId::kSafetyCheck;
  }

  if (IsSetUpListModuleType(type) || IsTipsModuleType(type)) {
    return PushNotificationClientId::kTips;
  }

  NOTREACHED();
}

/// Retrieves the message ID for the push notification feature title associated
/// with the specified `ContentSuggestionsModuleType`. Currently, push
/// notifications are exclusively supported by the Set Up List and Safety Check
/// modules.
- (int)pushNotificationTitleMessageId:(ContentSuggestionsModuleType)type {
  /// This is only supported for Set Up List and Safety Check modules.
  CHECK(IsSetUpListModuleType(type) || IsTipsModuleType(type) ||
        type == ContentSuggestionsModuleType::kSafetyCheck);

  if (type == ContentSuggestionsModuleType::kSafetyCheck) {
    return IDS_IOS_SAFETY_CHECK_TITLE;
  }

  if (IsSetUpListModuleType(type)) {
    return content_suggestions::SetUpListTitleStringID();
  }

  if (IsTipsModuleType(type)) {
    return IDS_IOS_MAGIC_STACK_TIP_TITLE;
  }

  NOTREACHED();
}

/// Returns YES if the user has already opted-in to notifications for the
/// specified `clientId`.
- (BOOL)optedInToNotificationsForClient:(PushNotificationClientId)clientId {
  /// Currently, push notifications are exclusively supported for the Set Up
  /// List and Safety Check modules.
  CHECK(clientId == PushNotificationClientId::kTips ||
        clientId == PushNotificationClientId::kSafetyCheck);

  /// IMPORTANT: Notifications for Set Up List and Safety Check are managed
  /// through the app-wide notification settings. If a feature that utilizes
  /// per-profile notification settings is being introduced, ensure a `gaia_id`
  /// is passed to `GetMobileNotificationPermissionStatusForClient()` below.
  return push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(clientId, GaiaId());
}

@end
