// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_

#import <UIKit/UIKit.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"
#include "url/gurl.h"

#define BookmarkEarlGrey \
  [BookmarkEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// GURL for the testing bookmark "First URL".
const GURL GetFirstUrl();

// GURL for the testing bookmark "Second URL".
const GURL GetSecondUrl();

// GURL for the testing bookmark "French URL".
const GURL GetFrenchUrl();

// Test methods that perform actions on Bookmarks. These methods may read or
// alter Chrome's internal state programmatically or via the UI, but in both
// cases will properly synchronize the UI for Earl Grey tests.
@interface BookmarkEarlGreyImpl : BaseEGTestHelperImpl

#pragma mark - Setup and Teardown

// Clear Bookmarks top most row position cache.
- (void)clearBookmarksPositionCache;

// Loads a set of default bookmarks in the model for the tests to use.
// GREYAssert is induced if test bookmarks can not be loaded.
- (void)setupStandardBookmarks;

// Loads a large set of bookmarks in the model which is longer than the screen
// height. GREYAssert is induced if test bookmarks can not be loaded.
- (void)setupBookmarksWhichExceedsScreenHeight;

// Waits for the Bookmark modedl to be `loaded`. GREYAssert is induced if test
// bookmarks can not be loaded.
- (void)waitForBookmarkModelLoaded:(BOOL)loaded;

#pragma mark - Common Helpers

// Verifies that `expectedCount` bookmarks exist with the corresponding `title`
// using the BookmarkModel. GREYAssert is induced if the count doesn't match.
- (void)verifyBookmarksWithTitle:(NSString*)title
                   expectedCount:(NSUInteger)expectedCount;

// Verifies that there is `count` children on the bookmark folder with `name`.
// GREYAssert is induced if the folder doesn't exist or the count doesn't match.
- (void)verifyChildCount:(int)count inFolderWithName:(NSString*)name;

// Programmatically adds a bookmark with the given title and URL. GREYAssert is
// induced if the bookmark cannot be added.
- (void)addBookmarkWithTitle:(NSString*)title URL:(NSString*)url;

// Removes programmatically the first bookmark with the given title. GREYAssert
// is induced if the bookmark can't be removed.
- (void)removeBookmarkWithTitle:(NSString*)title;

// Moves bookmark with title `bookmarkTitle` into a folder with title
// `newFolder`. GREYAssert is induced if the bookmark can't be moved.
- (void)moveBookmarkWithTitle:(NSString*)bookmarkTitle
            toFolderWithTitle:(NSString*)newFolder;

// Verifies the existence of a Bookmark with `URL` and `name`. GREYAssert is
// induced if the bookmarks doesn't exist.
- (void)verifyExistenceOfBookmarkWithURL:(NSString*)URL name:(NSString*)name;

// Verifies the absence of a Bookmark with `URL`. GREYAssert is induced if the
// bookmarks does exist.
- (void)verifyAbsenceOfBookmarkWithURL:(NSString*)URL;

// Verifies that a folder called `title` exists. GREYAssert is induced if the
// folder doesn't exist.
- (void)verifyExistenceOfFolderWithTitle:(NSString*)title;

#pragma mark - Promo

// Checks that the promo has already been seen or not. GREYAssert is induced if
// the opposite is true.
- (void)verifyPromoAlreadySeen:(BOOL)seen;

// Checks that the promo has already been seen or not.
- (void)setPromoAlreadySeen:(BOOL)seen;

// Sets that the promo has already been seen `times` number of times.
- (void)setPromoAlreadySeenNumberOfTimes:(int)times;

// Returns the number of times a Promo has been seen.
- (int)numberOfTimesPromoAlreadySeen;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EARL_GREY_H_
