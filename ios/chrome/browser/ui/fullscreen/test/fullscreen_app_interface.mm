// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_app_interface.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FullscreenAppInterface

+ (UIEdgeInsets)currentViewportInsets {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState)
    return UIEdgeInsetsZero;
  ChromeBrowserState* browserState =
      ChromeBrowserState::FromBrowserState(webState->GetBrowserState());
  // TODO: (crbug.com/1063516): Retrieve Browser-scoped FullscreenController
  // in a better way.
  std::set<Browser*> browsers =
      BrowserListFactory::GetForBrowserState(browserState)
          ->AllRegularBrowsers();
  // There is regular browser and inactive browser. More means multi-window.
  // NOTE: The inactive browser is always created even if the feature is
  // disabled, in order to ensure to restore all saved tabs.
  DCHECK(browsers.size() == 2);
  std::set<Browser*>::iterator iterator = base::ranges::find_if(
      browsers, [](Browser* browser) { return !browser->IsInactive(); });
  DCHECK(iterator != browsers.end());
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(*iterator);

  if (!fullscreenController)
    return UIEdgeInsetsZero;
  return fullscreenController->GetCurrentViewportInsets();
}

@end
