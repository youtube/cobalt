// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_load_details.h"

#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/url_index.h"

namespace bookmarks {

BookmarkLoadDetails::BookmarkLoadDetails(BookmarkClient* client)
    : load_managed_node_callback_(client->GetLoadManagedNodeCallback()),
      titled_url_index_(std::make_unique<TitledUrlIndex>()),
      load_start_(base::TimeTicks::Now()) {
  // WARNING: do NOT add |client| as a member. Much of this code runs on another
  // thread, and |client_| is not thread safe, and/or may be destroyed before
  // this.
  root_node_ = std::make_unique<BookmarkNode>(
      /*id=*/0, base::Uuid::ParseLowercase(kRootNodeUuid), GURL());
  root_node_ptr_ = root_node_.get();
  // WARNING: order is important here, various places assume the order is
  // constant (but can vary between embedders with the initial visibility
  // of permanent nodes).
  bb_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateBookmarkBar(
          max_id_++, client->IsPermanentNodeVisibleWhenEmpty(
                         BookmarkNode::BOOKMARK_BAR))));
  other_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateOtherBookmarks(
          max_id_++,
          client->IsPermanentNodeVisibleWhenEmpty(BookmarkNode::OTHER_NODE))));
  mobile_folder_node_ = static_cast<BookmarkPermanentNode*>(
      root_node_->Add(BookmarkPermanentNode::CreateMobileBookmarks(
          max_id_++,
          client->IsPermanentNodeVisibleWhenEmpty(BookmarkNode::MOBILE))));
}

BookmarkLoadDetails::~BookmarkLoadDetails() = default;

void BookmarkLoadDetails::LoadManagedNode() {
  if (!load_managed_node_callback_) {
    return;
  }

  std::unique_ptr<BookmarkPermanentNode> managed_node =
      std::move(load_managed_node_callback_).Run(&max_id_);
  if (!managed_node) {
    return;
  }
  root_node_->Add(std::move(managed_node));
}

void BookmarkLoadDetails::CreateIndices() {
  AddNodeToIndexRecursive(root_node_.get());
  url_index_ = base::MakeRefCounted<UrlIndex>(std::move(root_node_));
}

void BookmarkLoadDetails::AddNodeToIndexRecursive(BookmarkNode* node) {
  uuid_index_.insert(node);
  if (node->is_url()) {
    if (node->url().is_valid()) {
      titled_url_index_->Add(node);
    }
  } else {
    titled_url_index_->AddPath(node);
    for (const auto& child : node->children()) {
      AddNodeToIndexRecursive(child.get());
    }
  }
}

}  // namespace bookmarks
