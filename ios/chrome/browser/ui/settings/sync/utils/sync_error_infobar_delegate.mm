// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/utils/sync_error_infobar_delegate.h"

#import <UIKit/UIKit.h>

#import <utility>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_delegate.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_presenter.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/models/image_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// IconConfigs is the container for all configurations of the sync error infobar
// icon
struct IconConfigs {
  bool use_icon_background_tint;
  UIColor* background_color;
  UIColor* image_tint_color;
  UIImage* icon_image;
};

const IconConfigs& GetIconConfigs() {
  static const IconConfigs kSymbols = {
      true,
      [UIColor colorNamed:kRed500Color],
      [UIColor colorNamed:kPrimaryBackgroundColor],
      DefaultSymbolTemplateWithPointSize(kSyncErrorSymbol,
                                         kInfobarSymbolPointSize),
  };
  return kSymbols;
}

}  // namespace

// static
bool SyncErrorInfoBarDelegate::Create(infobars::InfoBarManager* infobar_manager,
                                      ChromeBrowserState* browser_state,
                                      id<SyncPresenter> presenter) {
  DCHECK(infobar_manager);
  std::unique_ptr<SyncErrorInfoBarDelegate> delegate(
      new SyncErrorInfoBarDelegate(browser_state, presenter));
  std::unique_ptr<InfoBarIOS> infobar = std::make_unique<InfoBarIOS>(
      InfobarType::kInfobarTypeSyncError, std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

SyncErrorInfoBarDelegate::SyncErrorInfoBarDelegate(
    ChromeBrowserState* browser_state,
    id<SyncPresenter> presenter)
    : browser_state_(browser_state), presenter_(presenter) {
  DCHECK(!browser_state->IsOffTheRecord());
  icon_ = gfx::Image(GetIconConfigs().icon_image);
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  DCHECK(sync_service);
  // Set all of the UI based on the sync state at the same time to ensure
  // they all correspond to the same sync error.
  error_state_ = sync_service->GetUserActionableError();
  title_ = GetSyncErrorInfoBarTitleForBrowserState(browser_state_);
  message_ = base::SysNSStringToUTF16(
      GetSyncErrorMessageForBrowserState(browser_state_));
  button_text_ = base::SysNSStringToUTF16(
      GetSyncErrorButtonTitleForBrowserState(browser_state_));

  // Register for sync status changes.
  sync_service->AddObserver(this);
}

SyncErrorInfoBarDelegate::~SyncErrorInfoBarDelegate() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  sync_service->RemoveObserver(this);
}

infobars::InfoBarDelegate::InfoBarIdentifier
SyncErrorInfoBarDelegate::GetIdentifier() const {
  return SYNC_ERROR_INFOBAR_DELEGATE_IOS;
}

std::u16string SyncErrorInfoBarDelegate::GetMessageText() const {
  return message_;
}

std::u16string SyncErrorInfoBarDelegate::GetTitleText() const {
  return title_;
}

int SyncErrorInfoBarDelegate::GetButtons() const {
  return button_text_.empty() ? BUTTON_NONE : BUTTON_OK;
}

std::u16string SyncErrorInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK(button == BUTTON_OK);
  return button_text_;
}

ui::ImageModel SyncErrorInfoBarDelegate::GetIcon() const {
  return ui::ImageModel::FromImage(icon_);
}

bool SyncErrorInfoBarDelegate::UseIconBackgroundTint() const {
  return GetIconConfigs().use_icon_background_tint;
}

UIColor* SyncErrorInfoBarDelegate::GetIconImageTintColor() const {
  return GetIconConfigs().image_tint_color;
}

UIColor* SyncErrorInfoBarDelegate::GetIconBackgroundColor() const {
  return GetIconConfigs().background_color;
}

bool SyncErrorInfoBarDelegate::Accept() {
  switch (error_state_) {
    case syncer::SyncService::UserActionableError::kSignInNeedsUpdate:
      [presenter_ showReauthenticateSignin];
      break;

    case syncer::SyncService::UserActionableError::kGenericUnrecoverableError:
    case syncer::SyncService::UserActionableError::kNone:
      DCHECK(ShouldShowSyncSettings(error_state_));
      [presenter_ showAccountSettings];
      break;

    case syncer::SyncService::UserActionableError::kNeedsPassphrase:
      [presenter_ showSyncPassphraseSettings];
      break;

    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForPasswords:
    case syncer::SyncService::UserActionableError::
        kNeedsTrustedVaultKeyForEverything:
      [presenter_
          showTrustedVaultReauthForFetchKeysWithTrigger:
              syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
      break;

    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
    case syncer::SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      [presenter_
          showTrustedVaultReauthForDegradedRecoverabilityWithTrigger:
              syncer::TrustedVaultUserActionTriggerForUMA::kNewTabPageInfobar];
      break;
  }

  return false;
}

void SyncErrorInfoBarDelegate::OnStateChanged(syncer::SyncService* sync) {
  // If the inforbar is in the process of being removed, nothing must be done.
  infobars::InfoBar* infobar = this->infobar();
  if (!infobar)
    return;
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForBrowserState(browser_state_);
  syncer::SyncService::UserActionableError new_error_state =
      sync_service->GetUserActionableError();
  if (error_state_ == new_error_state)
    return;
  error_state_ = new_error_state;
  if (new_error_state == syncer::SyncService::UserActionableError::kNone) {
    infobar->RemoveSelf();
  } else {
    infobars::InfoBarManager* infobar_manager = infobar->owner();
    if (infobar_manager) {
      std::unique_ptr<ConfirmInfoBarDelegate> new_infobar_delegate(
          new SyncErrorInfoBarDelegate(browser_state_, presenter_));
      infobar_manager->ReplaceInfoBar(
          infobar, CreateConfirmInfoBar(std::move(new_infobar_delegate)));
    }
  }
}
