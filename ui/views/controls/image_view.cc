// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/image_view.h"

#include <utility>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_flags.h"
#include "skia/ext/image_operations.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/paint_vector_icon.h"

namespace views {

namespace {

// Returns the pixels for the bitmap in |image| at scale |image_scale|.
void* GetBitmapPixels(const gfx::ImageSkia& image, float image_scale) {
  DCHECK_NE(0.0f, image_scale);
  return image.GetRepresentation(image_scale).GetBitmap().getPixels();
}

}  // namespace

ImageView::ImageView() = default;

ImageView::ImageView(const ui::ImageModel& image_model) {
  SetImage(image_model);
}

ImageView::~ImageView() = default;

void ImageView::SetImage(const ui::ImageModel& image_model) {
  if (IsImageEqual(image_model))
    return;

  const gfx::Size pref_size = GetPreferredSize();
  image_model_ = image_model;
  scaled_image_ = gfx::ImageSkia();
  if (pref_size != GetPreferredSize())
    PreferredSizeChanged();
  SchedulePaint();
}

gfx::ImageSkia ImageView::GetImage() const {
  return image_model_.Rasterize(GetColorProvider());
}

ui::ImageModel ImageView::GetImageModel() const {
  return image_model_;
}

bool ImageView::IsImageEqual(const ui::ImageModel& image_model) const {
  if (image_model != image_model_)
    return false;

  // It's not feasible to run the old and new generators and compare output
  // here, so for safety, simply assume the new generator's output differs.
  if (image_model.IsImageGenerator())
    return false;

  if (!image_model.IsImage())
    return true;

  // An ImageModel's Image holds a handle to a backing store, which may have
  // changed since the last call to SetImage(). The expectation is that
  // SetImage() with different pixels is treated as though the image changed.
  // For this reason we compare not only the Image but also the pixels we last
  // painted.
  return last_paint_scale_ != 0.0f &&
         last_painted_bitmap_pixels_ ==
             GetBitmapPixels(image_model.GetImage().AsImageSkia(),
                             last_paint_scale_);
}

gfx::Size ImageView::GetImageSize() const {
  return image_size_.value_or(image_model_.Size());
}

void ImageView::OnPaint(gfx::Canvas* canvas) {
  // This inlines View::OnPaint in order to OnPaintBorder() after OnPaintImage
  // so the border can paint over content (for rounded corners that overlap
  // content).
  TRACE_EVENT1("views", "ImageView::OnPaint", "class", GetClassName());
  OnPaintBackground(canvas);
  OnPaintImage(canvas);
  OnPaintBorder(canvas);
}

void ImageView::OnThemeChanged() {
  View::OnThemeChanged();
  if (image_model_.IsImageGenerator() ||
      (image_model_.IsVectorIcon() &&
       !image_model_.GetVectorIcon().has_color())) {
    scaled_image_ = gfx::ImageSkia();
    SchedulePaint();
  }
}

void ImageView::OnPaintImage(gfx::Canvas* canvas) {
  last_paint_scale_ = canvas->image_scale();
  last_painted_bitmap_pixels_ = nullptr;

  gfx::ImageSkia image = GetPaintImage(last_paint_scale_);
  if (image.isNull())
    return;

  gfx::Rect image_bounds(GetImageBounds());
  if (image_bounds.IsEmpty())
    return;

  if (image_bounds.size() != gfx::Size(image.width(), image.height())) {
    // Resize case
    cc::PaintFlags flags;
    flags.setFilterQuality(cc::PaintFlags::FilterQuality::kLow);
    canvas->DrawImageInt(image, 0, 0, image.width(), image.height(),
                         image_bounds.x(), image_bounds.y(),
                         image_bounds.width(), image_bounds.height(), true,
                         flags);
  } else {
    canvas->DrawImageInt(image, image_bounds.x(), image_bounds.y());
  }
  last_painted_bitmap_pixels_ = GetBitmapPixels(image, last_paint_scale_);
}

gfx::ImageSkia ImageView::GetPaintImage(float scale) {
  if (image_model_.IsEmpty())
    return gfx::ImageSkia();

  if (image_model_.IsImage() || image_model_.IsImageGenerator()) {
    const gfx::ImageSkia image = image_model_.Rasterize(GetColorProvider());
    if (image.isNull())
      return image;

    const gfx::ImageSkiaRep& rep = image.GetRepresentation(scale);
    if (rep.scale() == scale || rep.unscaled())
      return image;

    if (scaled_image_.HasRepresentation(scale))
      return scaled_image_;

    // Only caches one image rep for the current scale.
    scaled_image_ = gfx::ImageSkia();

    gfx::Size scaled_size =
        gfx::ScaleToCeiledSize(rep.pixel_size(), scale / rep.scale());
    scaled_image_.AddRepresentation(gfx::ImageSkiaRep(
        skia::ImageOperations::Resize(
            rep.GetBitmap(), skia::ImageOperations::RESIZE_BEST,
            scaled_size.width(), scaled_size.height()),
        scale));
  } else if (scaled_image_.isNull()) {
    scaled_image_ = image_model_.Rasterize(GetColorProvider());
  }
  return scaled_image_;
}

BEGIN_METADATA(ImageView, ImageViewBase)
END_METADATA

}  // namespace views
