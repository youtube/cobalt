/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_SHAPE_OUTSIDE_INFO_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_SHAPE_OUTSIDE_INFO_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/layout/shapes/shape.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class LayoutBox;
struct PhysicalRect;

class ShapeOutsideDeltas final {
  DISALLOW_NEW();

 public:
  ShapeOutsideDeltas() : line_overlaps_shape_(false), is_valid_(false) {}

  ShapeOutsideDeltas(LayoutUnit left_margin_box_delta,
                     LayoutUnit right_margin_box_delta,
                     bool line_overlaps_shape,
                     LayoutUnit border_box_line_top,
                     LayoutUnit line_height)
      : left_margin_box_delta_(left_margin_box_delta),
        right_margin_box_delta_(right_margin_box_delta),
        border_box_line_top_(border_box_line_top),
        line_height_(line_height),
        line_overlaps_shape_(line_overlaps_shape),
        is_valid_(true) {}

  bool IsForLine(LayoutUnit border_box_line_top, LayoutUnit line_height) {
    return is_valid_ && border_box_line_top_ == border_box_line_top &&
           line_height_ == line_height;
  }

  bool IsValid() { return is_valid_; }
  LayoutUnit LeftMarginBoxDelta() {
    DCHECK(is_valid_);
    return left_margin_box_delta_;
  }
  LayoutUnit RightMarginBoxDelta() {
    DCHECK(is_valid_);
    return right_margin_box_delta_;
  }
  bool LineOverlapsShape() {
    DCHECK(is_valid_);
    return line_overlaps_shape_;
  }

 private:
  LayoutUnit left_margin_box_delta_;
  LayoutUnit right_margin_box_delta_;
  LayoutUnit border_box_line_top_;
  LayoutUnit line_height_;
  bool line_overlaps_shape_ : 1;
  bool is_valid_ : 1;
};

class ShapeOutsideInfo final : public GarbageCollected<ShapeOutsideInfo> {
 public:
  explicit ShapeOutsideInfo(const LayoutBox& layout_box)
      : layout_box_(&layout_box), is_computing_shape_(false) {}

  void SetReferenceBoxLogicalSize(LayoutSize);
  void SetPercentageResolutionInlineSize(LayoutUnit);

  LayoutUnit ShapeLogicalBottom() const {
    return ComputedShape().ShapeMarginLogicalBoundingBox().MaxY() +
           LogicalTopOffset();
  }

  static ShapeOutsideInfo& EnsureInfo(const LayoutBox& key) {
    InfoMap& info_map = ShapeOutsideInfo::GetInfoMap();
    auto it = info_map.find(&key);
    if (it != info_map.end())
      return *it->value;
    InfoMap::AddResult result =
        info_map.insert(&key, MakeGarbageCollected<ShapeOutsideInfo>(key));
    return *result.stored_value->value;
  }
  static void RemoveInfo(const LayoutBox& key) { GetInfoMap().erase(&key); }
  static ShapeOutsideInfo* Info(const LayoutBox& key) {
    if (!IsEnabledFor(key))
      return nullptr;
    return GetInfoMap().at(&key);
  }

  void MarkShapeAsDirty() { shape_.reset(); }
  bool IsShapeDirty() { return !shape_.get(); }
  bool IsComputingShape() const { return is_computing_shape_; }

  PhysicalRect ComputedShapePhysicalBoundingBox() const;
  gfx::PointF ShapeToLayoutObjectPoint(gfx::PointF) const;
  const Shape& ComputedShape() const;

  void Trace(Visitor* visitor) const;

 private:
  static bool IsEnabledFor(const LayoutBox&);

  std::unique_ptr<Shape> CreateShapeForImage(StyleImage*,
                                             float shape_image_threshold,
                                             WritingMode,
                                             float margin) const;

  LayoutUnit LogicalTopOffset() const;
  LayoutUnit LogicalLeftOffset() const;

  using InfoMap =
      HeapHashMap<WeakMember<const LayoutBox>, Member<ShapeOutsideInfo>>;
  static InfoMap& GetInfoMap();

  const Member<const LayoutBox> layout_box_;
  mutable std::unique_ptr<Shape> shape_;
  LayoutSize reference_box_logical_size_;
  LayoutUnit percentage_resolution_inline_size_;
  ShapeOutsideDeltas shape_outside_deltas_;
  mutable bool is_computing_shape_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_SHAPE_OUTSIDE_INFO_H_
