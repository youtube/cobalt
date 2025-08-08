// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

// VectorBackedLinkedList iterators are not invalidated by mutation of the
// collection, unless they point to removed items. This means, for example, that
// you can safely modify the container while iterating over it generally, as
// long as you don't remove the current item. Moving items does not invalidate
// iterator, so that it may cause unexpected behavior (i.e. loop unexpectedly
// ends when moving the current item to last).

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListReverseIterator;
template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstReverseIterator;

template <typename ValueType, typename Allocator>
class VectorBackedLinkedListNode {
  USE_ALLOCATOR(VectorBackedLinkedListNode, Allocator);

 public:
  VectorBackedLinkedListNode() = delete;

  VectorBackedLinkedListNode(wtf_size_t prev_index, wtf_size_t next_index)
      : prev_index_(prev_index), next_index_(next_index) {}

  VectorBackedLinkedListNode(wtf_size_t prev_index,
                             wtf_size_t next_index,
                             const ValueType& value)
      : prev_index_(prev_index), next_index_(next_index), value_(value) {}

  VectorBackedLinkedListNode(wtf_size_t prev_index,
                             wtf_size_t next_index,
                             ValueType&& value)
      : prev_index_(prev_index),
        next_index_(next_index),
        value_(std::move(value)) {}

  VectorBackedLinkedListNode(const VectorBackedLinkedListNode& other) = default;
  VectorBackedLinkedListNode(VectorBackedLinkedListNode&& other) = default;
  VectorBackedLinkedListNode& operator=(
      const VectorBackedLinkedListNode& other) = default;
  VectorBackedLinkedListNode& operator=(VectorBackedLinkedListNode&& other) =
      default;

  template <typename VisitorDispathcer, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(
      VisitorDispathcer visitor) const {
    if (!WTF::IsWeak<ValueType>::value) {
      visitor->Trace(value_);
    }
  }

  // Those indices can be initialized with |kNotFound| (not with 0), since
  // VectorBackedLinkedList won't be initialized with memset.
  wtf_size_t prev_index_ = kNotFound;
  wtf_size_t next_index_ = kNotFound;
  ValueType value_ = HashTraits<ValueType>::EmptyValue();
};

template <typename ValueType, typename Allocator>
struct VectorTraits<VectorBackedLinkedListNode<ValueType, Allocator>>
    : VectorTraitsBase<VectorBackedLinkedListNode<ValueType, Allocator>> {
  STATIC_ONLY(VectorTraits);

  static const bool kNeedsDestruction =
      VectorTraits<ValueType>::kNeedsDestruction;
  // VectorBackedLinkedList can't be initialized with memset, because we use
  // kNotFound as sentinel value.
  static const bool kCanInitializeWithMemset = false;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<ValueType>::kCanClearUnusedSlotsWithMemset;
  static const bool kCanCopyWithMemcpy =
      VectorTraits<ValueType>::kCanCopyWithMemcpy;
  static const bool kCanMoveWithMemcpy =
      VectorTraits<ValueType>::kCanMoveWithMemcpy;

  static constexpr bool kCanTraceConcurrently =
      VectorTraits<ValueType>::kCanTraceConcurrently;
};

template <typename ValueType, typename Traits, typename Allocator>
class ConstructTraits<VectorBackedLinkedListNode<ValueType, Allocator>,
                      Traits,
                      Allocator> {
  STATIC_ONLY(ConstructTraits);

  using Node = VectorBackedLinkedListNode<ValueType, Allocator>;

 public:
  template <typename... Args>
  static Node* Construct(void* location, Args&&... args) {
    return new (NotNullTag::kNotNull, location)
        Node(std::forward<Args>(args)...);
  }

  static void NotifyNewElement(Node* element) {
    Allocator::template NotifyNewObject<Node, Traits>(element);
  }

  template <typename... Args>
  static Node* ConstructAndNotifyElement(void* location, Args&&... args) {
    Node* object = ConstructAndNotifyElementImpl::Construct(
        location, std::forward<Args>(args)...);
    NotifyNewElement(object);
    return object;
  }

  static void NotifyNewElements(Node* array, size_t len) {
    Allocator::template NotifyNewObjects<Node, Traits>(array, len);
  }

 private:
  struct ConstructAndNotifyElementImplNotGarbageCollected {
    template <typename... Args>
    static Node* Construct(void* location, Args&&... args) {
      return ConstructTraits<Node, Traits, Allocator>::Construct(
          location, std::forward<Args>(args)...);
    }
  };

  struct ConstructAndNotifyElementImplGarbageCollected {
    static Node* Construct(void* location, Node&& element) {
      // ConstructAndNotifyElement updates an existing node which might
      // also be concurrently traced while we update it. The regular ctors
      // don't use an atomic write which can lead to data races.
      static_assert(VectorTraits<Node>::kCanMoveWithMemcpy,
                    "Garbage collected types used in VectorBackedLinkedList "
                    "should be movable with memcpy");
      AtomicWriteMemcpy<sizeof(Node), alignof(Node)>(location, &element);
      return reinterpret_cast<Node*>(location);
    }
  };

  using ConstructAndNotifyElementImpl = typename std::conditional<
      Allocator::kIsGarbageCollected,
      ConstructAndNotifyElementImplGarbageCollected,
      ConstructAndNotifyElementImplNotGarbageCollected>::type;
};

// VectorBackedLinkedList maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
// The linked list is implementing in a vector (with links being indexes instead
// of pointers), to simplify the move of backing during GC compaction.
template <typename ValueType, typename Allocator = PartitionAllocator>
class VectorBackedLinkedList {
  USE_ALLOCATOR(VectorBackedLinkedList, Allocator);

 private:
  using Node = VectorBackedLinkedListNode<ValueType, Allocator>;
  // Using Vector like this (instead of HeapVector for garbage collected types)
  // skips the checks for HeapVector in heap_allocator.h. This is necessary
  // because HeapVector doesn't allow WeakMember, but we need to support
  // VectorBackedLinkedList<WeakMember>.
  using VectorType = Vector<Node, 0, Allocator>;

 public:
  using Value = ValueType;
  using iterator = VectorBackedLinkedListIterator<VectorBackedLinkedList>;
  using const_iterator =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedList>;
  friend class VectorBackedLinkedListConstIterator<VectorBackedLinkedList>;
  using reverse_iterator =
      VectorBackedLinkedListReverseIterator<VectorBackedLinkedList>;
  using const_reverse_iterator =
      VectorBackedLinkedListConstReverseIterator<VectorBackedLinkedList>;

  void swap(VectorBackedLinkedList&);

  bool empty() const { return size_ == 0; }
  wtf_size_t size() const { return size_; }

  iterator begin() { return MakeIterator(UsedFirstIndex()); }
  const_iterator begin() const { return MakeConstIterator(UsedFirstIndex()); }
  const_iterator cbegin() const { return MakeConstIterator(UsedFirstIndex()); }
  iterator end() { return MakeIterator(anchor_index_); }
  const_iterator end() const { return MakeConstIterator(anchor_index_); }
  const_iterator cend() const { return MakeConstIterator(anchor_index_); }
  reverse_iterator rbegin() { return MakeReverseIterator(UsedLastIndex()); }
  const_reverse_iterator rbegin() const {
    return MakeConstReverseIterator(UsedLastIndex());
  }
  const_reverse_iterator crbegin() const {
    return MakeConstReverseIterator(UsedLastIndex());
  }
  reverse_iterator rend() { return MakeReverseIterator(anchor_index_); }
  const_reverse_iterator rend() const {
    return MakeConstReverseIterator(anchor_index_);
  }
  const_reverse_iterator crend() const {
    return MakeConstReverseIterator(anchor_index_);
  }

  Value& front();
  const Value& front() const;
  Value& back();
  const Value& back() const;

  template <typename IncomingValueType>
  iterator insert(const_iterator position, IncomingValueType&& value);

  template <typename IncomingValueType>
  void push_front(IncomingValueType&& value) {
    insert(cbegin(), std::forward<IncomingValueType>(value));
  }

  template <typename IncomingValueType>
  void push_back(IncomingValueType&& value) {
    insert(cend(), std::forward<IncomingValueType>(value));
  }

  // Moves |target| right before |new_position| in a linked list. This operation
  // is executed by just updating indices of related nodes.
  iterator MoveTo(const_iterator target, const_iterator new_position);

  iterator erase(const_iterator);

  void pop_front() {
    DCHECK(!empty());
    erase(cbegin());
  }
  void pop_back() {
    DCHECK(!empty());
    erase(--cend());
  }

  // Removes all elements in a linked list.
  void clear() {
    // Keep anchor so that we can insert elements after this operation.
    nodes_.ShrinkCapacity(1);
    nodes_[anchor_index_].prev_index_ = anchor_index_;
    nodes_[anchor_index_].next_index_ = anchor_index_;
    free_head_index_ = anchor_index_;
    size_ = 0;
  }

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(
      VisitorDispatcher visitor) const {
    nodes_.Trace(visitor);
    if (WTF::IsWeak<ValueType>::value) {
      visitor->template RegisterWeakCallbackMethod<
          VectorBackedLinkedList,
          &VectorBackedLinkedList::ProcessCustomWeakness>(this);
    }
  }

 private:
  // The constructors are private, because the class is used only by
  // LinkedHashSet and we don't want it to be instantiated directly otherwise.
  // There are a couple resonts for that:
  // 1. We know that usage of VectorBackedLinkedList in LinkedHashSet is safe,
  //    since it is limited to Member and WeakMember for GCed sets. Other
  //    potential usages might not be safe.
  // 2. LinkedHashSet relies on indices inside VectorBackedLinkedList not
  //    changing. Usage of VectorBackedLinkedList outside of LinkedHashSet may
  //    encourage code optimizations that may break that assumption.
  VectorBackedLinkedList();
  VectorBackedLinkedList(const VectorBackedLinkedList&) = default;
  VectorBackedLinkedList(VectorBackedLinkedList&&) = default;
  VectorBackedLinkedList& operator=(const VectorBackedLinkedList&) = default;
  VectorBackedLinkedList& operator=(VectorBackedLinkedList&&) = default;
  ~VectorBackedLinkedList() = default;

  bool IsFreeListEmpty() const { return free_head_index_ == anchor_index_; }

  wtf_size_t UsedFirstIndex() const {
    return nodes_[anchor_index_].next_index_;
  }
  wtf_size_t UsedLastIndex() const { return nodes_[anchor_index_].prev_index_; }

  iterator MakeIterator(wtf_size_t index) { return iterator(index, this); }
  const_iterator MakeConstIterator(wtf_size_t index) const {
    return const_iterator(index, this);
  }
  reverse_iterator MakeReverseIterator(wtf_size_t index) {
    return reverse_iterator(index, this);
  }
  const_reverse_iterator MakeConstReverseIterator(wtf_size_t index) const {
    return const_reverse_iterator(index, this);
  }

  bool IsIndexValid(wtf_size_t index) const {
    return 0 <= index && index < nodes_.size();
  }

  bool IsAnchor(wtf_size_t index) const { return index == anchor_index_; }

  void Unlink(const Node&);

  template <typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> ProcessCustomWeakness(
      const typename A::LivenessBroker& broker) {
    auto it = begin();
    while (it != end()) {
      if (!broker.IsHeapObjectAlive(it->Get())) {
        // Calling erase() during the iteration is fine because this just
        // invokes (Weak)Member's destructor and is guaranteed not to reenter
        // the iteration.
        it = erase(it);
      } else {
        ++it;
      }
    }
  }

  VectorType nodes_;
  static constexpr wtf_size_t anchor_index_ = 0;
  // Anchor is not included in the free list, but it serves as the list's
  // terminator.
  wtf_size_t free_head_index_ = anchor_index_;
  wtf_size_t size_ = 0;

  template <typename T, typename U, typename V>
  friend class LinkedHashSet;
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, Insert);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, PushFront);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, PushBack);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, MoveTo);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, Erase);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, PopFront);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, PopBack);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, Clear);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, Iterator);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, ConstIterator);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, String);
  FRIEND_TEST_ALL_PREFIXES(VectorBackedLinkedListTest, UniquePtr);
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListIterator {
  DISALLOW_NEW();
  using ReferenceType = typename VectorBackedLinkedListType::Value&;
  using PointerType = typename VectorBackedLinkedListType::Value*;
  using Node = typename VectorBackedLinkedListConstIterator<
      VectorBackedLinkedListType>::Node;
  using const_iterator =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedListType>;

 public:
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  VectorBackedLinkedListIterator& operator++() {
    ++iterator_;
    return *this;
  }

  VectorBackedLinkedListIterator& operator--() {
    --iterator_;
    return *this;
  }

  VectorBackedLinkedListIterator& operator++(int) = delete;
  VectorBackedLinkedListIterator& operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListIterator& other) const {
    return iterator_ == other.iterator_;
  }

  bool operator!=(const VectorBackedLinkedListIterator& other) const {
    return !(*this == other);
  }

  operator const_iterator() const { return iterator_; }

 private:
  VectorBackedLinkedListIterator(const wtf_size_t index,
                                 VectorBackedLinkedListType* container)
      : iterator_(index, container) {}

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  wtf_size_t GetIndex() const { return iterator_.GetIndex(); }

  const_iterator iterator_;

  template <typename T, typename Allocator>
  friend class VectorBackedLinkedList;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstIterator {
  DISALLOW_NEW();
  using ReferenceType = const typename VectorBackedLinkedListType::Value&;
  using PointerType = const typename VectorBackedLinkedListType::Value*;
  using Node = typename VectorBackedLinkedListType::Node;

 public:
  PointerType Get() const {
    DCHECK(!container_->IsAnchor(GetIndex()));
    return &container_->nodes_[index_].value_;
  }

  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  wtf_size_t GetIndex() const { return index_; }

  VectorBackedLinkedListConstIterator& operator++() {
    wtf_size_t next_index = container_->nodes_[index_].next_index_;
    DCHECK(container_->IsIndexValid(next_index));
    index_ = next_index;
    return *this;
  }

  VectorBackedLinkedListConstIterator& operator--() {
    wtf_size_t prev_index = container_->nodes_[index_].prev_index_;
    DCHECK(container_->IsIndexValid(prev_index));
    index_ = prev_index;
    return *this;
  }

  VectorBackedLinkedListConstIterator operator++(int) = delete;
  VectorBackedLinkedListConstIterator operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListConstIterator& other) const {
    DCHECK_EQ(container_, other.container_);
    return index_ == other.index_;
  }

  bool operator!=(const VectorBackedLinkedListConstIterator& other) const {
    return !(*this == other);
  }

 protected:
  VectorBackedLinkedListConstIterator(
      const wtf_size_t index,
      const VectorBackedLinkedListType* container)
      : index_(index), container_(container) {}

 private:
  // The conservative stack scanning will strongly trace container_ and it
  // ensures that the container is kept alive during iteration.
  wtf_size_t index_;
  const VectorBackedLinkedListType* container_;

  template <typename T, typename Allocator>
  friend class VectorBackedLinkedList;
  friend class VectorBackedLinkedListIterator<VectorBackedLinkedListType>;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListReverseIterator {
  using ReferenceType = typename VectorBackedLinkedListType::Value&;
  using PointerType = typename VectorBackedLinkedListType::Value*;
  using Node = typename VectorBackedLinkedListConstIterator<
      VectorBackedLinkedListType>::Node;
  using const_reverse_iterator =
      VectorBackedLinkedListConstReverseIterator<VectorBackedLinkedListType>;

 public:
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  VectorBackedLinkedListReverseIterator& operator++() {
    ++iterator_;
    return *this;
  }

  VectorBackedLinkedListReverseIterator& operator--() {
    --iterator_;
    return *this;
  }

  VectorBackedLinkedListReverseIterator& operator++(int) = delete;
  VectorBackedLinkedListReverseIterator& operator--(int) = delete;

  bool operator==(const VectorBackedLinkedListReverseIterator& other) const {
    return iterator_ == other.iterator_;
  }

  bool operator!=(const VectorBackedLinkedListReverseIterator& other) const {
    return !(*this == other);
  }

  operator const_reverse_iterator() const { return iterator_; }

 private:
  VectorBackedLinkedListReverseIterator(const wtf_size_t index,
                                        VectorBackedLinkedListType* container)
      : iterator_(index, container) {}

  PointerType Get() const { return const_cast<PointerType>(iterator_.Get()); }
  wtf_size_t GetIndex() const { return iterator_.GetIndex(); }

  const_reverse_iterator iterator_;

  template <typename T, typename Allocator>
  friend class VectorBackedLinkedList;
};

template <typename VectorBackedLinkedListType>
class VectorBackedLinkedListConstReverseIterator
    : public VectorBackedLinkedListConstIterator<VectorBackedLinkedListType> {
  using Superclass =
      VectorBackedLinkedListConstIterator<VectorBackedLinkedListType>;
  using Node = typename Superclass::Node;

 public:
  VectorBackedLinkedListConstReverseIterator& operator++() {
    Superclass::operator--();
    return *this;
  }

  VectorBackedLinkedListConstReverseIterator& operator--() {
    Superclass::operator++();
    return *this;
  }

  VectorBackedLinkedListConstReverseIterator operator++(int) = delete;
  VectorBackedLinkedListConstReverseIterator operator--(int) = delete;

 private:
  VectorBackedLinkedListConstReverseIterator(
      const wtf_size_t index,
      const VectorBackedLinkedListType* container)
      : Superclass(index, container) {}

  template <typename T, typename Allocator>
  friend class VectorBackedLinkedList;
  friend class VectorBackedLinkedListReverseIterator<
      VectorBackedLinkedListType>;
};

template <typename T, typename Allocator>
VectorBackedLinkedList<T, Allocator>::VectorBackedLinkedList() {
  // First inserts anchor, which serves as the beginning and the end of
  // the used list.
  nodes_.push_back(Node(anchor_index_, anchor_index_));
}

template <typename T, typename Allocator>
inline void VectorBackedLinkedList<T, Allocator>::swap(
    VectorBackedLinkedList& other) {
  nodes_.swap(other.nodes_);
  std::swap(free_head_index_, other.free_head_index_);
  std::swap(size_, other.size_);
}

template <typename T, typename Allocator>
T& VectorBackedLinkedList<T, Allocator>::front() {
  DCHECK(!empty());
  return nodes_[UsedFirstIndex()].value_;
}

template <typename T, typename Allocator>
const T& VectorBackedLinkedList<T, Allocator>::front() const {
  DCHECK(!empty());
  return nodes_[UsedFirstIndex()].value_;
}

template <typename T, typename Allocator>
T& VectorBackedLinkedList<T, Allocator>::back() {
  DCHECK(!empty());
  return nodes_[UsedLastIndex()].value_;
}

template <typename T, typename Allocator>
const T& VectorBackedLinkedList<T, Allocator>::back() const {
  DCHECK(!empty());
  return nodes_[UsedLastIndex()].value_;
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename VectorBackedLinkedList<T, Allocator>::iterator
VectorBackedLinkedList<T, Allocator>::insert(const_iterator position,
                                             IncomingValueType&& value) {
  wtf_size_t position_index = position.GetIndex();
  wtf_size_t prev_index = nodes_[position_index].prev_index_;

  wtf_size_t new_entry_index;
  if (IsFreeListEmpty()) {
    new_entry_index = nodes_.size();
    nodes_.push_back(Node(prev_index, position_index,
                          std::forward<IncomingValueType>(value)));
  } else {
    new_entry_index = free_head_index_;
    Node& free_head = nodes_[free_head_index_];
    free_head_index_ = free_head.next_index_;
    free_head = Node(prev_index, position_index,
                     std::forward<IncomingValueType>(value));
  }
  nodes_[prev_index].next_index_ = new_entry_index;
  nodes_[position_index].prev_index_ = new_entry_index;
  size_++;
  return MakeIterator(new_entry_index);
}

template <typename T, typename Allocator>
typename VectorBackedLinkedList<T, Allocator>::iterator
VectorBackedLinkedList<T, Allocator>::MoveTo(const_iterator target,
                                             const_iterator new_position) {
  DCHECK(target != end());

  wtf_size_t target_index = target.GetIndex();
  if (target == new_position)
    return MakeIterator(target_index);

  Node& target_node = nodes_[target_index];
  wtf_size_t new_position_index = new_position.GetIndex();
  Node& new_position_node = nodes_[new_position_index];
  wtf_size_t prev_index = new_position_node.prev_index_;

  if (prev_index == target_index)
    return MakeIterator(target_index);

  Unlink(target_node);

  nodes_[prev_index].next_index_ = target_index;
  new_position_node.prev_index_ = target_index;
  target_node.prev_index_ = prev_index;
  target_node.next_index_ = new_position_index;
  return MakeIterator(target_index);
}

template <typename T, typename Allocator>
typename VectorBackedLinkedList<T, Allocator>::iterator
VectorBackedLinkedList<T, Allocator>::erase(const_iterator position) {
  DCHECK(position != end());
  wtf_size_t position_index = position.GetIndex();
  Node& node = nodes_[position_index];
  wtf_size_t next_index = node.next_index_;

  Unlink(node);
  node.value_ = HashTraits<T>::EmptyValue();

  node.next_index_ = free_head_index_;
  node.prev_index_ = kNotFound;
  free_head_index_ = position_index;

  size_--;
  return MakeIterator(next_index);
}

template <typename T, typename Allocator>
void VectorBackedLinkedList<T, Allocator>::Unlink(const Node& node) {
  wtf_size_t prev_index = node.prev_index_;
  wtf_size_t next_index = node.next_index_;

  Node& prev_node = nodes_[prev_index];
  Node& next_node = nodes_[next_index];

  prev_node.next_index_ = next_index;
  next_node.prev_index_ = prev_index;
}

}  // namespace WTF

using WTF::VectorBackedLinkedList;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_VECTOR_BACKED_LINKED_LIST_H_
