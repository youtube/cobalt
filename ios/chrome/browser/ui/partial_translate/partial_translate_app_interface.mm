// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_app_interface.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"

@implementation PartialTranslateAppInterface

+ (BOOL)installedPartialTranslate {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  bool incognito = chrome_test_util::IsIncognitoMode();
  if (incognito) {
    browserState = chrome_test_util::GetCurrentIncognitoBrowserState();
  }
  PrefService* prefService = browserState->GetPrefs();
  UIViewController* viewController =
      chrome_test_util::GetActiveViewController();
  Browser* browser = chrome_test_util::GetMainBrowser();
  WebStateList* webStateList = browser ? browser->GetWebStateList() : nullptr;
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);

  PartialTranslateMediator* partialTranslateMediator =
      [[PartialTranslateMediator alloc]
            initWithWebStateList:webStateList
          withBaseViewController:viewController
                     prefService:prefService
            fullscreenController:fullscreenController
                       incognito:incognito];
  BOOL shouldInstallPartialTranslate =
      [partialTranslateMediator shouldInstallPartialTranslate];
  [partialTranslateMediator shutdown];
  return shouldInstallPartialTranslate;
}

@end
