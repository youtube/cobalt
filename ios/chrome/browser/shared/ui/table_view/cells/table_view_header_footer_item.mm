// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"

#import "base/check.h"
@implementation TableViewHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  if ((self = [super initWithType:type])) {
    self.cellClass = [UITableViewHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter {
  DCHECK([headerFooter class] == self.cellClass);
  headerFooter.accessibilityTraits = self.accessibilityTraits;
  headerFooter.accessibilityIdentifier = self.accessibilityIdentifier;
}

@end
