// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_

#include <set>
#include <vector>

class ChromeBrowserState;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

// Removes all user bookmarks and clears bookmark-related pref. Requires
// bookmark model to be loaded.
// Return true if the bookmarks were successfully removed and false otherwise.
[[nodiscard]] bool RemoveAllUserBookmarksIOS(ChromeBrowserState* browser_state);

// Returns the permanent nodes whose url children are considered uncategorized
// and whose folder children should be shown in the bookmark menu.
// `model` must be loaded.
std::vector<const bookmarks::BookmarkNode*> PrimaryPermanentNodes(
    bookmarks::BookmarkModel* model);

// Returns whether `node` is a primary permanent node in the sense of
// `PrimaryPermanentNodes`.
bool IsPrimaryPermanentNode(const bookmarks::BookmarkNode* node,
                            bookmarks::BookmarkModel* model);

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_BOOKMARKS_UTILS_H_
