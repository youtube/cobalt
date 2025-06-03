// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection/composited_touch_handle_drawable.h"

#include <algorithm>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "cc/slim/ui_resource_layer.h"
#include "content/public/browser/android/compositor.h"
#include "ui/android/handle_view_resources.h"
#include "ui/android/view_android.h"

using base::android::JavaRef;

namespace content {

namespace {

ui::HandleViewResources& GetSelectionResources() {
  static base::NoDestructor<ui::HandleViewResources> selection_resources;
  return *selection_resources;
}

}  // namespace

CompositedTouchHandleDrawable::CompositedTouchHandleDrawable(
    gfx::NativeView parent_native_view,
    cc::slim::Layer* parent_layer,
    const JavaRef<jobject>& context)
    : orientation_(ui::TouchHandleOrientation::UNDEFINED),
      layer_(cc::slim::UIResourceLayer::Create()) {
  CHECK(parent_native_view);
  CHECK(parent_layer);

  view_ = parent_native_view;
  GetSelectionResources().LoadIfNecessary(context);
  drawable_horizontal_padding_ratio_ =
      GetSelectionResources().GetDrawableHorizontalPaddingRatio();
  parent_layer->AddChild(layer_.get());
}

CompositedTouchHandleDrawable::~CompositedTouchHandleDrawable() {
  DetachLayer();
}

void CompositedTouchHandleDrawable::SetEnabled(bool enabled) {
  layer_->SetIsDrawable(enabled);
  // Force a position update in case the disabled layer's properties are stale.
  if (enabled)
    UpdateLayerPosition();
}

void CompositedTouchHandleDrawable::SetOrientation(
    ui::TouchHandleOrientation orientation,
    bool mirror_vertical,
    bool mirror_horizontal) {
  DCHECK(layer_->parent());
  bool orientation_changed = orientation_ != orientation;

  orientation_ = orientation;

  if (orientation_changed) {
    const SkBitmap& bitmap = GetSelectionResources().GetBitmap(orientation);
    const int bitmap_height = bitmap.height();
    const int bitmap_width = bitmap.width();
    layer_->SetBitmap(bitmap);
    layer_->SetBounds(gfx::Size(bitmap_width, bitmap_height));
  }

  const int layer_height = layer_->bounds().height();
  const int layer_width = layer_->bounds().width();

  // Invert about X and Y axis based on the mirror values
  gfx::Transform transform;
  float scale_x = mirror_horizontal ? -1.f : 1.f;
  float scale_y = mirror_vertical ? -1.f : 1.f;

  layer_->SetTransformOrigin(
      gfx::Point3F(layer_width * 0.5f, layer_height * 0.5f, 0));
  transform.Scale(scale_x, scale_y);
  layer_->SetTransform(transform);
}

void CompositedTouchHandleDrawable::SetOrigin(const gfx::PointF& origin) {
  origin_position_ = origin;
  UpdateLayerPosition();
}

void CompositedTouchHandleDrawable::SetAlpha(float alpha) {
  DCHECK(layer_->parent());
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  layer_->SetOpacity(alpha);
  layer_->SetHideLayerAndSubtree(!alpha);
}

gfx::RectF CompositedTouchHandleDrawable::GetVisibleBounds() const {
  return gfx::ScaleRect(
      gfx::RectF(layer_->position().x(), layer_->position().y(),
                 layer_->bounds().width(), layer_->bounds().height()),
      1.f / view_->GetDipScale());
}

float CompositedTouchHandleDrawable::GetDrawableHorizontalPaddingRatio() const {
  return drawable_horizontal_padding_ratio_;
}

void CompositedTouchHandleDrawable::DetachLayer() {
  layer_->RemoveFromParent();
}

void CompositedTouchHandleDrawable::UpdateLayerPosition() {
  layer_->SetPosition(gfx::ScalePoint(origin_position_, view_->GetDipScale()));
}

}  // namespace content
