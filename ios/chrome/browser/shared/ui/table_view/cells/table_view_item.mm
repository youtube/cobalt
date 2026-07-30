// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
@implementation TableViewItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    _useCustomSeparator = NO;
    _selectionStyle = UITableViewCellSelectionStyleDefault;

    self.cellClass = [LegacyTableViewCell class];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell {
  DCHECK([cell class] == self.cellClass);
  DCHECK([cell isKindOfClass:[LegacyTableViewCell class]]);
  cell.accessoryType = self.accessoryType;
  cell.editingAccessoryType = self.editingAccessoryType;
  cell.accessoryView = self.accessoryView;
  cell.selectionStyle = self.selectionStyle;
  cell.useCustomSeparator = self.useCustomSeparator;
  cell.accessibilityTraits = self.accessibilityTraits;
  cell.accessibilityIdentifier = self.accessibilityIdentifier;
  if (!cell.backgroundView) {
    cell.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  return nil;
}

@end
