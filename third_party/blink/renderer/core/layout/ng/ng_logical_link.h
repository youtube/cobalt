// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LOGICAL_LINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LOGICAL_LINK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class NGPhysicalFragment;

// Similar to |NGLink| but with |LogicalOffset| instead of |PhysicalOffset|.
struct CORE_EXPORT NGLogicalLink {
  DISALLOW_NEW();

 public:
  const LogicalOffset& Offset() const { return offset; }
  const NGPhysicalFragment* get() const { return fragment; }

  explicit operator bool() const { return fragment; }
  const NGPhysicalFragment& operator*() const { return *fragment; }
  const NGPhysicalFragment* operator->() const { return fragment; }

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }

  Member<const NGPhysicalFragment> fragment;
  LogicalOffset offset;
};

using NGLogicalLinkVector = HeapVector<NGLogicalLink, 4>;

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGLogicalLink)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LOGICAL_LINK_H_
