// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_

#include <memory>
#include <set>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_service.h"

class GURL;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

class HistoryClientImpl : public history::HistoryClient,
                          public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit HistoryClientImpl(bookmarks::BookmarkModel* bookmark_model);

  HistoryClientImpl(const HistoryClientImpl&) = delete;
  HistoryClientImpl& operator=(const HistoryClientImpl&) = delete;

  ~HistoryClientImpl() override;

 private:
  void StopObservingBookmarkModel();

  // history::HistoryClient implementation.
  void OnHistoryServiceCreated(
      history::HistoryService* history_service) override;
  void Shutdown() override;
  history::CanAddURLCallback GetThreadSafeCanAddURLCallback() const override;
  void NotifyProfileError(sql::InitStatus init_status,
                          const std::string& diagnostics) override;
  std::unique_ptr<history::HistoryBackendClient> CreateBackendClient() override;
  void UpdateBookmarkLastUsedTime(const base::Uuid& bookmark_node_uuid,
                                  base::Time time) override;

  // bookmarks::BaseBookmarkModelObserver implementation.
  void BookmarkModelChanged() override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;

  // BookmarkModel instance providing access to bookmarks. May be null during
  // testing, and is null while shutting down.
  bookmarks::BookmarkModel* bookmark_model_;

  // Callback invoked when URLs are removed from BookmarkModel.
  base::RepeatingCallback<void(const std::set<GURL>&)> on_bookmarks_removed_;

  // Subscription for notifications of changes to favicons.
  base::CallbackListSubscription favicons_changed_subscription_;
};

#endif  // IOS_CHROME_BROWSER_HISTORY_HISTORY_CLIENT_IMPL_H_
