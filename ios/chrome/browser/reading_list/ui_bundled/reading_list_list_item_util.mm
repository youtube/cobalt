// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item_util.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_list_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

NSString* GetReadingListCellAccessibilityLabel(
    NSString* title,
    NSString* subtitle,
    BOOL showCloudSlashIcon) {
  int base_string_id =
      showCloudSlashIcon
          ? IDS_IOS_READING_LIST_ENTRY_WITH_UPLOAD_STATE_ACCESSIBILITY_LABEL
          : IDS_IOS_READING_LIST_ENTRY_ACCESSIBILITY_LABEL;

  return l10n_util::GetNSStringF(
      base_string_id, base::SysNSStringToUTF16(title), std::u16string(),
      base::SysNSStringToUTF16(subtitle));
}

BOOL AreReadingListListItemsEqual(id<ReadingListListItem> first,
                                  id<ReadingListListItem> second) {
  if (first == second) {
    return YES;
  }
  if (!first || !second || ![second isKindOfClass:[first class]]) {
    return NO;
  }
  return [first.title isEqualToString:second.title] &&
         first.entryURL.GetHost() == second.entryURL.GetHost() &&
         first.showCloudSlashIcon == second.showCloudSlashIcon;
}
