// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmarks_service_feature.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_apis/bookmarks/bookmarks_service_impl.h"

BookmarksServiceFeature::BookmarksServiceFeature(
    bookmarks::BookmarkModel* bookmark_model,
    bookmarks::ManagedBookmarkService* managed_bookmark_service)
    : bookmark_model_(bookmark_model),
      managed_bookmark_service_(managed_bookmark_service) {
  CHECK(bookmark_model_);
  observation_.Observe(bookmark_model);
  if (bookmark_model_->loaded()) {
    // The model might not be loaded at instantiation, in which case we need
    // to defer service instantiation.
    InitializeService();
  }
}

BookmarksServiceFeature::~BookmarksServiceFeature() = default;

void BookmarksServiceFeature::Accept(
    mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService> receiver) {
  if (bookmarks_service_) {
    bookmarks_service_->Accept(std::move(receiver));
  } else {
    queued_receivers_.push_back(std::move(receiver));
  }
}

void BookmarksServiceFeature::BookmarkModelChanged() {
  // BaseBookmarkModelObserver requires this override but we don't need to do
  // anything here as we only care about the initial load.
}

void BookmarksServiceFeature::BookmarkModelLoaded(bool ids_reassigned) {
  InitializeService();
}

void BookmarksServiceFeature::BookmarkModelBeingDeleted() {
  // TODO(crbug.com/501483829): if the model is gone, then the tab strip
  // service effectively has a dangling ptr. Need to understand the life
  // cycle a bit more.
}

void BookmarksServiceFeature::InitializeService() {
  // Safe for multiple calls.
  if (bookmarks_service_) {
    return;
  }
  bookmarks_service_ = std::make_unique<bookmarks_api::BookmarksServiceImpl>(
      bookmark_model_, managed_bookmark_service_);
  for (auto& receiver : queued_receivers_) {
    bookmarks_service_->Accept(std::move(receiver));
  }
  queued_receivers_.clear();
}
