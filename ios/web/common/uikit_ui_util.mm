// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/uikit_ui_util.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIWindow* GetAnyKeyWindow() {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::mac::ObjCCastStrict<UIWindowScene>(scene);
    // Find a key window if it exists.
    for (UIWindow* window in windowScene.windows) {
      if (window.isKeyWindow)
        return window;
    }
  }

  return nil;
}

UIInterfaceOrientation GetInterfaceOrientation() {
  return GetAnyKeyWindow().windowScene.interfaceOrientation;
}
