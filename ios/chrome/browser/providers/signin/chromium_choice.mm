// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"

namespace ios {
namespace provider {

ChromeCoordinator* CreateChoiceCoordinatorWithViewController(
    UIViewController* view_controller,
    Browser* browser) {
  NOTREACHED_NORETURN();
}

ChromeCoordinator* CreateChoiceCoordinatorForFREWithNavigationController(
    UINavigationController* navigation_controller,
    Browser* browser,
    id<FirstRunScreenDelegate> first_run_delegate) {
  NOTREACHED_NORETURN();
}

id<StandardPromoDisplayHandler> CreateChoiceDisplayHandler() {
  NOTREACHED_NORETURN();
}

id<SceneAgent> CreateChoiceSceneAgent(PromosManager* promosManager,
                                      ChromeBrowserState* browserState) {
  NOTREACHED_NORETURN();
}

bool IsChoiceEnabled() {
  // The feature is disabled on chromium
  return false;
}

}  // namespace provider
}  // namespace ios
