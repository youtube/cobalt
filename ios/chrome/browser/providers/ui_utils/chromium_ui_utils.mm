// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"

#import <CoreGraphics/CoreGraphics.h>

#import "ios/chrome/browser/providers/ui_utils/chromium_logo_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

void InitializeUI() {
  // Chromium does not have global UI state.
}

UITextField* CreateStyledTextField() {
  // Chromium uses default style for UITextField.
  return [[UITextField alloc] initWithFrame:CGRectZero];
}

id<LogoVendor> CreateLogoVendor(Browser* browser, web::WebState* web_state) {
  return [[ChromiumLogoController alloc] init];
}

void HideModalViewStack() {
  // Chromium provider does not present modals.
}

void LogIfModalViewsArePresented() {
  // Chromium provider does not present modals.
}

}  // namespace provider
}  // namespace ios
