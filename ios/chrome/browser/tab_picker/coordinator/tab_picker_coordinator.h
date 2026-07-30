// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/tab_picker_commands.h"

@class TabPickerParams;

// The tab picker coordinator.
@interface TabPickerCoordinator : ChromeCoordinator

// Returns `YES` if the coordinator is started.
@property(nonatomic, readonly) BOOL started;

// Configuration params for the tab picker.
@property(nonatomic, strong) TabPickerParams* params;

// Called when the user confirms a new selection of tabs.
@property(nonatomic, copy) TabPickerCompletionBlock tabPickerCompletionBlock;

// Handler for tab picker commands.
@property(nonatomic, weak) id<TabPickerCommands> tabPickerHandler;

@end
#endif  // IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_COORDINATOR_H_
