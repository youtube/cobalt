// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_manager_app_interface.h"

#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation InfobarManagerAppInterface

+ (BOOL)verifyInfobarCount:(NSInteger)totalInfobars {
  MainController* mainController = chrome_test_util::GetMainController();
  id<BrowserProvider> interface =
      mainController.browserProviderInterface.mainBrowserProvider;
  web::WebState* webState =
      interface.browser->GetWebStateList()->GetActiveWebState();
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(webState);
  return totalInfobars == (NSInteger)manager->infobars().size();
}

+ (BOOL)addTestInfoBarToCurrentTabWithMessage:(NSString*)message {
  MainController* mainController = chrome_test_util::GetMainController();
  id<BrowserProvider> interface =
      mainController.browserProviderInterface.mainBrowserProvider;
  web::WebState* webState =
      interface.browser->GetWebStateList()->GetActiveWebState();
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(webState);
  TestInfoBarDelegate* testInfobarDelegate = new TestInfoBarDelegate(message);
  return testInfobarDelegate->Create(manager);
}

@end
