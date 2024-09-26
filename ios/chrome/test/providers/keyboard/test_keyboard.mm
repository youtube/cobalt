// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/keyboard/keyboard_api.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

UIWindow* GetKeyboardWindow() {
  UIWindow* last_window = nil;
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* window_scene =
        base::mac::ObjCCastStrict<UIWindowScene>(scene);
    if (window_scene.windows.count) {
      last_window = [window_scene.windows lastObject];
    }
  }

  return last_window;
}

}  // namespace provider
}  // namespace ios
