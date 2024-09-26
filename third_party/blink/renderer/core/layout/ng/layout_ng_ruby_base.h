// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"

namespace blink {

// Represents a ruby base box.
// https://drafts.csswg.org/css-ruby-1/#ruby-base-box.
class CORE_EXPORT LayoutNGRubyBase final : public LayoutNGBlockFlow {
 public:
  explicit LayoutNGRubyBase();
  ~LayoutNGRubyBase() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutNGRubyBase";
  }
  bool IsOfType(LayoutObjectType type) const override;
  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  // This function removes all children that are before (!) `before_child`
  // and appends them to `to_base`.
  void MoveChildren(LayoutNGRubyBase& to_base,
                    LayoutObject* before_child = nullptr);

 private:
  void MoveInlineChildrenTo(LayoutNGRubyBase& to_base,
                            LayoutObject* before_child);
  void MoveBlockChildrenTo(LayoutNGRubyBase& to_base,
                           LayoutObject* before_child);
};

template <>
struct DowncastTraits<LayoutNGRubyBase> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsRubyBase();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_LAYOUT_NG_RUBY_BASE_H_
