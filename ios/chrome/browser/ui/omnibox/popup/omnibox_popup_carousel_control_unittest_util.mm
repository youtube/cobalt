// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_control_unittest_util.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_carousel_control.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIView* GetViewOfClassWithIdentifier(Class uiClass,
                                     NSString* accessibilityIdentifier,
                                     UIView* view) {
  for (UIView* subview in view.subviews) {
    if ([subview isKindOfClass:uiClass] &&
        subview.accessibilityIdentifier == accessibilityIdentifier) {
      return subview;
    }
  }
  return nil;
}

UILabel* GetLabelFromCarouselControl(OmniboxPopupCarouselControl* control) {
  return base::mac::ObjCCastStrict<UILabel>(GetViewOfClassWithIdentifier(
      [UILabel class], kOmniboxCarouselControlLabelAccessibilityIdentifier,
      control));
}
