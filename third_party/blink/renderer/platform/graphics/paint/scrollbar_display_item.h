// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_

#include "base/dcheck_is_on.h"
#include "cc/input/scrollbar.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace cc {
class ScrollbarLayerBase;
}

namespace blink {

class DisplayItemClient;
class GraphicsContext;
class TransformPaintPropertyNode;

// During paint, we create a ScrollbarDisplayItem for a non-custom scrollbar.
// During PaintArtifactCompositor::Update(), we decide whether to composite the
// scrollbar and, if not composited, call Paint() to actually paint the
// scrollbar into a paint record, otherwise call CreateLayer() to create a cc
// scrollbar layer.
class PLATFORM_EXPORT ScrollbarDisplayItem final : public DisplayItem {
 public:
  ScrollbarDisplayItem(
      DisplayItemClientId,
      Type,
      scoped_refptr<cc::Scrollbar>,
      const gfx::Rect& visual_rect,
      scoped_refptr<const TransformPaintPropertyNode> scroll_translation,
      CompositorElementId element_id,
      RasterEffectOutset outset,
      PaintInvalidationReason paint_invalidation_reason =
          PaintInvalidationReason::kJustCreated);

  const TransformPaintPropertyNode* ScrollTranslation() const {
    DCHECK(!IsTombstone());
    return data_->scroll_translation_.get();
  }
  CompositorElementId ElementId() const {
    DCHECK(!IsTombstone());
    return data_->element_id_;
  }

  // Paints the scrollbar into the internal paint record, for non-composited
  // scrollbar.
  PaintRecord Paint() const;

  bool NeedsUpdateDisplay() const;

  // Create or reuse the cc scrollbar layer, for composited scrollbar.
  scoped_refptr<cc::ScrollbarLayerBase> CreateOrReuseLayer(
      cc::ScrollbarLayerBase* existing_layer) const;

  // Records a scrollbar into a GraphicsContext. Must check
  // PaintController::UseCachedItem() before calling this function.
  // |rect| is the bounding box of the scrollbar in the current transform space.
  static void Record(
      GraphicsContext&,
      const DisplayItemClient&,
      DisplayItem::Type,
      scoped_refptr<cc::Scrollbar>,
      const gfx::Rect& visual_rect,
      scoped_refptr<const TransformPaintPropertyNode> scroll_translation,
      CompositorElementId element_id);

  bool IsOpaque() const;

 private:
  friend class DisplayItem;
  bool EqualsForUnderInvalidationImpl(const ScrollbarDisplayItem&) const;
#if DCHECK_IS_ON()
  void PropertiesAsJSONImpl(JSONObject&) const;
#endif

  struct Data {
    scoped_refptr<cc::Scrollbar> scrollbar_;
    scoped_refptr<const TransformPaintPropertyNode> scroll_translation_;
    CompositorElementId element_id_;
    // This is lazily created for non-composited scrollbar.
    mutable PaintRecord record_;

    USING_FAST_MALLOC(Data);
  };
  // This is to make ScrollbarDisplayItem not bigger than other DisplayItems,
  // so that we can store different types of DisplayItems in DisplayItemList
  // with fixed item size without big gaps. The unique_ptr indirection won't
  // affect performance much because ScrollbarDisplayItems are rare in the
  // painted result.
  std::unique_ptr<Data> data_;
};

template <>
struct DowncastTraits<ScrollbarDisplayItem> {
  static bool AllowFrom(const DisplayItem& i) {
    return !i.IsTombstone() && i.IsScrollbar();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_
