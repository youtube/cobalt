// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/cells/byo_textfield_item.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
  [item configureCell:cell];
  EXPECT_NSEQ(cell.contentView, [textField superview]);
}

}  // namespace
