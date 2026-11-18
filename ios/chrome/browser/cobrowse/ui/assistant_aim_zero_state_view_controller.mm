// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_zero_state_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The leading/trailing label padding.
const CGFloat kLabelHorizontalPadding = 16;

}  // namespace

@implementation AssistantAIMZeroStateViewController {
  UILabel* _greetingLabel;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor clearColor];

  _greetingLabel = [[UILabel alloc] init];
  _greetingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _greetingLabel.numberOfLines = 0;
  _greetingLabel.textAlignment = NSTextAlignmentCenter;
  _greetingLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  _greetingLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _greetingLabel.text = self.greetingMessage;

  [self.view addSubview:_greetingLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_greetingLabel.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [_greetingLabel.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_greetingLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.leadingAnchor
                                    constant:kLabelHorizontalPadding],
    [_greetingLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.trailingAnchor
                                 constant:-kLabelHorizontalPadding]
  ]];
}

- (NSString*)greetingMessage {
  return _greetingLabel.text;
}

- (void)setGreetingMessage:(NSString*)greetingMessage {
  _greetingLabel.text = greetingMessage;
}

@end
