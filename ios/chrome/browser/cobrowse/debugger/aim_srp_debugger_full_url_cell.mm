// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_full_url_cell.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Font size for the text view.
const CGFloat kFontSize = 14.0;

// Insets for the text view.
const NSDirectionalEdgeInsets kTextViewInsets = {12, 16, 12, 16};

}  // namespace

@implementation AIMSRPDebuggerFullURLCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _textView = [[UITextView alloc] init];
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont monospacedSystemFontOfSize:kFontSize
                                                 weight:UIFontWeightRegular];
    _textView.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _textView.backgroundColor = [UIColor clearColor];
    _textView.scrollEnabled = NO;
    _textView.textContainerInset = UIEdgeInsetsZero;
    _textView.textContainer.lineFragmentPadding = 0;
    [self.contentView addSubview:_textView];

    AddSameConstraintsWithInsets(_textView, self.contentView, kTextViewInsets);
  }
  return self;
}

@end
