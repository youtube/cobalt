// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/browser_apis/bookmarks/bookmarks_api.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace bookmarks {
class BookmarkModel;
class ManagedBookmarkService;
}

namespace bookmarks_api {
class BookmarksService;
}

class BookmarksServiceFeature : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarksServiceFeature(
      bookmarks::BookmarkModel* bookmark_model,
      bookmarks::ManagedBookmarkService* managed_bookmark_service);
  ~BookmarksServiceFeature() override;

  // Accepts an incoming connection. Note that if the underlying bookmarks
  // model is not ready yet, the acceptance will be deferred.
  void Accept(
      mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService> receiver);

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

 private:
  // Initializes the service and attached any pending clients. Safe to call
  // multiple times, but the service will only be instantiated once.
  void InitializeService();

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      observation_{this};
  std::unique_ptr<bookmarks_api::BookmarksService> bookmarks_service_;
  std::vector<mojo::PendingReceiver<bookmarks_api::mojom::BookmarksService>>
      queued_receivers_;
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARKS_SERVICE_FEATURE_H_
