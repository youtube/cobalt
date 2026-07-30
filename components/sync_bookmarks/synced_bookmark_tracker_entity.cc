// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker_entity.h"

#include <utility>

#include "base/check.h"
#include "base/hash/hash.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace sync_bookmarks {

SyncedBookmarkTrackerEntity::SyncedBookmarkTrackerEntity(
    const bookmarks::BookmarkNode* bookmark_node,
    syncer::ProcessorEntityMetadata entity_metadata)
    : bookmark_node_(bookmark_node), metadata_(std::move(entity_metadata)) {
  if (bookmark_node_) {
    DCHECK(!metadata_.IsDeleted());
  } else {
    DCHECK(metadata_.IsDeleted());
  }
}

SyncedBookmarkTrackerEntity::~SyncedBookmarkTrackerEntity() = default;

bool SyncedBookmarkTrackerEntity::IsDeleted() const {
  return metadata_.IsDeleted();
}

bool SyncedBookmarkTrackerEntity::IsUnsynced() const {
  return metadata_.IsUnsynced();
}

bool SyncedBookmarkTrackerEntity::IsUnsyncedLocalCreation() const {
  return metadata_.IsUnsyncedLocalCreation();
}

bool SyncedBookmarkTrackerEntity::IsVersionAlreadyKnown(
    int64_t update_version) const {
  return metadata_.IsVersionAlreadyKnown(update_version);
}

bool SyncedBookmarkTrackerEntity::MatchesData(
    const syncer::EntityData& data) const {
  return metadata_.MatchesData(data);
}

bool SyncedBookmarkTrackerEntity::MatchesBaseData(
    const syncer::EntityData& data) const {
  return metadata_.MatchesBaseData(data);
}

bool SyncedBookmarkTrackerEntity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  return metadata_.MatchesSpecificsHash(specifics);
}

bool SyncedBookmarkTrackerEntity::MatchesFaviconHash(
    const std::string& favicon_png_bytes) const {
  DCHECK(!metadata().is_deleted());
  return metadata().bookmark_favicon_hash() ==
         base::PersistentHash(favicon_png_bytes);
}

syncer::ClientTagHash SyncedBookmarkTrackerEntity::GetClientTagHash() const {
  return metadata_.GetClientTagHash();
}

size_t SyncedBookmarkTrackerEntity::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = metadata_.EstimateMemoryUsage();
  memory_usage += sizeof(bookmark_node_);
  return memory_usage;
}

}  // namespace sync_bookmarks
