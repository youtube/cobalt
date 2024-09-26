// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using SearchEngineItemTest = PlatformTest;
}  // namespace

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(SearchEngineItemTest, BasicProperties) {
  NSString* text = @"Title text";
  NSString* detailText = @"www.google.com";
  GURL URL = net::GURLWithNSURL([NSURL URLWithString:detailText]);

  SearchEngineItem* item = [[SearchEngineItem alloc] initWithType:0];
  item.text = text;
  item.detailText = detailText;
  item.URL = URL;
  item.accessoryType = UITableViewCellAccessoryCheckmark;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewURLCell class]]);

  TableViewURLCell* URLCell = base::mac::ObjCCastStrict<TableViewURLCell>(cell);
  EXPECT_FALSE(URLCell.titleLabel.text);
  EXPECT_FALSE(URLCell.URLLabel.text);
  EXPECT_EQ(item.uniqueIdentifier, URLCell.cellUniqueIdentifier);
  EXPECT_EQ(UITableViewCellAccessoryNone, URLCell.accessoryType);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:URLCell withStyler:styler];
  EXPECT_NSEQ(text, URLCell.titleLabel.text);
  EXPECT_NSEQ(detailText, URLCell.URLLabel.text);
  EXPECT_EQ(item.uniqueIdentifier, URLCell.cellUniqueIdentifier);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, URLCell.accessoryType);
}
