// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_ALL_TASKS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_ALL_TASKS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"

@class LevelUpCategory;
@class LevelUpTask;
@class LevelUpAllTasksViewController;

// Delegate protocol for All Tasks view controller actions.
@protocol LevelUpAllTasksViewControllerDelegate <LevelUpViewControllerDelegate>

// Called when the user taps an individual task row on the All Tasks screen.
- (void)levelUpAllTasksViewController:(LevelUpAllTasksViewController*)controller
                           didTapTask:(LevelUpTask*)task;

@end

// View controller displaying all level-up tasks grouped by category cards.
@interface LevelUpAllTasksViewController : UIViewController <LevelUpConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<LevelUpAllTasksViewControllerDelegate> delegate;

// Adds a category card.
- (void)addCategoryCard:(LevelUpCategory*)category;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_ALL_TASKS_VIEW_CONTROLLER_H_
