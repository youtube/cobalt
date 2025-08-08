// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_

#include "base/dcheck_is_on.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class JSONArray;
class JSONObject;
class PendingLayer;

class PLATFORM_EXPORT ContentLayerClientImpl : public cc::ContentLayerClient {
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl();
  ContentLayerClientImpl(const ContentLayerClientImpl&) = delete;
  ContentLayerClientImpl& operator=(const ContentLayerClientImpl&) = delete;
  ~ContentLayerClientImpl() override;

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() const final {
    return gfx::Rect(gfx::Size(raster_invalidator_.LayerBounds()));
  }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList() final {
    return cc_display_item_list_;
  }
  bool FillsBoundsCompletely() const final { return false; }

  // For LayersAsJSON.
  void AppendAdditionalInfoAsJSON(LayerTreeFlags,
                                  const cc::Layer&,
                                  JSONObject&) const;

  cc::Layer& Layer() const { return *cc_picture_layer_.get(); }

  void UpdateCcPictureLayer(const PendingLayer&);

  RasterInvalidator& GetRasterInvalidator() { return raster_invalidator_; }

  size_t ApproximateUnsharedMemoryUsage() const;

 private:
  // Callback from raster_invalidator_.
  void InvalidateRect(const gfx::Rect&);

  scoped_refptr<cc::PictureLayer> cc_picture_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;
  RasterInvalidator raster_invalidator_;
  RasterInvalidator::RasterInvalidationFunction raster_invalidation_function_;

  String debug_name_;
#if EXPENSIVE_DCHECKS_ARE_ON()
  std::unique_ptr<JSONArray> paint_chunk_debug_data_;
#endif  // EXPENSIVE_DCHECKS_ARE_ON()
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_CONTENT_LAYER_CLIENT_IMPL_H_
