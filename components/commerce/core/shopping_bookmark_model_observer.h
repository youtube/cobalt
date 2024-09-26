// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_SHOPPING_BOOKMARK_MODEL_OBSERVER_H_
#define COMPONENTS_COMMERCE_CORE_SHOPPING_BOOKMARK_MODEL_OBSERVER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"

class GURL;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

class ShoppingService;
class SubscriptionsManager;

// A utility class that watches for changes in bookmark URLs. In the case that
// the bookmark was a shopping item, the meta should be removed since we can't
// guarantee the URL still points to the product.
//
// TODO(1317783): We can probably update the data rather than delete it once
//                privacy-preserving fetch from optimization guide becomes
//                available.
class ShoppingBookmarkModelObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  ShoppingBookmarkModelObserver(bookmarks::BookmarkModel* model,
                                ShoppingService* shopping_service,
                                SubscriptionsManager* subscriptions_manager);
  ShoppingBookmarkModelObserver(const ShoppingBookmarkModelObserver&) = delete;
  ShoppingBookmarkModelObserver& operator=(
      const ShoppingBookmarkModelObserver&) = delete;

  ~ShoppingBookmarkModelObserver() override;

  void BookmarkModelChanged() override;

  void OnWillChangeBookmarkNode(bookmarks::BookmarkModel* model,
                                const bookmarks::BookmarkNode* node) override;

  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;

  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           size_t old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;

  void BookmarkMetaInfoChanged(bookmarks::BookmarkModel* model,
                               const bookmarks::BookmarkNode* node) override;

 private:
  base::raw_ptr<ShoppingService> shopping_service_;

  base::raw_ptr<SubscriptionsManager> subscriptions_manager_;

  // A map of bookmark ID to its current URL. This is used to detect incoming
  // changes to the URL since there isn't an explicit event for it.
  std::map<int64_t, GURL> node_to_url_map_;

  // Automatically remove this observer from its host when destroyed.
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      scoped_observation_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_SHOPPING_BOOKMARK_MODEL_OBSERVER_H_
