// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_coordinator_factory.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_placeholder_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/autofill_address_profile/save_address_profile_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/parcel_tracking/parcel_tracking_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/passwords/password_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/permissions/permissions_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/save_card/save_card_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/tab_pickup/tab_pickup_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_modal/translate/translate_infobar_modal_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_coordinator.h"

@implementation OverlayRequestCoordinatorFactory {
  Browser* _browser;
  OverlayModality _modality;
}

- (instancetype)initWithBrowser:(Browser*)browser
                       modality:(OverlayModality)modality {
  if (self = [super init]) {
    _browser = browser;
    DCHECK(_browser);

    _modality = modality;
  }
  return self;
}

- (BOOL)coordinatorForRequestUsesChildViewController:(OverlayRequest*)request {
  return [[self coordinatorClassForRequest:request]
      showsOverlayUsingChildViewController];
}

- (OverlayRequestCoordinator*)
    newCoordinatorForRequest:(OverlayRequest*)request
                    delegate:(OverlayRequestCoordinatorDelegate*)delegate
          baseViewController:(UIViewController*)baseViewController {
  return [[[self coordinatorClassForRequest:request] alloc]
      initWithBaseViewController:baseViewController
                         browser:_browser
                         request:request
                        delegate:delegate];
}

#pragma mark - Helpers

// Returns the OverlayRequestCoordinator subclass responsible for showing
// `request`'s overlay UI.
// TODO(crbug.com/1447483): Clean the switch when the default flow is added.
- (Class)coordinatorClassForRequest:(OverlayRequest*)request {
  if (DefaultInfobarOverlayRequestConfig::RequestSupport()->IsRequestSupported(
          request)) {
    DefaultInfobarOverlayRequestConfig* config =
        request->GetConfig<DefaultInfobarOverlayRequestConfig>();
    if (!config->delegate()) {
      // This happens when the browser is killed.
      return nil;
    }
    return [self coordinatorClassForInfobarType:config->infobar_type()];
  }

  switch (_modality) {
    case OverlayModality::kTesting:
      // Use TestOverlayRequestCoordinatorFactory to create factories for
      // OverlayModality::kTesting.
      // TODO(crbug.com/1056837): Remove requirement once modalities are
      // converted to no longer use enums.
      NOTREACHED_NORETURN() << "Received unsupported modality.";
    case OverlayModality::kWebContentArea:
      return [AlertOverlayCoordinator class];
    case OverlayModality::kInfobarBanner:
      if ([TranslateInfobarPlaceholderOverlayCoordinator requestSupport]
              ->IsRequestSupported(request)) {
        return [TranslateInfobarPlaceholderOverlayCoordinator class];
      }
      return [InfobarBannerOverlayCoordinator class];
    case OverlayModality::kInfobarModal:
      if ([SaveAddressProfileInfobarModalOverlayCoordinator requestSupport]
              ->IsRequestSupported(request)) {
        return [SaveAddressProfileInfobarModalOverlayCoordinator class];
      }
      break;
  }
  NOTREACHED_NORETURN() << "Received unsupported request type.";
}

// Returns the coordinator class corresponding to the given `infobarType`.
- (Class)coordinatorClassForInfobarType:(InfobarType)infobarType {
  switch (_modality) {
    case OverlayModality::kTesting:
      // Use TestOverlayRequestCoordinatorFactory to create factories for
      // OverlayModality::kTesting.
      // TODO(crbug.com/1056837): Remove requirement once modalities are
      // converted to no longer use enums.
      NOTREACHED_NORETURN() << "Received unsupported modality.";
    case OverlayModality::kWebContentArea:
      NOTREACHED_NORETURN()
          << "None implemented yet. Received unsupported modality.";
    case OverlayModality::kInfobarBanner:
      return [InfobarBannerOverlayCoordinator class];
    case OverlayModality::kInfobarModal:
      switch (infobarType) {
        case InfobarType::kInfobarTypePasswordSave:
        case InfobarType::kInfobarTypePasswordUpdate:
          return [PasswordInfobarModalOverlayCoordinator class];
        case InfobarType::kInfobarTypePermissions:
          return [PermissionsInfobarModalOverlayCoordinator class];
        case InfobarType::kInfobarTypeSaveCard:
          return [SaveCardInfobarModalOverlayCoordinator class];
        case InfobarType::kInfobarTypeTranslate:
          return [TranslateInfobarModalOverlayCoordinator class];
        case InfobarType::kInfobarTypeParcelTracking:
          return [ParcelTrackingInfobarModalOverlayCoordinator class];
        case InfobarType::kInfobarTypeTabPickup:
          return [TabPickupInfobarModalOverlayCoordinator class];
        default:
          break;
      }
  }
  NOTREACHED_NORETURN() << "Received unsupported infobar type.";
}

@end
