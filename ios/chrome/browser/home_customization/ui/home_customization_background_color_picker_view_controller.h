// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_consumer.h"

@protocol HomeCustomizationBackgroundColorPickerMutator;

// View controller responsible for displaying and managing the background color
// picker in the Home customization flow. Implements collection view delegate
// and data source to handle color options.
@interface HomeCustomizationBackgroundColorPickerViewController
    : UIViewController <HomeCustomizationBackgroundColorPickerConsumer,
                        UICollectionViewDataSource>

// Mutator for communicating with the
// `HomeCustomizationBackgroundColorPickerMediator`.
@property(nonatomic, weak) id<HomeCustomizationBackgroundColorPickerMutator>
    mutator;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_COLOR_PICKER_VIEW_CONTROLLER_H_
