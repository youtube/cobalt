// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A utility class for flex layout which given the current node will iterate
// through its children.
//
// This class does not handle modifications to its arguments after it has been
// constructed.
class CORE_EXPORT NGFlexChildIterator {
  STACK_ALLOCATED();

 public:
  NGFlexChildIterator(const NGBlockNode node);

  // Returns the next block node which should be laid out.
  NGBlockNode NextChild() {
    DCHECK(position_ <= children_.size());
    if (position_ == children_.size())
      return nullptr;
    return children_[position_++].child;
  }

  struct ChildWithOrder {
    DISALLOW_NEW();

   public:
    ChildWithOrder(NGBlockNode child, int order) : child(child), order(order) {}
    void Trace(Visitor* visitor) const { visitor->Trace(child); }

    NGBlockNode child;
    int order;
  };

 private:
  // |children_| cannot be modified except in ctor;
  HeapVector<ChildWithOrder, 4> children_;
  wtf_size_t position_ = 0;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(
    blink::NGFlexChildIterator::ChildWithOrder)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_CHILD_ITERATOR_H_
