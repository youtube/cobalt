// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/trace_event/traced_value.h"
#include "base/types/optional_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/adjust_mask_layer_geometry.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/pending_layer.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

ContentLayerClientImpl::ContentLayerClientImpl()
    : cc_picture_layer_(cc::PictureLayer::Create(this)),
      raster_invalidation_function_(
          base::BindRepeating(&ContentLayerClientImpl::InvalidateRect,
                              base::Unretained(this))) {}

ContentLayerClientImpl::~ContentLayerClientImpl() {
  cc_picture_layer_->ClearClient();
}

void ContentLayerClientImpl::AppendAdditionalInfoAsJSON(
    LayerTreeFlags flags,
    const cc::Layer& layer,
    JSONObject& json) const {
#if EXPENSIVE_DCHECKS_ARE_ON()
  if (flags & kLayerTreeIncludesDebugInfo)
    json.SetValue("paintChunkContents", paint_chunk_debug_data_->Clone());
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  if ((flags & (kLayerTreeIncludesInvalidations |
                kLayerTreeIncludesDetailedInvalidations)) &&
      raster_invalidator_.GetTracking()) {
    raster_invalidator_.GetTracking()->AsJSON(
        &json, flags & kLayerTreeIncludesDetailedInvalidations);
  }

#if DCHECK_IS_ON()
  if (flags & kLayerTreeIncludesPaintRecords) {
    LoggingCanvas canvas;
    cc_display_item_list_->Raster(&canvas);
    json.SetValue("paintRecord", canvas.Log());
  }
#endif
}

void ContentLayerClientImpl::UpdateCcPictureLayer(
    const PendingLayer& pending_layer) {
  const auto& paint_chunks = pending_layer.Chunks();
#if EXPENSIVE_DCHECKS_ARE_ON()
  paint_chunk_debug_data_ = std::make_unique<JSONArray>();
  for (auto it = paint_chunks.begin(); it != paint_chunks.end(); ++it) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("data", it->ToString(paint_chunks.GetPaintArtifact()));
    json->SetArray("displayItems",
                   DisplayItemList::DisplayItemsAsJSON(
                       paint_chunks.GetPaintArtifact(), it->begin_index,
                       it.DisplayItems(), DisplayItemList::kCompact));
    paint_chunk_debug_data_->PushObject(std::move(json));
  }
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

  auto layer_state = pending_layer.GetPropertyTreeState();
  gfx::Size layer_bounds = pending_layer.LayerBounds();
  gfx::Vector2dF layer_offset = pending_layer.LayerOffset();
  gfx::Size old_layer_bounds = raster_invalidator_.LayerBounds();

  if (layer_state.Effect().BlendMode() == SkBlendMode::kDstIn) {
    AdjustMaskLayerGeometry(pending_layer.GetPropertyTreeState().Transform(),
                            layer_offset, layer_bounds);
  }

  DCHECK_EQ(old_layer_bounds, cc_picture_layer_->bounds());
  raster_invalidator_.Generate(raster_invalidation_function_, paint_chunks,
                               layer_offset, layer_bounds, layer_state);

  absl::optional<RasterUnderInvalidationCheckingParams>
      raster_under_invalidation_params;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    raster_under_invalidation_params.emplace(
        *raster_invalidator_.GetTracking(), gfx::Rect(layer_bounds),
        paint_chunks.GetPaintArtifact().ClientDebugName(
            paint_chunks[0].id.client_id));
  }

  // Note: cc::Layer API assumes the layer bounds start at (0, 0), but the
  // bounding box of a paint chunk does not necessarily start at (0, 0) (and
  // could even be negative). Internally the generated layer translates the
  // paint chunk to align the bounding box to (0, 0) and we set the layer's
  // offset_to_transform_parent with the origin of the paint chunk here.
  cc_picture_layer_->SetOffsetToTransformParent(layer_offset);

  // If nothing changed in the layer, keep the original display item list.
  // Here check layer_bounds because RasterInvalidator doesn't issue raster
  // invalidation when only layer_bounds changes.
  if (cc_display_item_list_ && layer_bounds == old_layer_bounds &&
      cc_picture_layer_->draws_content() == pending_layer.DrawsContent() &&
      !raster_under_invalidation_params) {
    DCHECK_EQ(cc_picture_layer_->bounds(), layer_bounds);
    return;
  }

  cc_display_item_list_ = base::MakeRefCounted<cc::DisplayItemList>();
  PaintChunksToCcLayer::ConvertInto(
      paint_chunks, layer_state, layer_offset,
      base::OptionalToPtr(raster_under_invalidation_params),
      *cc_display_item_list_);
  cc_display_item_list_->Finalize();

  cc_picture_layer_->SetBounds(layer_bounds);
  cc_picture_layer_->SetHitTestable(true);
  cc_picture_layer_->SetIsDrawable(pending_layer.DrawsContent());

  cc_picture_layer_->SetBackgroundColor(pending_layer.ComputeBackgroundColor());
  bool contents_opaque =
      // If the background color is transparent, don't treat the layer as opaque
      // because we won't have a good SafeOpaqueBackgroundColor() to fill the
      // subpixels along the edges in case the layer is not aligned to whole
      // pixels during rasterization.
      cc_picture_layer_->background_color() != SkColors::kTransparent &&
      pending_layer.RectKnownToBeOpaque().Contains(
          gfx::RectF(gfx::PointAtOffsetFromOrigin(pending_layer.LayerOffset()),
                     gfx::SizeF(pending_layer.LayerBounds())));
  cc_picture_layer_->SetContentsOpaque(contents_opaque);
  if (!contents_opaque) {
    cc_picture_layer_->SetContentsOpaqueForText(
        cc_display_item_list_->has_draw_text_ops() &&
        pending_layer.TextKnownToBeOnOpaqueBackground());
  }
}

void ContentLayerClientImpl::InvalidateRect(const gfx::Rect& rect) {
  cc_display_item_list_ = nullptr;
  cc_picture_layer_->SetNeedsDisplayRect(rect);
}

size_t ContentLayerClientImpl::ApproximateUnsharedMemoryUsage() const {
  return sizeof(*this) + raster_invalidator_.ApproximateUnsharedMemoryUsage() -
         sizeof(raster_invalidator_);
}

}  // namespace blink
