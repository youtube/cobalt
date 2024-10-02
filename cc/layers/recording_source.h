// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RECORDING_SOURCE_H_
#define CC_LAYERS_RECORDING_SOURCE_H_

#include <stddef.h>

#include "base/memory/ref_counted.h"
#include "cc/base/invalidation_region.h"
#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class DisplayItemList;
class RasterSource;
class Region;

class CC_EXPORT RecordingSource {
 public:
  RecordingSource();
  RecordingSource(const RecordingSource&) = delete;
  virtual ~RecordingSource();

  RecordingSource& operator=(const RecordingSource&) = delete;

  bool UpdateAndExpandInvalidation(Region* invalidation,
                                   const gfx::Size& layer_size,
                                   const gfx::Rect& new_recorded_viewport);
  void UpdateDisplayItemList(const scoped_refptr<DisplayItemList>& display_list,
                             float recording_scale_factor);
  gfx::Size GetSize() const;
  void SetEmptyBounds();
  void SetSlowdownRasterScaleFactor(int factor);
  void SetBackgroundColor(SkColor4f background_color);
  void SetRequiresClear(bool requires_clear);

  void SetNeedsDisplayRect(const gfx::Rect& layer_rect);

  // These functions are virtual for testing.
  virtual scoped_refptr<RasterSource> CreateRasterSource() const;

  bool is_solid_color() const { return is_solid_color_; }

 protected:
  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  int slow_down_raster_scale_factor_for_debug_ = 0;
  bool requires_clear_ = false;
  bool is_solid_color_ = false;
  SkColor4f solid_color_ = SkColors::kTransparent;
  SkColor4f background_color_ = SkColors::kTransparent;
  scoped_refptr<DisplayItemList> display_list_;
  float recording_scale_factor_ = 1.0f;

 private:
  void UpdateInvalidationForNewViewport(const gfx::Rect& old_recorded_viewport,
                                        const gfx::Rect& new_recorded_viewport,
                                        Region* invalidation);

  void FinishDisplayItemListUpdate();

  friend class RasterSource;

  void DetermineIfSolidColor();

  InvalidationRegion invalidation_;
};

}  // namespace cc

#endif  // CC_LAYERS_RECORDING_SOURCE_H_
