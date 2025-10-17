// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_H_
#define COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_H_

#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/stack_allocated.h"
#include "base/types/pass_key.h"
#include "components/tabs/public/supports_handles.h"
#include "components/tabs/public/tab_collection_storage.h"

class TabStripServiceImpl;

namespace tabs {

class TabInterface;

// This is an interface that representing the hierarchical storage of tabs.
// This can be used to access and manipulate tabs and the state of the tabstrip.
// Different types of collections should implement this base class based on how
// their feature works. For example, a pinned collection can implement tab
// collection that does not store any collection.
class TabCollection : public SupportsHandles<TabCollection> {
 public:
  // TabIterator provides a way to traverse all tab objects within this
  // TabCollection and its sub-collections in a depth first inorder traversal
  // manner. This should not be mutated while holding reference to an iterator
  // as otherwise it will break as it is holding access to the index and tab
  // pointer.
  class TabIterator {
    STACK_ALLOCATED();

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = tabs::TabInterface*;
    using difference_type = ptrdiff_t;
    using pointer = value_type;
    using reference = value_type;

    TabIterator();
    TabIterator(base::PassKey<TabCollection>,
                const tabs::TabCollection* root,
                bool is_end = false);
    TabIterator(const TabIterator& iterator);
    ~TabIterator();

    pointer operator->() const { return cur_; }
    reference operator*() const { return cur_; }

    TabIterator& operator++() {
      Next();
      return *this;
    }

    TabIterator operator++(int) {
      TabIterator it(*this);
      Next();
      return it;
    }

    bool operator==(const TabIterator& other) const {
      return cur_ == other.cur_;
    }

   private:
    TabIterator(const tabs::TabCollection* root, bool is_end);
    void Next();

    // Contains information of the index within a collection to access during
    // the tree traversal. Multiple frames can be stored in the stack which
    // corresponds to children for multiple subcollection.
    struct Frame {
      raw_ptr<const TabCollection> collection;
      size_t index;
    };

    // Points to the currently accessed tab during iteration.
    raw_ptr<tabs::TabInterface> cur_;

    // Points to the root tab collection that the iterator is traversing.
    raw_ptr<const tabs::TabCollection> root_;

    // A stack used to maintain the traversal state for inorder traversal
    // of tabs within the TabCollection hierarchy. Each Frame on the stack
    // is a TabCollection in the current path of traversal, along
    // with the index of the next child to be visited. This is implemented as a
    // vector to take advantage of reserving memory for performance reasons.
    std::vector<Frame> stack_;
  };

  using iterator = TabIterator;
  using const_iterator = TabIterator;

  const_iterator begin() const {
    return TabIterator(GetPassKey(), this, false);
  }
  const_iterator end() const { return TabIterator(GetPassKey(), this, true); }

  // This is an iterator that iterates a TabCollection with a pre-order DFS
  // algorithm. collection_begin() starts with the root which is always a
  // TabCollection. For example, fetching and advancing on the
  // TabStripCollection would have a sequence like the following:
  // TabStrip -> Pinned -> PTab1 -> PTab2 -> Unpinned -> UPTab1.
  //
  // This tree uses a variant (TabCollection or TabInterface) for its child
  // nodes. While TabIterator() only iterates through TabInterfaces, Iterator()
  // will iterate on both child node types.
  class Iterator {
    STACK_ALLOCATED();

   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = tabs::ConstChildPtr;
    using pointer = value_type;
    using reference = value_type;

    Iterator();
    Iterator(base::PassKey<TabCollection>,
             const TabCollection* root,
             bool is_end = false);
    Iterator(const Iterator& other);
    ~Iterator();

    pointer operator->() const { return cur_; }

    reference operator*() const { return cur_; }

    Iterator& operator++() {
      Advance();
      return *this;
    }

    Iterator operator++(int) {
      Iterator it(*this);
      Advance();
      return it;
    }

    bool operator==(const Iterator& other) const {
      return cur_ == other.cur_ && root_collection_ == other.root_collection_ &&
             is_end_iterator_ == other.is_end_iterator_;
    }

   private:
    void Advance();

    struct Frame {
      raw_ptr<const TabCollection> collection;
      size_t child_idx;
    };

    std::vector<Frame> stack_;
    value_type cur_;
    raw_ptr<const TabCollection> root_collection_;
    bool is_end_iterator_;
  };

  Iterator collection_begin(base::PassKey<TabStripServiceImpl> key) const {
    return Iterator(GetPassKey(), this, false);
  }

  Iterator collection_end(base::PassKey<TabStripServiceImpl> key) const {
    return Iterator(GetPassKey(), this, true);
  }

  // Type describes the various kinds of tab collections:
  // - TABSTRIP:  The main container for tabs in a browser window.
  // - PINNED:    A container for pinned tabs.
  // - UNPINNED:  A container for unpinned tabs.
  // - GROUP:     A container to grouped tabs.
  // - SPLIT:     A container for split tabs.
  enum class Type { TABSTRIP, PINNED, UNPINNED, GROUP, SPLIT };

  ~TabCollection() override;
  TabCollection(const TabCollection&) = delete;
  TabCollection& operator=(const TabCollection&) = delete;

  // Returns true is the tab collection contains the collection. This is a
  // non-recursive check.
  bool ContainsCollection(TabCollection* collection) const;

  // Non-recursively get the index of a tab.
  std::optional<size_t> GetIndexOfTab(const TabInterface* tab) const;

  // Recursively get the index of the tab among all the leaf tabs.
  std::optional<size_t> GetIndexOfTabRecursive(const TabInterface* tab) const;

  // Recursively get the tab at the index.
  TabInterface* GetTabAtIndexRecursive(size_t index) const;

  // Returns a flattened list of all tabs in this collection and its
  // subcollections. Prefer using the result of this function instead of looping
  // over an index and using GetTabAtIndexRecursive, as this function will only
  // do one pass over the subcollections.
  std::vector<TabInterface*> GetTabsRecursive() const;

  // Non-recursively get the index of a collection.
  std::optional<size_t> GetIndexOfCollection(TabCollection* collection) const;

  // Convert a recursive index to a direct index. Fails a CHECK if the tab at
  // the recursive index lies within a subcollection.
  size_t ToDirectIndex(size_t index);

  // Total number of children that directly have this collection as their
  // parent.
  size_t ChildCount() const;

  TabCollectionStorage* GetTabCollectionStorageForTesting() {
    return impl_.get();
  }

  Type type() const { return type_; }

  // Total number of tabs the collection contains.
  size_t TabCountRecursive() const { return recursive_tab_count_; }

  // Callbacks that are triggered by the impl_ when tabs/collections are added
  // or removed from the tree.
  void OnCollectionAddedToTree(TabCollection* collection);
  void OnCollectionRemovedFromTree(TabCollection* collection);
  void OnTabAddedToTree();
  void OnTabRemovedFromTree();

  // Manipulate direct child tabs.
  TabInterface* AddTab(std::unique_ptr<TabInterface> tab, size_t index);
  void MoveTab(TabInterface* tab, size_t index);
  // Removes the tab if it is a direct child of this collection. This is then
  // returned to the caller as an unique_ptr. If the tab is not present it will
  // crash. This may overridden to return nullptr if the collection does not
  // support removing tabs.
  [[nodiscard]] virtual std::unique_ptr<TabInterface> MaybeRemoveTab(
      TabInterface* tab);

  // Manipulate direct child collections.
  // Adds a collection as a direct child of this collection. If this succeeds it
  // will return a pointer to the collection, otherwise it will return nullptr.
  template <std::derived_from<TabCollection> T>
  T* AddCollection(std::unique_ptr<T> collection, size_t index) {
    CHECK(collection);
    CHECK(supported_child_collections_.contains(collection->type()));
    CHECK(index <= ChildCount());

    TabCollection* added_collection =
        impl_->AddCollection(std::move(collection), index);
    added_collection->OnReparented(this);
    return static_cast<T*>(added_collection);
  }
  // Removes the collection if it is a direct child of this collection. This is
  // then returned to the caller as an unique_ptr. If the collection is not
  // present it will crash. This may be overridden to return nullptr if the
  // collection does not support removing collections.
  [[nodiscard]] virtual std::unique_ptr<TabCollection> MaybeRemoveCollection(
      TabCollection* collection);

  TabCollection* GetParentCollection() const { return parent_; }

  // This should be called either when this collection is added to another
  // collection or it is removed from another collection. The child collection
  // should not try to call this internally and set its parent.
  void OnReparented(TabCollection* new_parent);

 protected:
  explicit TabCollection(Type type,
                         std::unordered_set<Type> supported_child_collections,
                         bool supports_tabs);

  // Returns the pass key to be used by derived classes as operations such as
  // setting the parent of a tab can only be performed by a `TabCollection`.
  base::PassKey<TabCollection> GetPassKey() const {
    return base::PassKey<TabCollection>();
  }

  const ChildrenVector& GetChildren() const { return impl_->GetChildren(); }

  // Total number of tabs in the collection.
  size_t recursive_tab_count_ = 0;

 private:
  raw_ptr<TabCollection> parent_ = nullptr;
  Type type_;
  std::unordered_set<Type> supported_child_collections_;
  bool supports_tabs_;

  // Underlying implementation for the storage of children.
  std::unique_ptr<TabCollectionStorage> impl_;
};

using TabCollectionHandle = TabCollection::Handle;

}  // namespace tabs

#endif  // COMPONENTS_TABS_PUBLIC_TAB_COLLECTION_H_
