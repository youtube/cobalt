// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CHILD_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CHILD_ITERATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_types.h"

namespace blink {

class NGBlockBreakToken;

// A utility class for table layout which given the first child and a break
// token will iterate through unfinished children.
//
// NextChild() is used to iterate through the children. This will be done in
// child layout order (i.e. in this order: top captions, table header, table
// bodies, table footer, bottom captions). If there are child break tokens,
// though, their nodes will be processed first, in break token order.
class CORE_EXPORT NGTableChildIterator {
  STACK_ALLOCATED();

 public:
  NGTableChildIterator(const NGTableGroupedChildren&, const NGBlockBreakToken*);

  class Entry {
    STACK_ALLOCATED();

   public:
    Entry(NGBlockNode node,
          const NGBlockBreakToken* token,
          wtf_size_t section_index)
        : node(node), token(token), section_index(section_index) {}

    const NGBlockNode GetNode() const { return node; }
    const NGBlockBreakToken* GetBreakToken() const { return token; }
    wtf_size_t GetSectionIndex() const {
      DCHECK(!node.IsTableCaption());
      return section_index;
    }
    explicit operator bool() const { return !!node; }

   private:
    NGBlockNode node;
    const NGBlockBreakToken* token;
    wtf_size_t section_index;
  };

  // Returns the next node which should be laid out, along with its
  // respective break token.
  Entry NextChild();

 private:
  NGBlockNode CurrentChild() const;
  void AdvanceChild();

  const NGTableGroupedChildren* grouped_children_;
  const NGBlockBreakToken* break_token_;

  // The sections iterator is used to walk through the table sections in layout
  // order, i.e. table header, table bodies, table footer. If it is unset, it
  // means that we're processing top captions. If it's at end(), it means that
  // we should look for bottom captions.
  absl::optional<NGTableGroupedChildrenIterator> section_iterator_;

  // An index into break_token_'s ChildBreakTokens() vector. Used for keeping
  // track of the next child break token to inspect.
  wtf_size_t child_token_idx_ = 0;

  // An index into the current table caption. We're walking through the captions
  // twice. First we look for top captions. Then we walk through the sections
  // iterator. Then we walk through the captions again, looking for bottom
  // captions.
  wtf_size_t caption_idx_ = 0;

  wtf_size_t section_idx_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_TABLE_NG_TABLE_CHILD_ITERATOR_H_
