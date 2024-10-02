// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"

namespace blink {

// A break token is a continuation token for layout. A single layout input node
// can have multiple fragments asssociated with it.
//
// Each fragment whose node needs to resume layout in a future fragmentainer
// (column, line, etc.) will have a break token associated with it.
//
// See CSS Fragmentation (https://drafts.csswg.org/css-break/) for a detailed
// description of different types of breaks which can occur in CSS.
//
// Each layout algorithm which can fragment, e.g. block-flow can optionally
// accept a break token. For example:
//
// NGLayoutInputNode* node = ...;
// NGPhysicalFragment* fragment = node->Layout(space);
// NGPhysicalFragment* fragment2 = node->Layout(space, fragment->BreakToken());
//
// The break token should encapsulate enough information to "resume" the layout.
class CORE_EXPORT NGBreakToken : public GarbageCollected<NGBreakToken> {
 public:
  enum NGBreakTokenType {
    kBlockBreakToken = NGLayoutInputNode::kBlock,
    kInlineBreakToken = NGLayoutInputNode::kInline
  };
  NGBreakTokenType Type() const { return static_cast<NGBreakTokenType>(type_); }

  bool IsBlockType() const { return Type() == kBlockBreakToken; }
  bool IsInlineType() const { return Type() == kInlineBreakToken; }

  // Returns the node associated with this break token. A break token cannot be
  // used with any other node.
  NGLayoutInputNode InputNode() const {
    return NGLayoutInputNode::Create(
        box_, static_cast<NGLayoutInputNode::NGLayoutInputNodeType>(type_));
  }

#if DCHECK_IS_ON()
  String ToString() const;
  void ShowBreakTokenTree() const;
#endif

  void Trace(Visitor*) const;
  void TraceAfterDispatch(Visitor*) const;

 protected:
  NGBreakToken(NGBreakTokenType type,
               NGLayoutInputNode node,
               unsigned flags = 0)
      : box_(node.GetLayoutBox()),
        type_(type),
#if DCHECK_IS_ON()
        is_repeated_actual_break_(false),
#endif
        flags_(flags),
        is_break_before_(false),
        is_forced_break_(false),
        is_repeated_(false),
        is_caused_by_column_spanner_(false),
        is_at_block_end_(false),
        has_seen_all_children_(false) {
    DCHECK_EQ(type, static_cast<NGBreakTokenType>(node.Type()));
  }

 private:
  // Because |NGLayoutInputNode| has a pointer and 1 bit flag, and it's fast to
  // re-construct, keep |LayoutBox| to save the memory consumed by alignment.
  Member<LayoutBox> box_;

  unsigned type_ : 1;

 protected:
#if DCHECK_IS_ON()
  // If true, this is a break token for an actual break in a cloned fragment. In
  // such cases, only a few of the members here have been set up correctly, and
  // the rest should therefore not be accessed. Such break tokens are never used
  // in layout, only by pre-paint / paint.
  unsigned is_repeated_actual_break_ : 1;
#endif

  // The following bitfields are only to be used by NGInlineBreakToken (it's
  // defined here to save memory, since that class has no bitfields).

  const unsigned flags_ : 4;  // NGInlineBreakTokenFlags

  // The following bitfields are only to be used by NGBlockBreakToken (it's
  // defined here to save memory, since that class has no bitfields).

  unsigned is_break_before_ : 1;

  unsigned is_forced_break_ : 1;

  unsigned is_repeated_ : 1;

  unsigned is_caused_by_column_spanner_ : 1;

  // Set when layout is past the block-end border edge. If we break when we're
  // in this state, it means that something is overflowing, and thus establishes
  // a parallel flow.
  unsigned is_at_block_end_ : 1;

  // All children of this container have been "seen" at this point. This means
  // that all children have been fully laid out, or have break tokens. No more
  // children left to discover.
  unsigned has_seen_all_children_ : 1;

  // See |NGBlockBreakToken::HasUnpositionedListMarker|.
  unsigned has_unpositioned_list_marker_ : 1;
};

typedef HeapVector<Member<const NGBreakToken>> NGBreakTokenVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BREAK_TOKEN_H_
