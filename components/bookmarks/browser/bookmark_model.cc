// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_model.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/bookmark_load_details.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/bookmark_uuids.h"
#include "components/bookmarks/browser/model_loader.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/typed_count_sorter.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/bookmarks/browser/url_index.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/favicon_base/favicon_types.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"

using base::Time;

namespace bookmarks {

namespace {

// Helper to get a mutable bookmark node.
BookmarkNode* AsMutable(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// Comparator used when sorting permanent nodes. Nodes that are initially
// visible are sorted before nodes that are initially hidden.
class VisibilityComparator {
 public:
  explicit VisibilityComparator(BookmarkClient* client) : client_(client) {}

  // Returns true if `n1` precedes `n2`.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    DCHECK(n1->is_permanent_node());
    DCHECK(n2->is_permanent_node());
    bool n1_visible = client_->IsPermanentNodeVisibleWhenEmpty(n1->type());
    bool n2_visible = client_->IsPermanentNodeVisibleWhenEmpty(n2->type());
    return n1_visible != n2_visible && n1_visible;
  }

 private:
  raw_ptr<BookmarkClient> client_;
};

// Comparator used when sorting bookmarks. Folders are sorted first, then
// bookmarks.
class SortComparator {
 public:
  explicit SortComparator(icu::Collator* collator) : collator_(collator) {}

  // Returns true if `n1` precedes `n2`.
  bool operator()(const std::unique_ptr<BookmarkNode>& n1,
                  const std::unique_ptr<BookmarkNode>& n2) {
    if (n1->type() == n2->type()) {
      // Types are the same, compare the names.
      if (!collator_) {
        return n1->GetTitle() < n2->GetTitle();
      }
      return base::i18n::CompareString16WithCollator(
                 *collator_, n1->GetTitle(), n2->GetTitle()) == UCOL_LESS;
    }
    // Types differ, sort such that folders come first.
    return n1->is_folder();
  }

 private:
  raw_ptr<icu::Collator> collator_;
};

// Builds the path of the bookmark file from the profile path.
base::FilePath GetStorageFilePath(const base::FilePath& profile_path,
                                  StorageType storage_type) {
  switch (storage_type) {
    case StorageType::kLocalOrSyncable:
      return profile_path.Append(kLocalOrSyncableBookmarksFileName);
    case StorageType::kAccount:
      return profile_path.Append(kAccountBookmarksFileName);
  }
}

// Synchronously deletes the account storage file. Should be invoked on a
// background thread.
void DeleteAccountStorageFileSynchronously(
    const base::FilePath& account_storage_file_path) {
  base::DeleteFile(account_storage_file_path);
}

}  // namespace

// BookmarkModel --------------------------------------------------------------

BookmarkModel::BookmarkModel(std::unique_ptr<BookmarkClient> client)
    : owned_root_(std::make_unique<BookmarkNode>(
          /*id=*/0,
          base::Uuid::ParseLowercase(kRootNodeUuid),
          GURL())),
      root_(owned_root_.get()),
      observers_(base::ObserverListPolicy::EXISTING_ONLY),
      client_(std::move(client)) {
  DCHECK(client_);
  client_->Init(this);
}

BookmarkModel::~BookmarkModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelBeingDeleted(this);
  }

  if (store_) {
    // The store maintains a reference back to us. We need to tell it we're gone
    // so that it doesn't try and invoke a method back on us again.
    store_->BookmarkModelDeleted();
  }

  // `TitledUrlIndex` owns  a `TypedCountSorter` that keeps a raw_ptr to the
  // client. So titled_url_index_ must be reset first.
  titled_url_index_.reset();

  // ChromeBookmarkClient indirectly observes the model. The client should thus
  // be reset before the observer list.
  client_.reset();
}

void BookmarkModel::Load(const base::FilePath& profile_path,
                         StorageType storage_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the store is non-null, it means Load was already invoked. Load should
  // only be invoked once.
  DCHECK(!store_);

  const base::FilePath file_path =
      GetStorageFilePath(profile_path, storage_type);
  store_ = std::make_unique<BookmarkStorage>(this, file_path);
  // Creating ModelLoader schedules the load on a backend task runner.
  model_loader_ = ModelLoader::Create(
      file_path, std::make_unique<BookmarkLoadDetails>(client_.get()),
      base::BindOnce(&BookmarkModel::DoneLoading, AsWeakPtr()));
}

scoped_refptr<ModelLoader> BookmarkModel::model_loader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_loader_;
}

void BookmarkModel::AddObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BookmarkModel::RemoveObserver(BookmarkModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void BookmarkModel::BeginExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (++extensive_changes_ == 1) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.ExtensiveBookmarkChangesBeginning(this);
    }
  }
}

void BookmarkModel::EndExtensiveChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --extensive_changes_;
  DCHECK_GE(extensive_changes_, 0);
  if (extensive_changes_ == 0) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.ExtensiveBookmarkChangesEnded(this);
    }
  }
}

void BookmarkModel::BeginGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesBeginning(this);
  }
}

void BookmarkModel::EndGroupedChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.GroupedBookmarkChangesEnded(this);
  }
}

void BookmarkModel::Remove(const BookmarkNode* node,
                           metrics::BookmarkEditSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(!is_root_node(node));
  const BookmarkNode* parent = node->parent();
  DCHECK(parent);
  absl::optional<size_t> index = parent->GetIndexOf(node).value();
  DCHECK(index.has_value());

  // Removing a permanent node is problematic and can cause crashes elsewhere
  // that are difficult to trace back.
  CHECK(!is_permanent_node(node)) << "for type " << node->type();

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveBookmarks(this, parent, index.value(), node);
  }

  std::set<GURL> removed_urls;
  std::unique_ptr<BookmarkNode> owned_node =
      url_index_->Remove(AsMutable(node), &removed_urls);
  RemoveNodeFromIndicesRecursive(owned_node.get());

  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeRemoved(this, parent, index.value(), node,
                                 removed_urls);
  }

  client_->OnBookmarkNodeRemovedUndoable(this, parent, index.value(),
                                         std::move(owned_node));

  metrics::RecordBookmarkRemoved(source);
}

const BookmarkNode* BookmarkModel::MoveToOtherModelWithNewNodeIdsAndUuids(
    const BookmarkNode* node,
    BookmarkModel* dest_model,
    const BookmarkNode* dest_parent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(node);
  CHECK(dest_model);
  CHECK(dest_parent);
  CHECK(this != dest_model);
  CHECK(loaded_);
  CHECK(!is_root_node(node));
  CHECK(node->HasAncestor(root_node()));
  CHECK(dest_parent->HasAncestor(dest_model->root_node()));
  const BookmarkNode* parent = node->parent();
  CHECK(parent);
  absl::optional<size_t> index = parent->GetIndexOf(node).value();
  CHECK(index.has_value());
  // Can't move permanent nodes.
  CHECK(!is_permanent_node(node)) << "for type " << node->type();

  // Group removing bookmarks from `this` and adding them to `dest_model` into
  // one undo action.
  ScopedGroupBookmarkActions undo_grouping(this);

  // There are no special notifications for moving bookmarks between models, so
  // observers of the source model get removal notifications, while observers of
  // the destination model get bookmark addition notifications.
  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveBookmarks(this, parent, index.value(), node);
  }

  std::set<GURL> removed_urls;
  std::unique_ptr<BookmarkNode> owned_node =
      url_index_->Remove(AsMutable(node), &removed_urls);
  RemoveNodeFromIndicesRecursive(owned_node.get());

  std::unique_ptr<BookmarkNode> subtree_copy =
      CloneSubtreeForOtherModelWithNewNodeIdsAndUuids(owned_node.get(),
                                                      dest_model);
  // `MoveToOtherModelWithNewNodeIdsAndUuids` can only be triggered by a user
  // action at the moment. `AddNode` will take care of scheduling a save for
  // `dest_model`.
  const BookmarkNode* added_node = dest_model->AddNode(
      AsMutable(dest_parent), dest_parent->children().size(),
      std::move(subtree_copy), /*added_by_user=*/true);

  // Current implementation requires that `BookmarkNodeAdded` is sent for all
  // descendants (see `BookmarkNodeAdded` documentation).
  // TODO(crbug.com/1440384): Revise the `BookmarkModelObserver` API.
  dest_model->NotifyNodeAddedForAllDescendants(added_node,
                                               /*added_by_user=*/true);

  // TODO(crbug.com/1441911): Make sure this flow can never cause data loss.
  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeRemoved(this, parent, index.value(), node,
                                 removed_urls);
  }

  client_->OnBookmarkNodeRemovedUndoable(this, parent, index.value(),
                                         std::move(owned_node));
  // TODO(https://crbug.com/1416567): Record metrics.
  return added_node;
}

std::unique_ptr<BookmarkNode>
BookmarkModel::CloneSubtreeForOtherModelWithNewNodeIdsAndUuids(
    const BookmarkNode* node,
    BookmarkModel* dest_model) {
  auto new_node = std::make_unique<BookmarkNode>(
      dest_model->generate_next_node_id(), base::Uuid::GenerateRandomV4(),
      node->GetTitledUrlNodeUrl());
  new_node->SetTitle(node->GetTitle());
  new_node->set_date_added(node->date_added());
  new_node->set_date_folder_modified(node->date_folder_modified());

  for (const auto& child : node->children()) {
    std::unique_ptr<BookmarkNode> child_copy =
        CloneSubtreeForOtherModelWithNewNodeIdsAndUuids(child.get(),
                                                        dest_model);
    new_node->Add(std::move(child_copy));
  }
  return new_node;
}

void BookmarkModel::RemoveAllUserBookmarks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  std::set<GURL> removed_urls;
  struct RemoveNodeData {
    raw_ptr<const BookmarkNode> parent;
    int index;
    std::unique_ptr<BookmarkNode> node;
  };
  std::vector<RemoveNodeData> removed_node_data_list;

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillRemoveAllUserBookmarks(this);
  }

  BeginExtensiveChanges();
  // Skip deleting permanent nodes. Permanent bookmark nodes are the root and
  // its immediate children. For removing all non permanent nodes just remove
  // all children of non-root permanent nodes.
  {
    for (const auto& permanent_node : root_->children()) {
      if (client_->IsNodeManaged(permanent_node.get())) {
        continue;
      }

      for (int j = static_cast<int>(permanent_node->children().size() - 1);
           j >= 0; --j) {
        std::unique_ptr<BookmarkNode> node = url_index_->Remove(
            permanent_node->children()[j].get(), &removed_urls);
        RemoveNodeFromIndicesRecursive(node.get());
        removed_node_data_list.push_back(
            {permanent_node.get(), j, std::move(node)});
      }
    }
  }
  EndExtensiveChanges();
  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkAllUserNodesRemoved(this, removed_urls);
  }

  BeginGroupedChanges();
  for (auto& removed_node_data : removed_node_data_list) {
    client_->OnBookmarkNodeRemovedUndoable(this, removed_node_data.parent,
                                           removed_node_data.index,
                                           std::move(removed_node_data.node));
  }
  EndGroupedChanges();
}

void BookmarkModel::Move(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  // TODO(crbug.com/1446243): Check that `node` belongs to this model.
  CHECK(new_parent->HasAncestor(root_node()));
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));

  SCOPED_CRASH_KEY_NUMBER("BookmarkModelMove", "newParentType",
                          new_parent->type());
  DUMP_WILL_BE_CHECK(new_parent->is_folder());

  const BookmarkNode* old_parent = node->parent();
  size_t old_index = old_parent->GetIndexOf(node).value();

  if (old_parent == new_parent &&
      (index == old_index || index == old_index + 1)) {
    // Node is already in this position, nothing to do.
    return;
  }

  SetDateFolderModified(new_parent, Time::Now());

  if (old_parent == new_parent && index > old_index) {
    index--;
  }

  BookmarkNode* mutable_old_parent = AsMutable(old_parent);
  std::unique_ptr<BookmarkNode> owned_node =
      mutable_old_parent->Remove(old_index);
  BookmarkNode* mutable_new_parent = AsMutable(new_parent);
  mutable_new_parent->Add(std::move(owned_node), index);

  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeMoved(this, old_parent, old_index, new_parent, index);
  }

  if (old_parent != new_parent) {
    // TODO(crbug.com/1491227): Remove if check once the root cause of this
    // crash is identified and addressed, and new_parent->is_folder() is
    // checked at the top of this method.
    if (new_parent->is_folder()) {
      metrics::RecordBookmarkMovedTo(GetFolderType(new_parent));
    }
  }
}

void BookmarkModel::UpdateLastUsedTime(const BookmarkNode* node,
                                       const base::Time time,
                                       bool just_opened) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);

  base::Time last_used_time = node->date_last_used();
  UpdateLastUsedTimeImpl(node, time);
  if (just_opened) {
    metrics::RecordBookmarkOpened(time, last_used_time, node->date_added(),
                                  client_->GetStorageStateForUma());
  }
}

void BookmarkModel::UpdateLastUsedTimeImpl(const BookmarkNode* node,
                                           const base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->set_date_last_used(time);

  if (store_) {
    store_->ScheduleSave();
  }
}

void BookmarkModel::ClearLastUsedTimeInRange(const base::Time delete_begin,
                                             const base::Time delete_end) {
  ClearLastUsedTimeInRangeRecursive(root_, delete_begin, delete_end);

  if (store_) {
    store_->ScheduleSave();
  }
}

void BookmarkModel::ClearLastUsedTimeInRangeRecursive(
    BookmarkNode* node,
    const base::Time delete_begin,
    const base::Time delete_end) {
  bool within_range = node->date_last_used() >= delete_begin &&
                      node->date_last_used() < delete_end;
  bool for_all_time =
      delete_begin.is_null() && (delete_end.is_null() || delete_end.is_max());
  if (node->is_url() && (within_range || for_all_time)) {
    UpdateLastUsedTimeImpl(node, Time());
  }

  for (size_t i = 0; i < node->children().size(); ++i) {
    ClearLastUsedTimeInRangeRecursive(node->children()[i].get(), delete_begin,
                                      delete_end);
  }
}

void BookmarkModel::Copy(const BookmarkNode* node,
                         const BookmarkNode* new_parent,
                         size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(node);
  DCHECK(IsValidIndex(new_parent, index, true));
  DCHECK(!is_root_node(new_parent));
  DCHECK(!is_permanent_node(node));
  DCHECK(!new_parent->HasAncestor(node));

  SetDateFolderModified(new_parent, Time::Now());
  BookmarkNodeData drag_data(node);
  // CloneBookmarkNode will use BookmarkModel methods to do the job, so we
  // don't need to send notifications here.
  CloneBookmarkNode(this, drag_data.elements, new_parent, index, true);

  if (store_) {
    store_->ScheduleSave();
  }
}

const gfx::Image& BookmarkModel::GetFavicon(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  if (node->favicon_state() == BookmarkNode::INVALID_FAVICON) {
    BookmarkNode* mutable_node = AsMutable(node);
    LoadFavicon(mutable_node);
  }
  return node->favicon();
}

void BookmarkModel::SetTitle(const BookmarkNode* node,
                             const std::u16string& title,
                             metrics::BookmarkEditSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  if (node->GetTitle() == title) {
    return;
  }

  if (is_permanent_node(node) && !client_->CanSetPermanentNodeTitle(node)) {
    NOTREACHED();
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkNode(this, node);
  }

  // The title index doesn't support changing the title, instead we remove then
  // add it back. Only do this for URL nodes. A directory node can have its
  // title changed but should be excluded from the index.
  if (node->is_url()) {
    titled_url_index_->Remove(node);
  } else {
    titled_url_index_->RemovePath(node);
  }
  url_index_->SetTitle(AsMutable(node), title);
  if (node->is_url()) {
    titled_url_index_->Add(node);
  } else {
    titled_url_index_->AddPath(node);
  }

  if (store_) {
    store_->ScheduleSave();
  }

  metrics::RecordTitleEdit(source);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChanged(this, node);
  }
}

void BookmarkModel::SetURL(const BookmarkNode* node,
                           const GURL& url,
                           metrics::BookmarkEditSource source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!node->is_folder());

  if (node->url() == url) {
    return;
  }

  BookmarkNode* mutable_node = AsMutable(node);
  mutable_node->InvalidateFavicon();
  CancelPendingFaviconLoadRequests(mutable_node);

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkNode(this, node);
  }

  // The title index doesn't support changing the URL, instead we remove then
  // add it back.
  titled_url_index_->Remove(mutable_node);
  url_index_->SetUrl(mutable_node, url);
  titled_url_index_->Add(mutable_node);

  if (store_) {
    store_->ScheduleSave();
  }

  metrics::RecordURLEdit(source);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChanged(this, node);
  }
}

void BookmarkModel::SetNodeMetaInfo(const BookmarkNode* node,
                                    const std::string& key,
                                    const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string old_value;
  if (node->GetMetaInfo(key, &old_value) && old_value == value) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(this, node);
  }

  if (AsMutable(node)->SetMetaInfo(key, value) && store_.get()) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(this, node);
  }
}

void BookmarkModel::SetNodeMetaInfoMap(
    const BookmarkNode* node,
    const BookmarkNode::MetaInfoMap& meta_info_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BookmarkNode::MetaInfoMap* old_meta_info_map = node->GetMetaInfoMap();
  if ((!old_meta_info_map && meta_info_map.empty()) ||
      (old_meta_info_map && meta_info_map == *old_meta_info_map)) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(this, node);
  }

  AsMutable(node)->SetMetaInfoMap(meta_info_map);
  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(this, node);
  }
}

void BookmarkModel::DeleteNodeMetaInfo(const BookmarkNode* node,
                                       const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BookmarkNode::MetaInfoMap* meta_info_map = node->GetMetaInfoMap();
  if (!meta_info_map || meta_info_map->find(key) == meta_info_map->end()) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillChangeBookmarkMetaInfo(this, node);
  }

  if (AsMutable(node)->DeleteMetaInfo(key) && store_.get()) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkMetaInfoChanged(this, node);
  }
}

void BookmarkModel::OnFaviconsChanged(const std::set<GURL>& page_urls,
                                      const GURL& icon_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return;
  }

  std::set<const BookmarkNode*> to_update;
  for (const GURL& page_url : page_urls) {
    std::vector<const BookmarkNode*> nodes = GetNodesByURL(page_url);
    to_update.insert(nodes.begin(), nodes.end());
  }

  if (!icon_url.is_empty()) {
    // TODO(pkotwicz): Do something more efficient if `icon_url` is non-empty
    // many times a day for each user.
    url_index_->GetNodesWithIconUrl(icon_url, &to_update);
  }

  for (const BookmarkNode* node : to_update) {
    // Rerequest the favicon.
    BookmarkNode* mutable_node = AsMutable(node);
    mutable_node->InvalidateFavicon();
    CancelPendingFaviconLoadRequests(mutable_node);
    for (BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeFaviconChanged(this, node);
    }
  }
}

void BookmarkModel::SetDateAdded(const BookmarkNode* node, Time date_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);
  DCHECK(!is_permanent_node(node));

  if (node->date_added() == date_added) {
    return;
  }

  AsMutable(node)->set_date_added(date_added);

  // Syncing might result in dates newer than the folder's last modified date.
  if (date_added > node->parent()->date_folder_modified()) {
    // Will trigger store_->ScheduleSave().
    SetDateFolderModified(node->parent(), date_added);
  } else if (store_) {
    store_->ScheduleSave();
  }
}

std::vector<const BookmarkNode*> BookmarkModel::GetNodesByURL(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<const BookmarkNode*> nodes;

  if (url_index_) {
    url_index_->GetNodesByUrl(url, &nodes);
  }

  return nodes;
}

const BookmarkNode* BookmarkModel::GetNodeByUuid(const base::Uuid& uuid) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Because of having to create a dummy node, the invalid-UUID case needs
  // special handling.
  if (!uuid.is_valid()) {
    return nullptr;
  }

  // TODO(mastiz): When C++20 is supported in Chromium, this map lookup should
  // be possible without having to use a BookmarkNode instance.
  BookmarkNode dummy_node_for_lookup(/*id=*/0, uuid, GURL());

  auto it = uuid_index_.find(&dummy_node_for_lookup);
  return it == uuid_index_.end() ? nullptr : *it;
}

const BookmarkNode* BookmarkModel::GetMostRecentlyAddedUserNodeForURL(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<const BookmarkNode*> nodes = GetNodesByURL(url);
  std::sort(nodes.begin(), nodes.end(), &MoreRecentlyAdded);

  // Look for the first node that the user can edit.
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (!client_->IsNodeManaged(nodes[i])) {
      return nodes[i];
    }
  }

  return nullptr;
}

bool BookmarkModel::HasBookmarks() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->HasBookmarks();
}

bool BookmarkModel::HasNoUserCreatedBookmarksOrFolders() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bookmark_bar_node_->children().empty() &&
         other_node_->children().empty() && mobile_node_->children().empty();
}

bool BookmarkModel::IsBookmarked(const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_index_ && url_index_->IsBookmarked(url);
}

std::vector<UrlAndTitle> BookmarkModel::GetUniqueUrls() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url_index_) {
    return {};
  }
  return url_index_->GetUniqueUrls();
}

metrics::BookmarkFolderTypeForUMA BookmarkModel::GetFolderType(
    const BookmarkNode* folder) const {
  CHECK(folder->is_folder());
  if (folder == bookmark_bar_node()) {
    return metrics::BookmarkFolderTypeForUMA::kBookmarksBar;
  } else if (folder == other_node()) {
    return metrics::BookmarkFolderTypeForUMA::kOtherBookmarks;
  } else if (folder == mobile_node()) {
    return metrics::BookmarkFolderTypeForUMA::kMobileBookmarks;
  }
  return metrics::BookmarkFolderTypeForUMA::kUserGeneratedFolder;
}

const BookmarkNode* BookmarkModel::AddFolder(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const BookmarkNode::MetaInfoMap* meta_info,
    absl::optional<base::Time> creation_time,
    absl::optional<base::Uuid> uuid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(IsValidIndex(parent, index, true));
  DCHECK(!uuid || uuid->is_valid());

  const base::Time provided_creation_time_or_now =
      creation_time.value_or(Time::Now());

  auto new_node = std::make_unique<BookmarkNode>(
      generate_next_node_id(), uuid.value_or(base::Uuid::GenerateRandomV4()),
      GURL());
  new_node->set_date_added(provided_creation_time_or_now);
  new_node->set_date_folder_modified(provided_creation_time_or_now);
  // Folders shouldn't have line breaks in their titles.
  new_node->SetTitle(title);
  if (meta_info) {
    new_node->SetMetaInfoMap(*meta_info);
  }
  metrics::RecordBookmarkFolderAdded(GetFolderType(parent),
                                     client_->GetStorageStateForUma());
  return AddNode(AsMutable(parent), index, std::move(new_node));
}

const BookmarkNode* BookmarkModel::AddNewURL(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const BookmarkNode::MetaInfoMap* meta_info) {
  metrics::RecordUrlBookmarkAdded(GetFolderType(parent),
                                  client_->GetStorageStateForUma());
  return AddURL(parent, index, title, url, meta_info, absl::nullopt,
                absl::nullopt, true);
}

const BookmarkNode* BookmarkModel::AddURL(
    const BookmarkNode* parent,
    size_t index,
    const std::u16string& title,
    const GURL& url,
    const BookmarkNode::MetaInfoMap* meta_info,
    absl::optional<base::Time> creation_time,
    absl::optional<base::Uuid> uuid,
    bool added_by_user) {
  // TODO(b/294100289): We should ensure that the specified UUID does not
  //                    conflict with a reserved folder ID.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(url.is_valid());
  DCHECK(parent);
  DCHECK(parent->is_folder());
  DCHECK(!is_root_node(parent));
  DCHECK(IsValidIndex(parent, index, true));
  DCHECK(!uuid || uuid->is_valid());

  const base::Time provided_creation_time_or_now =
      creation_time.value_or(Time::Now());

  // Syncing may result in dates newer than the last modified date.
  if (provided_creation_time_or_now > parent->date_folder_modified()) {
    SetDateFolderModified(parent, provided_creation_time_or_now);
  }

  auto new_node = std::make_unique<BookmarkNode>(
      generate_next_node_id(), uuid.value_or(base::Uuid::GenerateRandomV4()),
      url);
  new_node->SetTitle(title);
  new_node->set_date_added(provided_creation_time_or_now);
  if (meta_info) {
    new_node->SetMetaInfoMap(*meta_info);
  }

  return AddNode(AsMutable(parent), index, std::move(new_node), added_by_user);
}

void BookmarkModel::SortChildren(const BookmarkNode* parent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_->IsNodeManaged(parent));

  if (!parent || !parent->is_folder() || is_root_node(parent) ||
      parent->children().size() <= 1) {
    return;
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillReorderBookmarkNode(this, parent);
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  if (U_FAILURE(error)) {
    collator.reset(nullptr);
  }

  AsMutable(parent)->SortChildren(SortComparator(collator.get()));

  if (store_) {
    store_->ScheduleSave();
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChildrenReordered(this, parent);
  }
}

void BookmarkModel::ReorderChildren(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& ordered_nodes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_->IsNodeManaged(parent));

  // Ensure that all children in `parent` are in `ordered_nodes`.
  DCHECK_EQ(parent->children().size(), ordered_nodes.size());
  for (const BookmarkNode* node : ordered_nodes) {
    DCHECK_EQ(parent, node->parent());
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.OnWillReorderBookmarkNode(this, parent);
  }

  if (ordered_nodes.size() > 1) {
    std::map<const BookmarkNode*, int> order;
    for (size_t i = 0; i < ordered_nodes.size(); ++i) {
      order[ordered_nodes[i]] = i;
    }

    std::vector<size_t> new_order(ordered_nodes.size());
    for (size_t old_index = 0; old_index < parent->children().size();
         ++old_index) {
      const BookmarkNode* node = parent->children()[old_index].get();
      size_t new_index = order[node];
      new_order[old_index] = new_index;
    }

    AsMutable(parent)->ReorderChildren(new_order);

    if (store_) {
      store_->ScheduleSave();
    }
  }

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeChildrenReordered(this, parent);
  }
}

void BookmarkModel::SetDateFolderModified(const BookmarkNode* parent,
                                          const Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(parent);
  AsMutable(parent)->set_date_folder_modified(time);

  if (store_) {
    store_->ScheduleSave();
  }
}

void BookmarkModel::ResetDateFolderModified(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetDateFolderModified(node, Time());
}

std::vector<TitledUrlMatch> BookmarkModel::GetBookmarksMatching(
    const std::u16string& query,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loaded_) {
    return {};
  }

  return titled_url_index_->GetResultsMatching(query, max_count,
                                               matching_algorithm);
}

void BookmarkModel::ClearStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_.reset();
}

// static
void BookmarkModel::WipeAccountStorageForRollback(
    const base::FilePath& profile_path) {
  CHECK(base::FeatureList::IsEnabled(
      bookmarks::kRollbackBookmarksAccountStorage));

  base::FilePath account_storage_path =
      GetStorageFilePath(profile_path, StorageType::kAccount);
  base::ThreadPool::PostTask(
      FROM_HERE, base::MayBlock(),
      base::BindOnce(&DeleteAccountStorageFileSynchronously,
                     std::move(account_storage_path)));
}

void BookmarkModel::RestoreRemovedNode(const BookmarkNode* parent,
                                       size_t index,
                                       std::unique_ptr<BookmarkNode> node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  AddNode(AsMutable(parent), index, std::move(node));

  // We might be restoring a folder node that have already contained a set of
  // child nodes. We need to notify all of them.
  NotifyNodeAddedForAllDescendants(node_ptr, /*added_by_user=*/false);
}

void BookmarkModel::NotifyNodeAddedForAllDescendants(const BookmarkNode* node,
                                                     bool added_by_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < node->children().size(); ++i) {
    for (BookmarkModelObserver& observer : observers_) {
      observer.BookmarkNodeAdded(this, node, i, added_by_user);
    }
    NotifyNodeAddedForAllDescendants(node->children()[i].get(), added_by_user);
  }
}

void BookmarkModel::RemoveNodeFromIndicesRecursive(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  DCHECK(!is_permanent_node(node));

  if (node->is_url()) {
    titled_url_index_->Remove(node);
  } else {
    titled_url_index_->RemovePath(node);
  }

  uuid_index_.erase(node);

  // Reset favicon state for the case when the `node` is restored.
  CancelPendingFaviconLoadRequests(node);
  node->InvalidateFavicon();

  // Recurse through children.
  for (size_t i = node->children().size(); i > 0; --i) {
    RemoveNodeFromIndicesRecursive(node->children()[i - 1].get());
  }
}

void BookmarkModel::DoneLoading(std::unique_ptr<BookmarkLoadDetails> details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(details);
  DCHECK(!loaded_);

  next_node_id_ = details->max_id();
  if (details->computed_checksum() != details->stored_checksum() ||
      details->ids_reassigned() || details->uuids_reassigned()) {
    // If bookmarks file changed externally, the IDs may have changed
    // externally. In that case, the decoder may have reassigned IDs to make
    // them unique. So when the file has changed externally, we should save the
    // bookmarks file to persist such changes. The same applies if new UUIDs
    // have been assigned to bookmarks.
    if (store_) {
      store_->ScheduleSave();
    }
  }

  titled_url_index_ = details->owned_titled_url_index();
  uuid_index_ = details->owned_uuid_index();
  url_index_ = details->url_index();
  root_ = details->root_node();
  // See declaration for details on why `owned_root_` is reset.
  owned_root_.reset();
  bookmark_bar_node_ = details->bb_node();
  other_node_ = details->other_folder_node();
  mobile_node_ = details->mobile_folder_node();

  titled_url_index_->SetNodeSorter(
      std::make_unique<TypedCountSorter>(client_.get()));
  // Sorting the permanent nodes has to happen on the main thread, so we do it
  // here, after loading completes.
  root_->SortChildren(VisibilityComparator(client_.get()));

  root_->SetMetaInfoMap(details->model_meta_info_map());

  loaded_ = true;
  client_->DecodeBookmarkSyncMetadata(
      details->sync_metadata_str(),
      store_ ? base::BindRepeating(&BookmarkStorage::ScheduleSave,
                                   base::Unretained(store_.get()))
             : base::DoNothing());

  const base::TimeDelta load_duration =
      base::TimeTicks::Now() - details->load_start();
  metrics::RecordTimeToLoadAtStartup(load_duration,
                                     client_->GetStorageStateForUma());

  // Notify our direct observers.
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkModelLoaded(this, details->ids_reassigned());
  }
}

BookmarkNode* BookmarkModel::AddNode(BookmarkNode* parent,
                                     size_t index,
                                     std::unique_ptr<BookmarkNode> node,
                                     bool added_by_user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BookmarkNode* node_ptr = node.get();
  url_index_->Add(parent, index, std::move(node));

  if (store_) {
    store_->ScheduleSave();
  }

  AddNodeToIndicesRecursive(node_ptr);

  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeAdded(this, parent, index, added_by_user);
  }

  return node_ptr;
}

void BookmarkModel::AddNodeToIndicesRecursive(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool uuid_is_unique = uuid_index_.insert(node).second;
  DUMP_WILL_BE_CHECK(uuid_is_unique);

  if (node->is_url()) {
    titled_url_index_->Add(node);
  } else {
    titled_url_index_->AddPath(node);
  }

  for (const auto& child : node->children()) {
    AddNodeToIndicesRecursive(child.get());
  }
}

bool BookmarkModel::IsValidIndex(const BookmarkNode* parent,
                                 size_t index,
                                 bool allow_end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent && parent->is_folder() &&
         (index < parent->children().size() ||
          (allow_end && index == parent->children().size()));
}

void BookmarkModel::OnFaviconDataAvailable(
    BookmarkNode* node,
    const favicon_base::FaviconImageResult& image_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(node);

  node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  node->set_favicon_state(BookmarkNode::LOADED_FAVICON);
  if (!image_result.image.IsEmpty()) {
    node->set_favicon(image_result.image);
    node->set_icon_url(image_result.icon_url);
    FaviconLoaded(node);
  } else {
    // No favicon available, but we still notify observers.
    FaviconLoaded(node);
  }
}

void BookmarkModel::LoadFavicon(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (node->is_folder()) {
    return;
  }

  DCHECK(node->url().is_valid());
  node->set_favicon_state(BookmarkNode::LOADING_FAVICON);
  base::CancelableTaskTracker::TaskId taskId =
      client_->GetFaviconImageForPageURL(
          node->url(),
          base::BindOnce(&BookmarkModel::OnFaviconDataAvailable,
                         base::Unretained(this), node),
          &cancelable_task_tracker_);
  if (taskId != base::CancelableTaskTracker::kBadTaskId) {
    node->set_favicon_load_task_id(taskId);
  }
}

void BookmarkModel::FaviconLoaded(const BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (BookmarkModelObserver& observer : observers_) {
    observer.BookmarkNodeFaviconChanged(this, node);
  }
}

void BookmarkModel::CancelPendingFaviconLoadRequests(BookmarkNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (node->favicon_load_task_id() != base::CancelableTaskTracker::kBadTaskId) {
    cancelable_task_tracker_.TryCancel(node->favicon_load_task_id());
    node->set_favicon_load_task_id(base::CancelableTaskTracker::kBadTaskId);
  }
}

int64_t BookmarkModel::generate_next_node_id() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(loaded_);
  return next_node_id_++;
}

}  // namespace bookmarks
