// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

UIImage* GetSigninDefaultAvatar() {
  return ImageWithColor([UIColor lightGrayColor]);
}

}  // namespace provider
}  // namespace ios
