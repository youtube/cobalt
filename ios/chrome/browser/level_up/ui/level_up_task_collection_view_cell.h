// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_CELL_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_CELL_H_

#import <UIKit/UIKit.h>

@class LevelUpTask;
@class LevelUpTaskCollectionViewCell;

// Delegate protocol for actions inside the individual card cell.
@protocol LevelUpTaskCollectionViewCellDelegate <NSObject>

// Called when the user taps the "See All" button on the tasks checklist card
// cell.
- (void)didTapSeeAllTasks:(UICollectionViewCell*)cell;

// Called when the user taps the completed tasks header row in the card cell.
- (void)taskCollectionViewDidTapCompletedHeader:(UICollectionViewCell*)cell;

@end

// Collection view cell representing the individual Level Up checklist tasks
// card.
@interface LevelUpTaskCollectionViewCell : UICollectionViewCell

// The card header title.
@property(nonatomic, copy) NSString* headerTitle;

// Whether to display the "See All" action button.
@property(nonatomic, assign) BOOL showsSeeAllButton;

// Delegate receiving user action callbacks.
@property(nonatomic, weak) id<LevelUpTaskCollectionViewCellDelegate> delegate;

// Populates the card cell, separating active and completed tasks.
- (void)setTasks:(NSArray<LevelUpTask*>*)activeTasks
       completedTasks:(NSArray<LevelUpTask*>*)completedTasks
    completedExpanded:(BOOL)completedExpanded;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_TASK_COLLECTION_VIEW_CELL_H_
