// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DEFAULT_ANCHOR_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DEFAULT_ANCHOR_DATA_H_

#include "third_party/blink/renderer/core/style/position_area.h"
#include "third_party/blink/renderer/core/style/style_position_anchor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Encapsulates the information needed to resolve the default anchor.
class DefaultAnchorData {
  STACK_ALLOCATED();

 public:
  DefaultAnchorData() = default;
  DefaultAnchorData(const StylePositionAnchor& position_anchor,
                    PositionArea position_area)
      : position_anchor_(position_anchor), position_area_(position_area) {}

  using Type = StylePositionAnchor::Type;

  // Coerces the "normal" keyword to either "none" or "auto".
  Type GetType() const {
    Type type = position_anchor_.GetType();
    if (type == Type::kNormal) {
      return position_area_.IsNone() ? Type::kNone : Type::kAuto;
    }
    return type;
  }

  const ScopedCSSName& GetName() const { return position_anchor_.GetName(); }
  const PositionArea& GetPositionArea() const { return position_area_; }

  bool operator==(const DefaultAnchorData& other) const {
    return position_anchor_ == other.position_anchor_ &&
           position_area_ == other.position_area_;
  }

 private:
  StylePositionAnchor position_anchor_ = StylePositionAnchor::Initial();
  PositionArea position_area_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DEFAULT_ANCHOR_DATA_H_
