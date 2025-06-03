// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

class ChromeBrowserState;

namespace favicon {
class LargeIconService;
}

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
}

@class CSSearchableItem;
@class TopSitesSpotlightManager;

/// This class is intended to be used by the SpotlightManager
/// It maintains an index of bookmark items in spotlightInterface from the
/// observed bookmarkModel. The methods should be called on main thread, but
/// will internally dispatch work to a background thread
@interface BookmarksSpotlightManager : BaseSpotlightManager

- (instancetype)
        initWithLargeIconService:(favicon::LargeIconService*)largeIconService
    localOrSyncableBookmarkModel:
        (bookmarks::BookmarkModel*)localOrSyncableBookmarkModel
            accountBookmarkModel:(bookmarks::BookmarkModel*)accountBookmarkModel
              spotlightInterface:(SpotlightInterface*)spotlightInterface
           searchableItemFactory:(SearchableItemFactory*)searchableItemFactory;

/// Number of pending large icon tasks.
@property(nonatomic, assign) NSUInteger pendingLargeIconTasksCount;

+ (BookmarksSpotlightManager*)bookmarksSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState;

/// Checks the date of the latest global indexation and reindex all bookmarks if
/// needed.
- (void)reindexBookmarksIfNeeded;

/// Clears all the bookmarks in the Spotlight index then index the bookmarks in
/// the model.
- (void)clearAndReindexModel;

/// Recursively returns all the parent folders names of a node.
/// It is exposed mainly to be tested.
- (NSMutableArray*)parentFolderNamesForNode:
    (const bookmarks::BookmarkNode*)node;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_BOOKMARKS_SPOTLIGHT_MANAGER_H_
