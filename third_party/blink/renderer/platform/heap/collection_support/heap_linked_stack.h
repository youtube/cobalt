/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_STACK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_STACK_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

// HeapLinkedStack<> is an Oilpan-managed stack that avoids pre-allocation
// of memory and heap fragmentation.
//
// The API was originally implemented on the call stack by LinkedStack<>
// (now removed: https://codereview.chromium.org/2761853003/).
// See https://codereview.chromium.org/17314010 for the original use-case.
template <typename T>
class HeapLinkedStack final : public GarbageCollected<HeapLinkedStack<T>> {
 public:
  HeapLinkedStack() { CheckType(); }

  inline wtf_size_t size() const;
  inline bool IsEmpty() const;

  inline void Push(const T&);
  inline const T& Peek() const;
  inline void Pop();

  void Trace(Visitor* visitor) const {
    visitor->Trace(head_);
  }

 private:
  class Node final : public GarbageCollected<Node> {
   public:
    Node(const T&, Node*);

    void Trace(Visitor* visitor) const {
      visitor->Trace(data_);
      visitor->Trace(next_);
    }

    T data_;
    Member<Node> next_;
  };

  static void CheckType() {
    static_assert(WTF::IsMemberType<T>::value,
                  "HeapLinkedStack supports only Member.");
  }

  Member<Node> head_;
  wtf_size_t size_ = 0;
};

template <typename T>
HeapLinkedStack<T>::Node::Node(const T& data, Node* next)
    : data_(data), next_(next) {}

template <typename T>
bool HeapLinkedStack<T>::IsEmpty() const {
  return !head_;
}

template <typename T>
void HeapLinkedStack<T>::Push(const T& data) {
  head_ = MakeGarbageCollected<Node>(data, head_);
  ++size_;
}

template <typename T>
const T& HeapLinkedStack<T>::Peek() const {
  return head_->data_;
}

template <typename T>
void HeapLinkedStack<T>::Pop() {
  DCHECK(head_);
  DCHECK(size_);
  head_ = head_->next_;
  --size_;
}

template <typename T>
wtf_size_t HeapLinkedStack<T>::size() const {
  return size_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_STACK_H_
