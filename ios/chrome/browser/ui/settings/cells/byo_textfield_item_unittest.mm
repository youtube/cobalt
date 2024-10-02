// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/byo_textfield_item.h"

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using BYOTextFieldItemTest = PlatformTest;

// Tests that the textfield is set properly after a call to `configureCell:`.
TEST_F(BYOTextFieldItemTest, ConfigureCell) {
  BYOTextFieldItem* item = [[BYOTextFieldItem alloc] initWithType:0];
  BYOTextFieldCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[BYOTextFieldCell class]]);

  UITextField* textField = [[UITextField alloc] init];
  EXPECT_NSEQ(nil, [textField superview]);
  item.textField = textField;
  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(cell.contentView, [textField superview]);
}

}  // namespace
