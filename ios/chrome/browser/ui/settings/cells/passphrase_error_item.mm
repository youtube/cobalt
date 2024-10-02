// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"

#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PassphraseErrorItem

@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PassphraseErrorCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.textLabel.text = self.text;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
}

@end

@interface PassphraseErrorCell ()
@property(nonatomic, readonly, strong) UIImageView* errorImageView;
@end

@implementation PassphraseErrorCell

@synthesize textLabel = _textLabel;
@synthesize errorImageView = _errorImageView;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    UIView* contentView = self.contentView;

    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.textColor = UIColor.redColor;
    [contentView addSubview:_textLabel];

    _errorImageView = [[UIImageView alloc] init];
    _errorImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _errorImageView.image = [UIImage imageNamed:@"encryption_error"];
    [contentView addSubview:_errorImageView];

    // Set up the constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_errorImageView.leadingAnchor
          constraintEqualToAnchor:contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:_errorImageView.trailingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing],
      [_errorImageView.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],
      [_textLabel.topAnchor
          constraintEqualToAnchor:contentView.topAnchor
                         constant:kTableViewOneLabelCellVerticalSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:contentView.bottomAnchor
                         constant:-kTableViewOneLabelCellVerticalSpacing],

    ]];

    [_errorImageView
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
}

@end
