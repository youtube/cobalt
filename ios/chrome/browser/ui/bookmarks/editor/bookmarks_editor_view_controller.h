// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_view_controller_delegate.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"

@protocol BookmarksEditorMutator;
class Browser;
@protocol SnackbarCommands;

// View controller for editing bookmarks. Allows editing of the title, URL and
// the parent folder of the bookmark.
//
// This view controller will also monitor bookmark model change events and react
// accordingly depending on whether the bookmark and folder it is editing
// changes underneath it.
@interface BookmarksEditorViewController
    : ChromeTableViewController <BookmarksEditorConsumer, KeyCommandActions>

@property(nonatomic, weak) id<BookmarksEditorViewControllerDelegate> delegate;
// Snackbar commands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;
// Cancel button item in navigation bar.
@property(nonatomic, strong, readonly) UIBarButtonItem* cancelItem;
// Mutator for the presented bookmark.
@property(nonatomic, weak) id<BookmarksEditorMutator> mutator;
// Whether some value was edited.
@property(nonatomic, assign) BOOL edited;

// Designated initializer.
// TODO(crbug.com/1404311) Remove the model from init.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Cancels the editor.
- (void)cancel;

// Saves the current changes.
- (void)save;

// Enables or disables the navigation left and right buttons.
- (void)setNavigationItemsEnabled:(BOOL)enabled;

// Dismisses the bookmark edit view.
- (void)dismissBookmarkEditorView;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_H_
