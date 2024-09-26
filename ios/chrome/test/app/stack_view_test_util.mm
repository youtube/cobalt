// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/stack_view_test_util.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/app/main_controller_private.h"
#import "ios/chrome/browser/ui/stack_view/stack_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

bool IsTabSwitcherActive() {
  return [chrome_test_util::GetMainController() isTabSwitcherActive];
}

StackViewController* GetStackViewController() {
  if (!IsTabSwitcherActive())
    return nil;
  MainController* mainController = chrome_test_util::GetMainController();
  DCHECK(mainController);
  return base::mac::ObjCCastStrict<StackViewController>(
      mainController.tabSwitcher);
}

}  // namespace chrome_test_util
