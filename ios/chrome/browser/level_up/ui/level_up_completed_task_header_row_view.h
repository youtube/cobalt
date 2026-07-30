// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_COMPLETED_TASK_HEADER_ROW_VIEW_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_COMPLETED_TASK_HEADER_ROW_VIEW_H_

#import "ios/chrome/browser/level_up/ui/level_up_task_row_view.h"

// A custom header row displaying the number of completed tasks.
@interface LevelUpCompletedTaskHeaderRowView : LevelUpTaskRowView

// Configures the row as a completed tasks header, displaying the count of
// completed tasks.
- (void)configureWithCompletedTasksCount:(NSInteger)count
                                expanded:(BOOL)expanded;

// Updates the expansion state.
- (void)setExpanded:(BOOL)expanded;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_UI_LEVEL_UP_COMPLETED_TASK_HEADER_ROW_VIEW_H_
