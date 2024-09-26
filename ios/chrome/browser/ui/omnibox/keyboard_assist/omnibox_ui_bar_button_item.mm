// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_ui_bar_button_item.h"

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxUIBarButtonItem () {
  id<OmniboxAssistiveKeyboardDelegate> _delegate;
}
@end

@implementation OmniboxUIBarButtonItem

- (instancetype)initWithTitle:(NSString*)title
                     delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate {
  self = [super initWithTitle:title
                        style:UIBarButtonItemStylePlain
                       target:self
                       action:@selector(pressed)];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)pressed {
  [_delegate keyPressed:self.title];
}

@end
