// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GeminiContainerViewController {
  // The child view controller wrapping the actual Gemini UI provided by the
  // provider.
  UIViewController* _geminiViewController;
}

- (instancetype)initWithGeminiViewController:
    (UIViewController*)geminiViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _geminiViewController = geminiViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (_geminiViewController) {
    [self addChildViewController:_geminiViewController];
    _geminiViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_geminiViewController.view];
    AddSameConstraints(_geminiViewController.view, self.view);
    [_geminiViewController didMoveToParentViewController:self];
  }
}

@end
