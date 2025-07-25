// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_ITEM_H_

#import <UIKit/UIKit.h>

// An enum representing the different Features and Chrome tips added to What's
// New.
enum class WhatsNewType {
  kError = -1,
  kMinValue = 0,
  kSearchTabs = kMinValue,
  kNewOverflowMenu = 1,
  kSharedHighlighting = 2,
  kAddPasswordManually = 3,
  kUseChromeByDefault = 4,
  kPasswordsInOtherApps = 5,
  kAutofill = 6,
  kIncognitoTabsFromOtherApps = 7,
  kIncognitoLock = 8,
  kCalendarEvent = 9,
  kChromeActions = 10,
  kMiniMaps = 11,
  kMaxValue = kMiniMaps,
};

// An enum representing the different primary button actions for features in
// What's New.
enum class WhatsNewPrimaryAction {
  kError = -1,
  kMinValue = 0,
  kNoAction = kMinValue,
  kIOSSettings = 1,
  kPrivacySettings = 2,
  kChromeSettings = 3,
  kIOSSettingsPasswords = 4,
  kMaxValue = kIOSSettingsPasswords,
};

class GURL;

// Represents a `WhatsNewEntry`.
@interface WhatsNewItem : NSObject

// What's New entry type.
@property(nonatomic, assign) WhatsNewType type;
// What's New entry type.
@property(nonatomic, assign) WhatsNewPrimaryAction primaryAction;
// What's New entry title.
@property(nonatomic, copy) NSString* title;
// What's New entry subtitle.
@property(nonatomic, copy) NSString* subtitle;
// What's New entry hero banner image.
@property(nonatomic, copy) UIImage* heroBannerImage;
// What's New entry banner image.
@property(nonatomic, copy) UIImage* bannerImage;
// What's New entry icon image.
@property(nonatomic, copy) UIImage* iconImage;
// What's New entry icon background color.
@property(nonatomic, copy) UIColor* backgroundColor;
// What's New entry instruction steps.
@property(nonatomic, copy) NSArray<NSString*>* instructionSteps;
// Title of the pimiary action button if the What's New entry has one.
@property(nonatomic, copy) NSString* primaryActionTitle;
// What's New entry URL to learn more about the feature or chrome tip.
@property(nonatomic, assign) const GURL& learnMoreURL;
// What's New entry screenshot name.
@property(nonatomic, copy) NSString* screenshotName;
// What's New entry screenshot text provier for localization.
@property(nonatomic, copy) NSDictionary* screenshotTextProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_ITEM_H_
