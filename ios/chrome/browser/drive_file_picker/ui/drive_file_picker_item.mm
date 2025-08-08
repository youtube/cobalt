// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the icons.
const CGFloat kIconPointSize = 18;

// For some reason, some icon appear smaller. Use a bigger size.
const CGFloat kBiggerIconPointSize = 22;

}  // namespace

@implementation DriveFilePickerItem

- (instancetype)initWithIdentifier:(NSString*)identifier
                             title:(NSString*)title
                          subtitle:(NSString*)subtitle
                              icon:(UIImage*)icon
                              type:(DriveItemType)type {
  self = [super init];
  if (self) {
    _identifier = [identifier copy];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _icon = icon;
    _type = type;
    _enabled = YES;
    _shouldFetchIcon = NO;
    _titleRangeToEmphasize.location = NSNotFound;
  }
  return self;
}

+ (instancetype)myDriveItem {
  return [[DriveFilePickerItem alloc]
      initWithIdentifier:kDriveFilePickerMyDriveItemIdentifier
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_MY_DRIVE)
                subtitle:nil
                    icon:CustomSymbolWithPointSize(kMyDriveSymbol,
                                                   kIconPointSize)
                    type:DriveItemType::kMyDrive];
}

+ (instancetype)sharedDrivesItem {
  return [[DriveFilePickerItem alloc]
      initWithIdentifier:kDriveFilePickerSharedDrivesItemIdentifier
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_SHARED_DRIVES)
                subtitle:nil
                    icon:CustomSymbolWithPointSize(kSharedDrivesSymbol,
                                                   kIconPointSize)
                    type:DriveItemType::kSharedDrives];
}

+ (instancetype)starredItem {
  return [[DriveFilePickerItem alloc]
      initWithIdentifier:kDriveFilePickerStarredItemIdentifier
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_STARRED)
                subtitle:nil
                    icon:DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                    kIconPointSize)
                    type:DriveItemType::kStarred];
}

+ (instancetype)recentItem {
  return [[DriveFilePickerItem alloc]
      initWithIdentifier:kDriveFilePickerRecentItemIdentifier
                   title:l10n_util::GetNSString(
                             IDS_IOS_DRIVE_FILE_PICKER_RECENT)
                subtitle:nil
                    icon:DefaultSymbolWithPointSize(kClockSymbol,
                                                    kIconPointSize)
                    type:DriveItemType::kRecent];
}

+ (instancetype)sharedWithMeItem {
    UIImageConfiguration* drive_symbol_configuration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kBiggerIconPointSize
                                weight:UIImageSymbolWeightSemibold
                                 scale:UIImageSymbolScaleMedium];
    UIImage* drive_symbol = DefaultSymbolWithConfiguration(
        kPersonTwoSymbol, drive_symbol_configuration);

    return [[DriveFilePickerItem alloc]
        initWithIdentifier:kDriveFilePickerSharedWithMeItemIdentifier
                     title:l10n_util::GetNSString(
                               IDS_IOS_DRIVE_FILE_PICKER_SHARED_WITH_ME)
                  subtitle:nil
                      icon:drive_symbol
                      type:DriveItemType::kSharedWithMe];
}

@end
