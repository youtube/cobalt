// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_param_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AIMSRPDebuggerURLParamCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _keyField = [[UITextField alloc] init];
    _keyField.translatesAutoresizingMaskIntoConstraints = NO;
    _keyField.font = [UIFont systemFontOfSize:14];
    _keyField.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _keyField.placeholder = @"name";
    _keyField.backgroundColor = [UIColor clearColor];

    _valueField = [[UITextView alloc] init];
    _valueField.translatesAutoresizingMaskIntoConstraints = NO;
    _valueField.font = [UIFont systemFontOfSize:14];
    _valueField.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _valueField.backgroundColor = [UIColor clearColor];
    _valueField.scrollEnabled = NO;
    _valueField.textContainerInset = UIEdgeInsetsZero;
    _valueField.textContainer.lineFragmentPadding = 0;
    _valueField.textAlignment = NSTextAlignmentRight;

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _keyField, _valueField ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentTop;
    stackView.spacing = 8;
    [self.contentView addSubview:stackView];

    AddSameConstraintsWithInsets(stackView, self.contentView,
                                 NSDirectionalEdgeInsetsMake(10, 16, 10, 16));
    [_keyField.widthAnchor constraintEqualToAnchor:stackView.widthAnchor
                                        multiplier:0.3]
        .active = YES;
  }
  return self;
}

@end
