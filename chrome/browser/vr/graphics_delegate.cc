// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate.h"

#include <vector>

#include "base/check.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/fov_rectangle.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;

CameraModel CameraModelViewProjFromXRView(
    const device::mojom::XRViewPtr& view) {
  CameraModel model = {};

  // TODO(https://crbug.com/1070380): mojo space is currently equivalent to
  // world space, so the view matrix is world_from_view.
  model.view_matrix = view->mojo_from_view;

  bool is_invertible = model.view_matrix.GetInverse(&model.view_matrix);
  DCHECK(is_invertible);

  float up_tan = tanf(view->field_of_view->up_degrees * base::kPiFloat / 180.0);
  float left_tan =
      tanf(view->field_of_view->left_degrees * base::kPiFloat / 180.0);
  float right_tan =
      tanf(view->field_of_view->right_degrees * base::kPiFloat / 180.0);
  float down_tan =
      tanf(view->field_of_view->down_degrees * base::kPiFloat / 180.0);
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  // clang-format off
  gfx::Transform proj_matrix = gfx::Transform::RowMajor(
      x_scale, 0, -((left_tan - right_tan) * x_scale * 0.5), 0,
      0, y_scale, ((up_tan - down_tan) * y_scale * 0.5), 0,
      0, 0, (kZFar + kZNear) / (kZNear - kZFar),
          2 * kZFar * kZNear / (kZNear - kZFar),
      0, 0, -1, 0);
  // clang-format on
  model.view_proj_matrix = proj_matrix * model.view_matrix;
  return model;
}

}  // namespace

GraphicsDelegate::GraphicsDelegate() = default;
GraphicsDelegate::~GraphicsDelegate() = default;

float GraphicsDelegate::GetZNear() {
  return kZNear;
}

float GraphicsDelegate::GetZFar() {
  return kZFar;
}

void GraphicsDelegate::SetXrViews(
    const std::vector<device::mojom::XRViewPtr>& views) {
  // Store the first left and right views.
  for (auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft) {
      left_ = view.Clone();
    } else if (view->eye == device::mojom::XREye::kRight) {
      right_ = view.Clone();
    }
  }

  DCHECK(left_);
  DCHECK(right_);
}

gfx::RectF GraphicsDelegate::GetLeft() {
  gfx::Size size = GetTextureSize();
  return gfx::RectF(
      0, 0, static_cast<float>(left_->viewport.width()) / size.width(),
      static_cast<float>(left_->viewport.height()) / size.height());
}

gfx::RectF GraphicsDelegate::GetRight() {
  gfx::Size size = GetTextureSize();
  return gfx::RectF(
      static_cast<float>(left_->viewport.width()) / size.width(), 0,
      static_cast<float>(right_->viewport.width()) / size.width(),
      static_cast<float>(right_->viewport.height()) / size.height());
}

FovRectangles GraphicsDelegate::GetRecommendedFovs() {
  DCHECK(left_);
  DCHECK(right_);
  FovRectangle left = {
      left_->field_of_view->left_degrees,
      left_->field_of_view->right_degrees,
      left_->field_of_view->down_degrees,
      left_->field_of_view->up_degrees,
  };

  FovRectangle right = {
      right_->field_of_view->left_degrees,
      right_->field_of_view->right_degrees,
      right_->field_of_view->down_degrees,
      right_->field_of_view->up_degrees,
  };

  return std::pair<FovRectangle, FovRectangle>(left, right);
}

RenderInfo GraphicsDelegate::GetRenderInfo(FrameType frame_type,
                                           const gfx::Transform& head_pose) {
  RenderInfo info;
  info.head_pose = head_pose;

  CameraModel left = CameraModelViewProjFromXRView(left_);
  left.eye_type = kLeftEye;
  left.viewport =
      gfx::Rect(0, 0, left_->viewport.width(), left_->viewport.height());
  info.left_eye_model = left;

  CameraModel right = CameraModelViewProjFromXRView(right_);
  right.eye_type = kRightEye;
  right.viewport =
      gfx::Rect(left_->viewport.width(), 0, right_->viewport.width(),
                right_->viewport.height());
  info.right_eye_model = right;
  cached_info_ = info;
  return info;
}

RenderInfo GraphicsDelegate::GetOptimizedRenderInfoForFovs(
    const FovRectangles& fovs) {
  RenderInfo info = cached_info_;
  // TODO(billorr): consider optimizing overlays to save texture size.
  // For now, we use a full-size texture when we could get by with less.
  return info;
}

void GraphicsDelegate::PrepareBufferForWebXr() {
  // Desktop doesn't render WebXR through the browser renderer.
  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kWebXr;
}

void GraphicsDelegate::PrepareBufferForWebXrOverlayElements() {
  // No-op.  We reuse the same buffer for overlays and other content, which is
  // initialized in PreRender.
  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kWebXrOverlayElements;
}

void GraphicsDelegate::PrepareBufferForBrowserUi() {
  ClearBufferToBlack();
  DCHECK(prepared_drawing_buffer_ == DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kBrowserUi;
}

void GraphicsDelegate::OnFinishedDrawingBuffer() {
  DCHECK(prepared_drawing_buffer_ != DrawingBufferMode::kNone);
  prepared_drawing_buffer_ = DrawingBufferMode::kNone;
}

void GraphicsDelegate::GetWebXrDrawParams(int* texture_id,
                                          Transform* uv_transform) {
  // Reporting a texture_id of 0 will skip texture copies.
  *texture_id = 0;
}

bool GraphicsDelegate::RunInSkiaContext(base::OnceClosure callback) {
  // TODO(billorr): Support multiple contexts in a share group.  For now just
  // share one context.
  std::move(callback).Run();
  return true;
}

gfx::Size GraphicsDelegate::GetTextureSize() {
  int width = left_->viewport.width() + right_->viewport.width();
  int height = std::max(left_->viewport.height(), right_->viewport.height());

  return gfx::Size(width, height);
}

}  // namespace vr
