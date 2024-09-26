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
  kMaxValue = kAutofill,
};

class GURL;

// Represents a `WhatsNewEntry`.
@interface WhatsNewItem : NSObject

// What's New entry type.
@property(nonatomic, assign) WhatsNewType type;
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
// Whether the What's New entry has a pimiary action button.
@property(nonatomic, assign) BOOL hasPrimaryAction;
// Title of the pimiary action button if the What's New entry has one.
@property(nonatomic, copy) NSString* primaryActionTitle;
// What's New entry URL to learn more about the feature or chrome tip.
@property(nonatomic, assign) const GURL& learnMoreURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_DATA_SOURCE_WHATS_NEW_ITEM_H_
