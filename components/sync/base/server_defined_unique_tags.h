// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SERVER_DEFINED_UNIQUE_TAGS_H_
#define COMPONENTS_SYNC_BASE_SERVER_DEFINED_UNIQUE_TAGS_H_

namespace syncer {

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags.
//
// Each tag corresponds to a singleton instance of a particular top-level node
// in a user's account; the tags are consistent across users. The tags allow us
// to locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "bookmark_bar" represents the permanent
// node for bookmarks bar in Chrome. The tag "other_bookmarks" represents the
// permanent folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// SyncService can rely on the presence of tagged nodes.
inline constexpr char kBookmarkBarTag[] = "bookmark_bar";
inline constexpr char kSyncedBookmarksTag[] = "synced_bookmarks";
inline constexpr char kOtherBookmarksTag[] = "other_bookmarks";

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SERVER_DEFINED_UNIQUE_TAGS_H_
