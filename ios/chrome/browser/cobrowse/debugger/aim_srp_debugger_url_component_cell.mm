// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_component_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AIMSRPDebuggerURLComponentCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [UIFont systemFontOfSize:14 weight:UIFontWeightMedium];
    _titleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    [_titleLabel setContentHuggingPriority:UILayoutPriorityRequired
                                   forAxis:UILayoutConstraintAxisHorizontal];
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];

    _textView = [[UITextView alloc] init];
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont systemFontOfSize:14];
    _textView.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textView.backgroundColor = [UIColor clearColor];
    _textView.scrollEnabled = NO;
    _textView.textContainerInset = UIEdgeInsetsZero;
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.textAlignment = NSTextAlignmentRight;

    UIStackView* stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _textView ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentTop;
    stackView.spacing = 16;
    [self.contentView addSubview:stackView];

    AddSameConstraintsWithInsets(stackView, self.contentView,
                                 NSDirectionalEdgeInsetsMake(10, 16, 10, 16));
    [_titleLabel.widthAnchor constraintEqualToConstant:60].active = YES;
  }
  return self;
}

@end
