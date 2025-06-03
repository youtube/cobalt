// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include <utility>

#include "ash/display/output_protection_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/desks/desks_util.h"
#include "base/containers/adapters.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/exo/buffer.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "components/exo/window_properties.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "media/media_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/class_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(exo::Surface*)

namespace exo {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSurfaceHasAugmentedSurfaceKey, false)

namespace {

// A property key containing the surface that is associated with
// window. If unset, no surface is associated with window.
DEFINE_UI_CLASS_PROPERTY_KEY(Surface*, kSurfaceKey, nullptr)

// A property key to store whether the surface should only consume
// stylus input events.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kStylusOnlyKey, false)

// Helper function that returns an iterator to the first entry in |list|
// with |key|.
template <typename T, typename U>
typename T::iterator FindListEntry(T& list, U key) {
  return base::ranges::find(list, key, &T::value_type::first);
}

// Helper function that returns true if |list| contains an entry with |key|.
template <typename T, typename U>
bool ListContainsEntry(T& list, U key) {
  return FindListEntry(list, key) != list.end();
}

// Helper function that returns true if |format| may have an alpha channel.
// Note: False positives are allowed but false negatives are not.
bool FormatHasAlpha(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return false;
    default:
      return true;
  }
}

Transform InvertY(Transform transform) {
  switch (transform) {
    case Transform::NORMAL:
      return Transform::FLIPPED_ROTATE_180;
    case Transform::ROTATE_90:
      return Transform::FLIPPED_ROTATE_270;
    case Transform::ROTATE_180:
      return Transform::FLIPPED;
    case Transform::ROTATE_270:
      return Transform::FLIPPED_ROTATE_90;
    case Transform::FLIPPED:
      return Transform::ROTATE_180;
    case Transform::FLIPPED_ROTATE_90:
      return Transform::ROTATE_270;
    case Transform::FLIPPED_ROTATE_180:
      return Transform::NORMAL;
    case Transform::FLIPPED_ROTATE_270:
      return Transform::ROTATE_90;
  }
  NOTREACHED();
}

// Returns a gfx::Transform that can transform a (0,0 1x1) rect to the same
// rect while rotate/flip the contents about (0.5, 0.5) origin. It's equivalent
// to rotating/flipping about (0, 0) origin then translating by
// (0 or 1, 0 or 1). Note that the rotations are counter-clockwise.
gfx::Transform ToBufferTransformMatrix(Transform transform, bool invert_y) {
  switch (invert_y ? InvertY(transform) : transform) {
    case Transform::NORMAL:
      return gfx::Transform();
    case Transform::ROTATE_90:
      return gfx::Transform::Affine(0, -1, 1, 0, 0, 1);
    case Transform::ROTATE_180:
      return gfx::Transform::Affine(-1, 0, 0, -1, 1, 1);
    case Transform::ROTATE_270:
      return gfx::Transform::Affine(0, 1, -1, 0, 1, 0);
    case Transform::FLIPPED:
      return gfx::Transform::Affine(-1, 0, 0, 1, 1, 0);
    case Transform::FLIPPED_ROTATE_90:
      return gfx::Transform::Affine(0, 1, 1, 0, 0, 0);
    case Transform::FLIPPED_ROTATE_180:
      return gfx::Transform::Affine(1, 0, 0, -1, 0, 1);
    case Transform::FLIPPED_ROTATE_270:
      return gfx::Transform::Affine(0, -1, -1, 0, 1, 1);
  }
  NOTREACHED();
}

// Helper function that returns |size| after adjusting for |transform|.
gfx::Size ToTransformedSize(const gfx::Size& size, Transform transform) {
  switch (transform) {
    case Transform::NORMAL:
    case Transform::ROTATE_180:
    case Transform::FLIPPED:
    case Transform::FLIPPED_ROTATE_180:
      return size;
    case Transform::ROTATE_90:
    case Transform::ROTATE_270:
    case Transform::FLIPPED_ROTATE_90:
    case Transform::FLIPPED_ROTATE_270:
      return gfx::Size(size.height(), size.width());
  }

  NOTREACHED();
}

bool IsDeskContainer(aura::Window* container) {
  return ash::desks_util::IsDeskContainer(container);
}

class CustomWindowDelegate : public aura::WindowDelegate {
 public:
  explicit CustomWindowDelegate(Surface* surface) : surface_(surface) {}

  CustomWindowDelegate(const CustomWindowDelegate&) = delete;
  CustomWindowDelegate& operator=(const CustomWindowDelegate&) = delete;

  ~CustomWindowDelegate() override {}

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget)
      return widget->GetNativeWindow()->GetCursor(point /* not used */);
    return ui::mojom::CursorType::kNull;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget && IsDeskContainer(widget->GetNativeView()->parent()) &&
        surface_->HitTest(point)) {
      return HTCLIENT;
    }

    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    surface_->OnScaleFactorChanged(old_device_scale_factor,
                                   new_device_scale_factor);
  }
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  void OnWindowOcclusionChanged(
      aura::Window::OcclusionState old_occlusion_state,
      aura::Window::OcclusionState new_occlusion_state) override {
    surface_->OnWindowOcclusionChanged(old_occlusion_state,
                                       new_occlusion_state);
  }
  bool HasHitTestMask() const override { return true; }
  void GetHitTestMask(SkPath* mask) const override {
    surface_->GetHitTestMask(mask);
  }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Propagates the key event upto the top-level views Widget so that we can
    // trigger proper events in the views/ash level there. Event handling for
    // Surfaces is done in a post event handler in keyboard.cc.
    views::Widget* widget =
        views::Widget::GetTopLevelWidgetForNativeView(surface_->window());
    if (widget)
      widget->OnKeyEvent(event);
  }

 private:
  const raw_ptr<Surface, ExperimentalAsh> surface_;
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter() {}

  CustomWindowTargeter(const CustomWindowTargeter&) = delete;
  CustomWindowTargeter& operator=(const CustomWindowTargeter&) = delete;

  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = Surface::AsSurface(window);
    if (!surface || !surface->IsInputEnabled(surface))
      return false;

    gfx::Point local_point =
        ConvertEventLocationToWindowCoordinates(window, event);
    return surface->HitTest(local_point);
  }
};

const std::string& GetApplicationId(aura::Window* window) {
  static const std::string empty_app_id;
  if (!window)
    return empty_app_id;
  while (window) {
    const std::string* app_id = exo::GetShellApplicationId(window);
    if (app_id)
      return *app_id;
    window = window->parent();
  }
  return empty_app_id;
}

int surface_id = 0;

void ImmediateExplicitRelease(
    Buffer::PerCommitExplicitReleaseCallback callback) {
  if (callback)
    std::move(callback).Run(/*release_fence=*/gfx::GpuFenceHandle());
}

}  // namespace

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kClientSurfaceIdKey, nullptr)

// A property key to store the window session Id set by client or full_restore
// component.
DEFINE_UI_CLASS_PROPERTY_KEY(int32_t, kWindowSessionId, -1)

ScopedSurface::ScopedSurface(Surface* surface, SurfaceObserver* observer)
    : surface_(surface), observer_(observer) {
  surface_->AddSurfaceObserver(observer_);
}

ScopedSurface::~ScopedSurface() {
  surface_->RemoveSurfaceObserver(observer_);
}

////////////////////////////////////////////////////////////////////////////////
// Surface, public:

Surface::Surface()
    : window_(
          std::make_unique<aura::Window>(new CustomWindowDelegate(this),
                                         aura::client::WINDOW_TYPE_CONTROL)) {
  window_->SetName(base::StringPrintf("ExoSurface-%d", surface_id++));
  window_->SetProperty(kSurfaceKey, this);
  window_->Init(ui::LAYER_NOT_DRAWN);
  window_->SetEventTargeter(std::make_unique<CustomWindowTargeter>());
  window_->set_owned_by_parent(false);
  WMHelper::GetInstance()->SetDragDropDelegate(window_.get());
}
Surface::~Surface() {
  for (SurfaceObserver& observer : observers_)
    observer.OnSurfaceDestroying(this);

  // Call all frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  state_.frame_callbacks.splice(state_.frame_callbacks.end(),
                                cached_state_.frame_callbacks);
  state_.frame_callbacks.splice(state_.frame_callbacks.end(),
                                pending_state_.frame_callbacks);
  for (const auto& frame_callback : state_.frame_callbacks)
    frame_callback.Run(base::TimeTicks());

  // Call all presentation callbacks with a null presentation time to indicate
  // that they have been cancelled.
  state_.presentation_callbacks.splice(state_.presentation_callbacks.end(),
                                       cached_state_.presentation_callbacks);
  state_.presentation_callbacks.splice(state_.presentation_callbacks.end(),
                                       pending_state_.presentation_callbacks);
  for (const auto& presentation_callback : state_.presentation_callbacks)
    presentation_callback.Run(gfx::PresentationFeedback());

  // Call explicit release on all explicit release callbacks that have been
  // committed.
  ImmediateExplicitRelease(
      std::move(state_.per_commit_explicit_release_callback_));
  ImmediateExplicitRelease(
      std::move(cached_state_.per_commit_explicit_release_callback_));

  // Do not reset the DragDropDelegate in order to handle exit upon deletion.
}

// static
Surface* Surface::AsSurface(const aura::Window* window) {
  return window->GetProperty(kSurfaceKey);
}

void Surface::Attach(Buffer* buffer) {
  Attach(buffer, gfx::Vector2d());
}

void Surface::Attach(Buffer* buffer, gfx::Vector2d offset) {
  TRACE_EVENT2(
      "exo", "Surface::Attach", "buffer_id",
      buffer ? static_cast<const void*>(buffer->gfx_buffer()) : nullptr,
      "app_id", GetApplicationId(window_.get()));
  has_pending_contents_ = true;
  if (!pending_state_.buffer.has_value())
    pending_state_.buffer.emplace();
  pending_state_.buffer->Reset(buffer ? buffer->AsWeakPtr()
                                      : base::WeakPtr<Buffer>());
  pending_state_.basic_state.offset = offset;
}

gfx::Vector2d Surface::GetBufferOffset() {
  return state_.basic_state.offset;
}

bool Surface::HasPendingAttachedBuffer() const {
  return pending_state_.buffer.has_value() &&
         pending_state_.buffer->buffer() != nullptr;
}

void Surface::Damage(const gfx::Rect& damage) {
  TRACE_EVENT1("exo", "Surface::Damage", "damage", damage.ToString());

  gfx::Rect t_damage = damage;
  if (t_damage.width() == 0x7FFFFFFF) {
    t_damage.set_width(0x7FFFFFFE);
  }
  if (t_damage.height() == 0x7FFFFFFF) {
    t_damage.set_height(0x7FFFFFFE);
  }

  // SkRegion forbids 0x7FFFFFFF (INT32_MAX) as width or height, see
  // SkRegion_kRunTypeSentinel, and would mark the resulting region from the
  // union below as empty. See https://crbug.com/1463905
  gfx::Rect intersected_damage = gfx::Rect(0x7FFFFFFE, 0x7FFFFFFE);
  intersected_damage.Intersect(t_damage);
  pending_state_.damage.Union(intersected_damage);
}

void Surface::RequestFrameCallback(const FrameCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestFrameCallback");

  pending_state_.frame_callbacks.push_back(callback);
}

void Surface::RequestPresentationCallback(
    const PresentationCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestPresentationCallback");

  pending_state_.presentation_callbacks.push_back(callback);
}

void Surface::SetOpaqueRegion(const cc::Region& region) {
  TRACE_EVENT1("exo", "Surface::SetOpaqueRegion", "region", region.ToString());

  pending_state_.basic_state.opaque_region = region;
}

void Surface::SetInputRegion(const cc::Region& region) {
  TRACE_EVENT1("exo", "Surface::SetInputRegion", "region", region.ToString());

  pending_state_.basic_state.input_region = region;
}

void Surface::ResetInputRegion() {
  TRACE_EVENT0("exo", "Surface::ResetInputRegion");

  pending_state_.basic_state.input_region = absl::nullopt;
}

void Surface::SetInputOutset(int outset) {
  TRACE_EVENT1("exo", "Surface::SetInputOutset", "outset", outset);

  pending_state_.basic_state.input_outset = outset;
}

void Surface::SetBufferScale(float scale) {
  TRACE_EVENT1("exo", "Surface::SetBufferScale", "scale", scale);

  pending_state_.basic_state.buffer_scale = scale;
}

void Surface::SetBufferTransform(Transform transform) {
  TRACE_EVENT1("exo", "Surface::SetBufferTransform", "transform",
               static_cast<int>(transform));

  pending_state_.basic_state.buffer_transform = transform;
}

void Surface::AddSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::AddSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  DCHECK(!sub_surface->window()->parent());
  sub_surface->window()->SetBounds(
      gfx::Rect(sub_surface->window()->bounds().size()));

  // As an optimization, don't add augmented subsurfaces's aura::Window to the
  // tree.
  if (!GetProperty(kSurfaceHasAugmentedSurfaceKey)) {
    window_->AddChild(sub_surface->window());
  }

  DCHECK(!ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::PointF()));
  sub_surfaces_.push_back(std::make_pair(sub_surface, gfx::PointF()));
  sub_surfaces_changed_ = true;

  // Propagate the kSkipImeProcessing property to the new child.
  if (window_->GetProperty(aura::client::kSkipImeProcessing))
    sub_surface->window()->SetProperty(aura::client::kSkipImeProcessing, true);

  // The shell might have not be added to the root yet.
  if (window_->GetRootWindow()) {
    auto display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window_.get());
    sub_surface->UpdateDisplay(display::kInvalidDisplayId, display.id());
  }
}

void Surface::OnNewOutputAdded() {
  if (delegate_)
    delegate_->OnNewOutputAdded();
}

void Surface::RemoveSubSurface(Surface* sub_surface) {
  TRACE_EVENT1("exo", "Surface::RemoveSubSurface", "sub_surface",
               sub_surface->AsTracedValue());

  if (sub_surface->window()->IsVisible())
    sub_surface->window()->Hide();
  if (sub_surface->window()->parent() == window_.get()) {
    window_->RemoveChild(sub_surface->window());
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  pending_sub_surfaces_.erase(
      FindListEntry(pending_sub_surfaces_, sub_surface));

  DCHECK(ListContainsEntry(sub_surfaces_, sub_surface));
  auto it = FindListEntry(sub_surfaces_, sub_surface);
  sub_surfaces_.erase(it);
  // Force recreating resources when the surface is added to a tree again.
  sub_surface->SurfaceHierarchyResourcesLost();
  sub_surfaces_changed_ = true;
}

void Surface::SetSubSurfacePosition(Surface* sub_surface,
                                    const gfx::PointF& position) {
  TRACE_EVENT2("exo", "Surface::SetSubSurfacePosition", "sub_surface",
               sub_surface->AsTracedValue(), "position", position.ToString());

  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  DCHECK(it != pending_sub_surfaces_.end());
  if (it->second == position)
    return;
  it->second = position;
  sub_surfaces_changed_ = true;
}

void Surface::PlaceSubSurfaceAbove(Surface* sub_surface, Surface* reference) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceAbove", "sub_surface",
               sub_surface->AsTracedValue(), "reference",
               reference->AsTracedValue());

  if (sub_surface == reference) {
    DLOG(WARNING) << "Client tried to place sub-surface above itself";
    return;
  }

  auto position_it = pending_sub_surfaces_.begin();
  if (reference != this) {
    position_it = FindListEntry(pending_sub_surfaces_, reference);
    if (position_it == pending_sub_surfaces_.end()) {
      DLOG(WARNING) << "Client tried to place sub-surface above a reference "
                       "surface that is neither a parent nor a sibling";
      return;
    }

    // Advance iterator to have |position_it| point to the sibling surface
    // above |reference|.
    ++position_it;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);
  if (it == position_it)
    return;
  pending_sub_surfaces_.splice(position_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
}

void Surface::PlaceSubSurfaceBelow(Surface* sub_surface, Surface* sibling) {
  TRACE_EVENT2("exo", "Surface::PlaceSubSurfaceBelow", "sub_surface",
               sub_surface->AsTracedValue(), "sibling",
               sibling->AsTracedValue());

  if (sub_surface == sibling) {
    DLOG(WARNING) << "Client tried to place sub-surface below itself";
    return;
  }

  auto position_it = FindListEntry(pending_sub_surfaces_, sibling);
  if (position_it == pending_sub_surfaces_.end()) {
    DLOG(WARNING) << "Client tried to place sub-surface below a surface that "
                     "is not a sibling";
    return;
  }

  DCHECK(ListContainsEntry(pending_sub_surfaces_, sub_surface));
  auto it = FindListEntry(pending_sub_surfaces_, sub_surface);

  // If |sub_surface| is already immediately below |sibling|, do not do
  // anything.
  if (it == --position_it)
    return;
  pending_sub_surfaces_.splice(++position_it, pending_sub_surfaces_, it);
  sub_surfaces_changed_ = true;
}

void Surface::OnSubSurfaceCommit() {
  if (delegate_)
    delegate_->OnSurfaceCommit();
}

void Surface::SetRoundedCorners(const gfx::RRectF& rounded_corners_bounds,
                                bool is_root_coordinates,
                                bool commit_override) {
  TRACE_EVENT1("exo", "Surface::SetRoundedCorner", "corners",
               rounded_corners_bounds.ToString());

  if (rounded_corners_bounds != pending_state_.rounded_corners_bounds ||
      is_root_coordinates !=
          pending_state_.rounded_corners_is_root_coordinates) {
    has_pending_contents_ = true;
    pending_state_.rounded_corners_bounds = rounded_corners_bounds;
    pending_state_.rounded_corners_is_root_coordinates = is_root_coordinates;
  }

  if (commit_override &&
      (rounded_corners_bounds != state_.rounded_corners_bounds ||
       is_root_coordinates != state_.rounded_corners_is_root_coordinates)) {
    state_.rounded_corners_bounds = rounded_corners_bounds;
    state_.rounded_corners_is_root_coordinates = is_root_coordinates;
  }
}

void Surface::SetOverlayPriorityHint(OverlayPriority hint) {
  TRACE_EVENT0("exo", "Surface::SetOverlayPriorityHint");
  pending_state_.overlay_priority_hint = hint;
}

void Surface::SetClipRect(const absl::optional<gfx::RectF>& clip_rect) {
  TRACE_EVENT1("exo", "Surface::SetClipRect", "clip_rect",
               (clip_rect ? clip_rect->ToString() : "nullopt"));

  if (pending_state_.clip_rect == clip_rect &&
      !pending_state_.clip_rect_is_parent_coordinates) {
    return;
  }
  has_pending_contents_ = true;
  pending_state_.clip_rect = clip_rect;
  pending_state_.clip_rect_is_parent_coordinates = false;
}

void Surface::SetClipRectOnParentSurface(
    const absl::optional<gfx::RectF>& clip_rect) {
  TRACE_EVENT1("exo", "Surface::SetClipRectOnParentSurface", "clip_rect",
               (clip_rect ? clip_rect->ToString() : "nullopt"));

  if (pending_state_.clip_rect == clip_rect &&
      pending_state_.clip_rect_is_parent_coordinates) {
    return;
  }
  has_pending_contents_ = true;
  pending_state_.clip_rect = clip_rect;
  pending_state_.clip_rect_is_parent_coordinates = true;
}

void Surface::SetSurfaceTransform(const gfx::Transform& transform) {
  TRACE_EVENT1("exo", "Surface::SetSurfaceTransform", "transform",
               transform.ToString());
  if (pending_state_.surface_transform != transform) {
    has_pending_contents_ = true;
    pending_state_.surface_transform = transform;
  }
}

void Surface::SetBackgroundColor(absl::optional<SkColor4f> background_color) {
  TRACE_EVENT0("exo", "Surface::SetBackgroundColor");
  pending_state_.basic_state.background_color = background_color;
}

void Surface::SetTrustedDamage(bool trusted_damage) {
  TRACE_EVENT0("exo", "Surface::SetTrustedDamage");
  trusted_damage_ = trusted_damage;
}

void Surface::SetViewport(const gfx::SizeF& viewport) {
  TRACE_EVENT1("exo", "Surface::SetViewport", "viewport", viewport.ToString());

  pending_state_.basic_state.viewport = viewport;
}

void Surface::SetCrop(const gfx::RectF& crop) {
  TRACE_EVENT1("exo", "Surface::SetCrop", "crop", crop.ToString());

  pending_state_.basic_state.crop = crop;
}

void Surface::SetOnlyVisibleOnSecureOutput(bool only_visible_on_secure_output) {
  TRACE_EVENT1("exo", "Surface::SetOnlyVisibleOnSecureOutput",
               "only_visible_on_secure_output", only_visible_on_secure_output);

  pending_state_.basic_state.only_visible_on_secure_output =
      only_visible_on_secure_output;
}

void Surface::SetBlendMode(SkBlendMode blend_mode) {
  TRACE_EVENT1("exo", "Surface::SetBlendMode", "blend_mode",
               static_cast<int>(blend_mode));

  pending_state_.basic_state.blend_mode = blend_mode;
}

void Surface::SetAlpha(float alpha) {
  TRACE_EVENT1("exo", "Surface::SetAlpha", "alpha", alpha);

  pending_state_.basic_state.alpha = alpha;
}

void Surface::SetFrame(SurfaceFrameType type) {
  TRACE_EVENT1("exo", "Surface::SetFrame", "type", static_cast<uint32_t>(type));

  if (delegate_)
    delegate_->OnSetFrame(type);
}

void Surface::SetServerStartResize() {
  if (delegate_)
    delegate_->OnSetServerStartResize();
  SetFrame(SurfaceFrameType::SHADOW);
}

void Surface::SetFrameColors(SkColor active_color, SkColor inactive_color) {
  TRACE_EVENT2("exo", "Surface::SetFrameColors", "active_color", active_color,
               "inactive_color", inactive_color);

  if (delegate_)
    delegate_->OnSetFrameColors(active_color, inactive_color);
}

void Surface::SetStartupId(const char* startup_id) {
  TRACE_EVENT1("exo", "Surface::SetStartupId", "startup_id", startup_id);

  if (delegate_)
    delegate_->OnSetStartupId(startup_id);
}

void Surface::SetApplicationId(const char* application_id) {
  TRACE_EVENT1("exo", "Surface::SetApplicationId", "application_id",
               application_id);

  if (delegate_)
    delegate_->OnSetApplicationId(application_id);
}

void Surface::SetUseImmersiveForFullscreen(bool value) {
  TRACE_EVENT1("exo", "Surface::SetUseImmersiveForFullscreen", "value", value);

  if (delegate_)
    delegate_->SetUseImmersiveForFullscreen(value);
}

void Surface::ShowSnapPreviewToSecondary() {
  if (delegate_)
    delegate_->ShowSnapPreviewToSecondary();
}

void Surface::ShowSnapPreviewToPrimary() {
  if (delegate_)
    delegate_->ShowSnapPreviewToPrimary();
}

void Surface::HideSnapPreview() {
  if (delegate_)
    delegate_->HideSnapPreview();
}

void Surface::SetSnapPrimary(float snap_ratio) {
  if (delegate_)
    delegate_->SetSnapPrimary(snap_ratio);
}

void Surface::SetSnapSecondary(float snap_ratio) {
  if (delegate_)
    delegate_->SetSnapSecondary(snap_ratio);
}

void Surface::UnsetSnap() {
  if (delegate_)
    delegate_->UnsetSnap();
}

void Surface::SetCanGoBack() {
  if (delegate_)
    delegate_->SetCanGoBack();
}

void Surface::UnsetCanGoBack() {
  if (delegate_)
    delegate_->UnsetCanGoBack();
}

void Surface::SetColorSpace(gfx::ColorSpace color_space) {
  TRACE_EVENT1("exo", "Surface::SetColorSpace", "color_space",
               color_space.ToString());

  pending_state_.basic_state.color_space = color_space;
}

void Surface::SetParent(Surface* parent, const gfx::Point& position) {
  TRACE_EVENT2("exo", "Surface::SetParent", "parent", !!parent, "position",
               position.ToString());

  if (delegate_)
    delegate_->OnSetParent(parent, position);
}

void Surface::RequestActivation() {
  TRACE_EVENT0("exo", "Surface::RequestActivation");

  if (delegate_)
    delegate_->OnActivationRequested();
}

void Surface::SetClientSurfaceId(const char* client_surface_id) {
  if (client_surface_id && strlen(client_surface_id) > 0)
    window_->SetProperty(kClientSurfaceIdKey,
                         new std::string(client_surface_id));
  else
    window_->ClearProperty(kClientSurfaceIdKey);
}

std::string Surface::GetClientSurfaceId() const {
  std::string* value = window_->GetProperty(kClientSurfaceIdKey);
  return value ? *value : std::string();
}

void Surface::SetContainsVideo(bool contains_video) {
  TRACE_EVENT1("exo", "Surface::SetContainsVideo", "contains_video",
               contains_video ? "true" : "false");
  pending_state_.basic_state.contains_video = contains_video;
}

bool Surface::ContainsVideo() {
  if (state_.basic_state.contains_video)
    return true;

  for (auto& subsurface : sub_surfaces_) {
    if (subsurface.first->ContainsVideo())
      return true;
  }
  return false;
}

void Surface::SetWindowSessionId(int32_t window_session_id) {
  if (window_session_id > 0)
    window_->SetProperty(kWindowSessionId, window_session_id);
  else
    window_->ClearProperty(kWindowSessionId);
}

int32_t Surface::GetWindowSessionId() {
  return window_->GetProperty(kWindowSessionId);
}

void Surface::SetPip() {
  if (delegate_)
    delegate_->SetPip();
}

void Surface::UnsetPip() {
  if (delegate_)
    delegate_->UnsetPip();
}

void Surface::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  if (delegate_)
    delegate_->SetAspectRatio(aspect_ratio);
}

void Surface::SetEmbeddedSurfaceId(
    base::RepeatingCallback<viz::SurfaceId()> surface_id_callback) {
  get_current_surface_id_ = std::move(surface_id_callback);
  first_embedded_surface_id_ = viz::SurfaceId();
}

void Surface::SetEmbeddedSurfaceSize(const gfx::Size& size) {
  embedded_surface_size_ = size;
}

void Surface::SetAcquireFence(std::unique_ptr<gfx::GpuFence> gpu_fence) {
  TRACE_EVENT1("exo", "Surface::SetAcquireFence", "fence_fd",
               gpu_fence ? gpu_fence->GetGpuFenceHandle().Peek() : -1);

  pending_state_.acquire_fence = std::move(gpu_fence);
}

bool Surface::HasPendingAcquireFence() const {
  return !!pending_state_.acquire_fence;
}

bool Surface::HasAcquireFence() const {
  return !!state_.acquire_fence;
}

void Surface::SetPerCommitBufferReleaseCallback(
    Buffer::PerCommitExplicitReleaseCallback callback) {
  TRACE_EVENT0("exo", "Surface::SetPerCommitBufferReleaseCallback");

  pending_state_.per_commit_explicit_release_callback_ = std::move(callback);
}

bool Surface::HasPendingPerCommitBufferReleaseCallback() const {
  return !!pending_state_.per_commit_explicit_release_callback_;
}

void Surface::Commit() {
  TRACE_EVENT1(
      "exo", "Surface::Commit", "buffer_id",
      static_cast<const void*>(
          pending_state_.buffer.has_value() && pending_state_.buffer->buffer()
              ? pending_state_.buffer->buffer()->gfx_buffer()
              : nullptr));

  for (auto& observer : observers_)
    observer.OnCommit(this);

  needs_commit_surface_ = true;

  // Transfer pending state to cached state.
  cached_state_.basic_state = pending_state_.basic_state;
  pending_state_.basic_state.only_visible_on_secure_output = false;
  has_cached_contents_ |= has_pending_contents_;
  has_pending_contents_ = false;
  if (pending_state_.buffer.has_value()) {
    cached_state_.buffer = std::move(pending_state_.buffer);
    pending_state_.buffer.reset();
  }
  cached_state_.rounded_corners_bounds = pending_state_.rounded_corners_bounds;
  cached_state_.rounded_corners_is_root_coordinates =
      pending_state_.rounded_corners_is_root_coordinates;
  cached_state_.overlay_priority_hint = pending_state_.overlay_priority_hint;
  cached_state_.clip_rect = pending_state_.clip_rect;
  cached_state_.clip_rect_is_parent_coordinates =
      pending_state_.clip_rect_is_parent_coordinates;
  cached_state_.surface_transform = pending_state_.surface_transform;
  cached_state_.acquire_fence = std::move(pending_state_.acquire_fence);
  cached_state_.per_commit_explicit_release_callback_ =
      std::move(pending_state_.per_commit_explicit_release_callback_);
  cached_state_.frame_callbacks.splice(cached_state_.frame_callbacks.end(),
                                       pending_state_.frame_callbacks);
  cached_state_.damage.Union(pending_state_.damage);
  pending_state_.damage.Clear();

  // Existing presentation callbacks in the cached state when a new pending
  // state is merged in should end up delivered as "discarded".
  for (const auto& presentation_callback : cached_state_.presentation_callbacks)
    presentation_callback.Run(gfx::PresentationFeedback());
  cached_state_.presentation_callbacks.clear();
  cached_state_.presentation_callbacks.splice(
      cached_state_.presentation_callbacks.end(),
      pending_state_.presentation_callbacks);

  if (delegate_)
    delegate_->OnSurfaceCommit();
  else
    CommitSurfaceHierarchy(false);
}

bool Surface::UpdateDisplay(int64_t old_display, int64_t new_display) {
  display_id_ = new_display;
  if (has_contents() && !leave_enter_callback_.is_null()) {
    if (!leave_enter_callback_.Run(old_display, new_display)) {
      return false;
    }
  }
  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    if (!sub_surface->UpdateDisplay(old_display, new_display))
      return false;
  }

  for (auto& observer : observers_) {
    observer.OnDisplayChanged(this, old_display, new_display);
  }

  return true;
}

display::Display Surface::GetDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window());
}

void Surface::CommitSurfaceHierarchy(bool synchronized) {
  TRACE_EVENT0("exo", "Surface::CommitSurfaceHierarchy");
  if (needs_commit_surface_ && (synchronized || !IsSynchronized())) {
    needs_commit_surface_ = false;
    synchronized = true;

    // TODO(penghuang): Make the damage more precise for sub surface changes.
    // https://crbug.com/779704
    bool needs_full_damage = false;
    if (!trusted_damage_) {
      needs_full_damage =
          sub_surfaces_changed_ ||
          cached_state_.basic_state.opaque_region !=
              state_.basic_state.opaque_region ||
          cached_state_.basic_state.buffer_scale !=
              state_.basic_state.buffer_scale ||
          cached_state_.basic_state.buffer_transform !=
              state_.basic_state.buffer_transform ||
          cached_state_.basic_state.viewport != state_.basic_state.viewport ||
          cached_state_.rounded_corners_bounds !=
              state_.rounded_corners_bounds ||
          cached_state_.basic_state.crop != state_.basic_state.crop ||
          cached_state_.basic_state.only_visible_on_secure_output !=
              state_.basic_state.only_visible_on_secure_output ||
          cached_state_.basic_state.blend_mode !=
              state_.basic_state.blend_mode ||
          cached_state_.basic_state.alpha != state_.basic_state.alpha ||
          cached_state_.basic_state.color_space !=
              state_.basic_state.color_space ||
          cached_state_.basic_state.is_tracking_occlusion !=
              state_.basic_state.is_tracking_occlusion;
    }

    bool needs_update_buffer_transform =
        cached_state_.basic_state.buffer_scale !=
            state_.basic_state.buffer_scale ||
        cached_state_.basic_state.buffer_transform !=
            state_.basic_state.buffer_transform;

    bool needs_output_protection =
        cached_state_.basic_state.only_visible_on_secure_output !=
        state_.basic_state.only_visible_on_secure_output;

    bool cached_invert_y = false;

    // If the current state is fully transparent, the last submitted frame will
    // not include the TextureDrawQuad for the resource, so the resource might
    // have been released and needs to be updated again.
    if (!state_.basic_state.alpha && cached_state_.basic_state.alpha)
      needs_update_resource_ = true;

    state_.basic_state = cached_state_.basic_state;
    cached_state_.basic_state.only_visible_on_secure_output = false;

    window_->SetEventTargetingPolicy(
        (state_.basic_state.input_region.has_value() &&
         state_.basic_state.input_region->IsEmpty())
            ? aura::EventTargetingPolicy::kDescendantsOnly
            : aura::EventTargetingPolicy::kTargetAndDescendants);

    if (state_.basic_state.is_tracking_occlusion) {
      // TODO(edcourtney): Currently, it doesn't seem to be possible to stop
      // tracking the occlusion state once started, but it would be nice to stop
      // if the tracked occlusion region becomes empty.
      window_->TrackOcclusionState();
    }

    if (needs_output_protection) {
      if (!output_protection_) {
        output_protection_ =
            std::make_unique<ash::OutputProtectionDelegate>(window_.get());
      }

      uint32_t protection_mask =
          state_.basic_state.only_visible_on_secure_output
              ? display::CONTENT_PROTECTION_METHOD_HDCP
              : display::CONTENT_PROTECTION_METHOD_NONE;

      output_protection_->SetProtection(protection_mask, base::DoNothing());
    }

    // We update contents if Attach() has been called since last commit.
    if (has_cached_contents_) {
      has_cached_contents_ = false;

      bool current_invert_y = state_.buffer.has_value() &&
                              state_.buffer->buffer() &&
                              state_.buffer->buffer()->y_invert();
      cached_invert_y = cached_state_.buffer.has_value() &&
                        cached_state_.buffer->buffer() &&
                        cached_state_.buffer->buffer()->y_invert();
      if (current_invert_y != cached_invert_y)
        needs_update_buffer_transform = true;

      if (cached_state_.buffer.has_value()) {
        bool had_contents = has_contents();

        state_.buffer = std::move(cached_state_.buffer);
        cached_state_.buffer.reset();

        if (display_id_ != display::kInvalidDisplayId &&
            !leave_enter_callback_.is_null()) {
          if (!had_contents && has_contents()) {
            leave_enter_callback_.Run(display::kInvalidDisplayId, display_id_);
          } else if (had_contents && !has_contents()) {
            leave_enter_callback_.Run(display_id_, display::kInvalidDisplayId);
          }
        }
      }
      state_.rounded_corners_bounds = cached_state_.rounded_corners_bounds;
      state_.rounded_corners_is_root_coordinates =
          cached_state_.rounded_corners_is_root_coordinates;
      state_.clip_rect = cached_state_.clip_rect;
      state_.clip_rect_is_parent_coordinates =
          cached_state_.clip_rect_is_parent_coordinates;
      state_.surface_transform = cached_state_.surface_transform;
      state_.acquire_fence = std::move(cached_state_.acquire_fence);
      state_.per_commit_explicit_release_callback_ =
          std::move(cached_state_.per_commit_explicit_release_callback_);
      if (state_.basic_state.alpha)
        needs_update_resource_ = true;
    }

    // The overlay priority hint can get set before any buffer gets
    // allocated/attached and may influence the format/modifier selection for
    // these.
    UpdateOverlayPriorityHint(cached_state_.overlay_priority_hint);

    // Either we didn't have a pending acquire fence, or we had one along with
    // a new buffer, and it was already moved to state_.acquire_fence. Note that
    // it is a commit-time client error to commit a fence without a buffer.
    DCHECK(!cached_state_.acquire_fence);
    // Similarly for the per commit buffer release callback.
    DCHECK(!cached_state_.per_commit_explicit_release_callback_);

    if (needs_update_buffer_transform)
      UpdateBufferTransform(cached_invert_y);

    // Move pending frame callbacks to the end of |state_.frame_callbacks|.
    state_.frame_callbacks.splice(state_.frame_callbacks.end(),
                                  cached_state_.frame_callbacks);

    // Move pending presentation callbacks to the end of
    // |state_.presentation_callbacks|.
    state_.presentation_callbacks.splice(state_.presentation_callbacks.end(),
                                         cached_state_.presentation_callbacks);

    UpdateContentSize();

    // Synchronize window hierarchy. This will position and update the stacking
    // order of all sub-surfaces after committing all pending state of
    // sub-surface descendants.
    // Changes to sub_surface stack is immediately applied to pending, which
    // will be copied to active directly when parent surface is committed,
    // skipping the cached state.
    if (sub_surfaces_changed_) {
      sub_surfaces_.clear();
      aura::Window* stacking_target = nullptr;
      for (const auto& sub_surface_entry : pending_sub_surfaces_) {
        Surface* sub_surface = sub_surface_entry.first;
        // If the parent has trusted damage set, then consider it trusted for
        // all subsurfaces.
        sub_surface->SetTrustedDamage(trusted_damage_);
        sub_surfaces_.push_back(sub_surface_entry);
        // Move sub-surface to its new position in the stack.
        if (stacking_target && sub_surface->window()->parent()) {
          window_->StackChildAbove(sub_surface->window(), stacking_target);
        }

        // Stack next sub-surface above this sub-surface.
        stacking_target = sub_surface->window();

        // Update sub-surface position relative to surface origin.
        sub_surface->window()->SetBounds(
            gfx::Rect(gfx::ToFlooredPoint(sub_surface_entry.second),
                      sub_surface->window()->bounds().size()));
      }
      sub_surfaces_changed_ = false;
    }

    gfx::Rect output_rect(gfx::ToCeiledSize(content_size_));
    if (needs_full_damage) {
      state_.damage = output_rect;
    } else {
      // cached_state_.damage is in Surface coordinates.
      state_.damage.Swap(&cached_state_.damage);
      state_.damage.Intersect(output_rect);
    }
    cached_state_.damage.Clear();
  }

  surface_hierarchy_content_bounds_ =
      gfx::Rect(gfx::ToCeiledSize(content_size_));

  if (state_.basic_state.input_region) {
    hit_test_region_ = *state_.basic_state.input_region;
    hit_test_region_.Intersect(surface_hierarchy_content_bounds_);
  } else {
    hit_test_region_ = surface_hierarchy_content_bounds_;
  }

  int outset = state_.basic_state.input_outset;
  if (outset > 0) {
    gfx::Rect input_rect = surface_hierarchy_content_bounds_;
    input_rect.Inset(-outset);
    hit_test_region_ = input_rect;
  }

  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    gfx::Vector2d offset =
        gfx::ToRoundedPoint(sub_surface_entry.second).OffsetFromOrigin();
    // Synchronously commit all pending state of the sub-surface and its
    // descendants.
    sub_surface->CommitSurfaceHierarchy(synchronized);
    surface_hierarchy_content_bounds_.Union(
        sub_surface->surface_hierarchy_content_bounds() + offset);
    hit_test_region_.Union(sub_surface->hit_test_region_ + offset);
  }
}

void Surface::AppendSurfaceHierarchyCallbacks(
    std::list<FrameCallback>* frame_callbacks,
    std::list<PresentationCallback>* presentation_callbacks) {
  // Move frame callbacks to the end of |frame_callbacks|.
  frame_callbacks->splice(frame_callbacks->end(), state_.frame_callbacks);
  // Move presentation callbacks to the end of |presentation_callbacks|.
  presentation_callbacks->splice(presentation_callbacks->end(),
                                 state_.presentation_callbacks);

  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    sub_surface->AppendSurfaceHierarchyCallbacks(frame_callbacks,
                                                 presentation_callbacks);
  }
}

void Surface::AppendSurfaceHierarchyContentsToFrame(
    const gfx::PointF& origin,
    bool needs_full_damage,
    FrameSinkResourceManager* resource_manager,
    absl::optional<float> device_scale_factor,
    viz::CompositorFrame* frame) {
  // The top most sub-surface is at the front of the RenderPass's quad_list,
  // so we need composite sub-surface in reversed order.
  for (const auto& sub_surface_entry : base::Reversed(sub_surfaces_)) {
    auto* sub_surface = sub_surface_entry.first;
    // Synchronsouly commit all pending state of the sub-surface and its
    // decendents.
    sub_surface->AppendSurfaceHierarchyContentsToFrame(
        origin + sub_surface_entry.second.OffsetFromOrigin(), needs_full_damage,
        resource_manager, device_scale_factor, frame);
  }

  // Update the resource, or if not required, ensure we call the buffer release
  // callback, since the buffer will not be used for this commit.
  if (needs_update_resource_) {
    UpdateResource(resource_manager);
  } else {
    ImmediateExplicitRelease(
        std::move(state_.per_commit_explicit_release_callback_));
  }

  AppendContentsToFrame(origin, needs_full_damage, device_scale_factor, frame);
}

bool Surface::IsSynchronized() const {
  return delegate_ && delegate_->IsSurfaceSynchronized();
}

bool Surface::IsInputEnabled(Surface* surface) const {
  return !delegate_ || delegate_->IsInputEnabled(surface);
}

bool Surface::HasHitTestRegion() const {
  return !hit_test_region_.IsEmpty();
}

bool Surface::HitTest(const gfx::Point& point) const {
  return hit_test_region_.Contains(point);
}

void Surface::GetHitTestMask(SkPath* mask) const {
  hit_test_region_.GetBoundaryPath(mask);
}

void Surface::SetSurfaceDelegate(SurfaceDelegate* delegate) {
  DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
}

bool Surface::HasSurfaceDelegate() const {
  return !!delegate_;
}

SurfaceDelegate* Surface::GetDelegateForTesting() {
  return delegate_;
}

void Surface::AddSurfaceObserver(SurfaceObserver* observer) {
  observers_.AddObserver(observer);
}

void Surface::RemoveSurfaceObserver(SurfaceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Surface::HasSurfaceObserver(const SurfaceObserver* observer) const {
  return observers_.HasObserver(observer);
}

std::unique_ptr<base::trace_event::TracedValue> Surface::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("name", window_->layer()->name());
  return value;
}

bool Surface::IsStylusOnly() {
  return window_->GetProperty(kStylusOnlyKey);
}

void Surface::SetStylusOnly() {
  window_->SetProperty(kStylusOnlyKey, true);
}

void Surface::SurfaceHierarchyResourcesLost() {
  // Update resource and full damage are needed for next frame.
  needs_update_resource_ = true;
  for (const auto& sub_surface : sub_surfaces_)
    sub_surface.first->SurfaceHierarchyResourcesLost();
}

bool Surface::FillsBoundsOpaquely() const {
  return !current_resource_has_alpha_ ||
         state_.basic_state.blend_mode == SkBlendMode::kSrc ||
         state_.basic_state.opaque_region.Contains(
             gfx::ToEnclosingRect(gfx::RectF(content_size_)));
}

void Surface::SetOcclusionTracking(bool tracking) {
  pending_state_.basic_state.is_tracking_occlusion = tracking;
}

bool Surface::IsTrackingOcclusion() {
  return state_.basic_state.is_tracking_occlusion;
}

void Surface::SetSurfaceHierarchyContentBoundsForTest(
    const gfx::Rect& content_bounds) {
  surface_hierarchy_content_bounds_ = content_bounds;
}

////////////////////////////////////////////////////////////////////////////////
// Buffer, private:

Surface::State::State() {}

Surface::State::~State() = default;

bool Surface::State::operator==(const State& other) const {
  return other.opaque_region == opaque_region &&
         other.input_region == input_region &&
         other.buffer_scale == buffer_scale &&
         other.buffer_transform == buffer_transform &&
         other.viewport == viewport && other.crop == crop &&
         other.only_visible_on_secure_output == only_visible_on_secure_output &&
         other.blend_mode == blend_mode && other.alpha == alpha;
}

Surface::BufferAttachment::BufferAttachment() = default;

Surface::BufferAttachment::~BufferAttachment() {
  if (buffer_)
    buffer_->OnDetach();
}

Surface::BufferAttachment::BufferAttachment(BufferAttachment&& other) {
  *this = std::move(other);
}

Surface::BufferAttachment& Surface::BufferAttachment::operator=(
    BufferAttachment&& other) {
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = other.buffer_;
  size_ = other.size_;
  other.buffer_ = base::WeakPtr<Buffer>();
  other.size_ = gfx::Size();
  return *this;
}

base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() {
  return buffer_;
}

const base::WeakPtr<Buffer>& Surface::BufferAttachment::buffer() const {
  return buffer_;
}

const gfx::Size& Surface::BufferAttachment::size() const {
  return size_;
}

void Surface::BufferAttachment::Reset(base::WeakPtr<Buffer> buffer) {
  size_ = gfx::Size();
  if (buffer) {
    buffer->OnAttach();
    size_ = buffer->GetSize();
  }
  if (buffer_)
    buffer_->OnDetach();
  buffer_ = buffer;
}

Surface::ExtendedState::ExtendedState() = default;

Surface::ExtendedState::~ExtendedState() = default;

void Surface::UpdateResource(FrameSinkResourceManager* resource_manager) {
  DCHECK(needs_update_resource_);
  needs_update_resource_ = false;
  if (state_.buffer.has_value() && state_.buffer->buffer()) {
    gfx::ColorSpace buffer_color_space = state_.basic_state.color_space;
    // Invalid color spaces cause issues went sent to the buffer. In these cases
    // revert to passing SRGB as before.
    if (!buffer_color_space.IsValid()) {
      buffer_color_space = gfx::ColorSpace::CreateSRGB();
    }
    if (legacy_buffer_release_skippable_ &&
        state_.per_commit_explicit_release_callback_) {
      state_.buffer->buffer()->SkipLegacyRelease();
    }
    if (state_.buffer->buffer()->ProduceTransferableResource(
            resource_manager, std::move(state_.acquire_fence),
            state_.basic_state.only_visible_on_secure_output,
            &current_resource_, buffer_color_space,
            window_->GetToplevelWindow()->GetProperty(
                kProtectedNativePixmapQueryDelegate),
            std::move(state_.per_commit_explicit_release_callback_))) {
      current_resource_has_alpha_ =
          FormatHasAlpha(state_.buffer->buffer()->GetFormat());
      current_resource_.color_space = state_.basic_state.color_space;
    } else {
      current_resource_.id = viz::kInvalidResourceId;
      // Use the buffer's size, so the AppendContentsToFrame() will append
      // a SolidColorDrawQuad with the buffer's size.
      current_resource_.size = state_.buffer->size();
      SkColor4f color = state_.buffer->buffer()->GetColor();
      current_resource_has_alpha_ = !color.isOpaque();
    }
  } else {
    current_resource_.id = viz::kInvalidResourceId;
    current_resource_.size = gfx::Size();
    current_resource_has_alpha_ = false;
    ImmediateExplicitRelease(
        std::move(state_.per_commit_explicit_release_callback_));
  }
}

void Surface::UpdateBufferTransform(bool y_invert) {
  buffer_transform_ =
      ToBufferTransformMatrix(state_.basic_state.buffer_transform, y_invert);
  if (state_.basic_state.buffer_scale != 0) {
    buffer_transform_.PostScale(1.0f / state_.basic_state.buffer_scale,
                                1.0f / state_.basic_state.buffer_scale);
  }
}

void Surface::UpdateOverlayPriorityHint(OverlayPriority overlay_priority_hint) {
  if (state_.overlay_priority_hint == overlay_priority_hint) {
    return;
  }

  state_.overlay_priority_hint = overlay_priority_hint;
  for (SurfaceObserver& observer : observers_) {
    observer.OnOverlayPriorityHintChanged(overlay_priority_hint);
  }
}

// Try to share the |SharedQuadState| (sqs) when a single layer can be
// reconstructed. This is important for performance reasons in the occlusion
// code and correctness in the per edge anti-alias code.
static viz::SharedQuadState* AppendOrCreateSharedQuadState(
    float opacity,
    const std::unique_ptr<viz::CompositorRenderPass>& render_pass,
    const gfx::Transform quad_to_target_transform,
    const gfx::Rect& quad_rect,
    const gfx::MaskFilterInfo& msk,
    const absl::optional<gfx::Rect>& quad_clip_rect,
    const bool are_contents_opaque) {
  viz::SharedQuadState* quad_state =
      !render_pass->shared_quad_state_list.empty()
          ? render_pass->shared_quad_state_list.back()
          : nullptr;
  auto test_union = quad_rect;
  bool is_sealed_union = false;
  if (quad_state) {
    // A sealed union is when the combined rect has no gaps and can form a
    // single layer rect.
    test_union.Union(quad_state->quad_layer_rect);
    if ((test_union.width() == quad_rect.width() &&
         test_union.width() == quad_state->quad_layer_rect.width())) {
      if (quad_rect.height() + quad_state->quad_layer_rect.height() >=
          test_union.height())
        is_sealed_union = true;
    }

    if ((test_union.height() == quad_rect.height() &&
         test_union.height() == quad_state->quad_layer_rect.height())) {
      if (quad_rect.width() + quad_state->quad_layer_rect.width() >=
          test_union.width())
        is_sealed_union = true;
    }
  }

  if (quad_state && is_sealed_union &&
      quad_to_target_transform == quad_state->quad_to_target_transform &&
      opacity == quad_state->opacity &&
      quad_clip_rect == quad_state->clip_rect &&
      are_contents_opaque == quad_state->are_contents_opaque &&
      msk == quad_state->mask_filter_info) {
    // Expland the layer portion of the sqs.
    quad_state->quad_layer_rect = test_union;
    quad_state->visible_quad_layer_rect = test_union;
  } else {
    quad_state = render_pass->CreateAndAppendSharedQuadState();
    quad_state->SetAll(quad_to_target_transform, quad_rect, quad_rect, msk,
                       quad_clip_rect, are_contents_opaque, opacity,
                       SkBlendMode::kSrcOver, /*sorting_context=*/0,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  }
  return quad_state;
}

void Surface::AppendContentsToFrame(const gfx::PointF& origin,
                                    bool needs_full_damage,
                                    absl::optional<float> device_scale_factor,
                                    viz::CompositorFrame* frame) {
  const std::unique_ptr<viz::CompositorRenderPass>& render_pass =
      frame->render_pass_list.back();
  gfx::RectF output_rect(origin, content_size_);
  gfx::Rect quad_rect(0, 0, 1, 1);

  // Surface bounds are in DIPs, but |damage_rect| and |output_rect| are in
  // pixels, so we need to scale by the |device_scale_factor|.
  gfx::RectF damage_rect = needs_full_damage
                               ? gfx::RectF(content_size_)
                               : gfx::RectF(state_.damage.bounds());
  if (!damage_rect.IsEmpty()) {
    // Outset damage by 1 DIP to as damage is in surface coordinate space and
    // client might not be aware of |device_scale_factor| and the
    // scaling/filtering it requires.
    damage_rect.Inset(-1);
    damage_rect += origin.OffsetFromOrigin();
    damage_rect.Intersect(output_rect);

    if (device_scale_factor.has_value()) {
      if (device_scale_factor.value() <= 1) {
        damage_rect =
            gfx::ConvertRectToPixels(damage_rect, device_scale_factor.value());
      } else {
        // The damage will eventually be rescaled by 1/device_scale_factor.
        // Since that scale factor is <1, taking the enclosed rect here means
        // that that rescaled RectF is <1px smaller than |damage_rect| in each
        // dimension, which makes the enclosing rect equal to |damage_rect|.
        damage_rect.Scale(device_scale_factor.value());
      }
    }
  }

  absl::optional<gfx::Rect> quad_clip_rect;
  if (state_.clip_rect) {
    // `state_.clip_rect` should be on local surface coordinates but the
    // deprecated implementation still uses parent surface coordinates. If so,
    // we skip translating into the root surface coordinates to keep the old
    // behavior.
    // TODO(crbug.com/1457446): Remove this.
    auto clip_rect_offset = state_.clip_rect_is_parent_coordinates
                                ? gfx::Vector2d()
                                : origin.OffsetFromOrigin();
    quad_clip_rect = gfx::ToEnclosedRect(*state_.clip_rect + clip_rect_offset);
  }

  state_.damage.Clear();

  gfx::Vector2dF scale(content_size_.width(), content_size_.height());

  gfx::Vector2dF translate(0.0f, 0.0f);

  // Surface quads require the quad rect to be appropriately sized and need to
  // use the shared quad clip rect.
  if (get_current_surface_id_) {
    quad_rect = gfx::Rect(embedded_surface_size_);
    scale = gfx::Vector2dF(1.0f, 1.0f);

    if (!state_.basic_state.crop.IsEmpty()) {
      // In order to crop an AxB rect to CxD we need to scale by A/C, B/D.
      // We achieve clipping by scaling it up and then drawing only in the
      // output rectangle.
      scale.Scale(content_size_.width() / state_.basic_state.crop.width(),
                  content_size_.height() / state_.basic_state.crop.height());

      auto offset = state_.basic_state.crop.origin().OffsetFromOrigin();
      translate =
          gfx::Vector2dF(-offset.x() * scale.x(), -offset.y() * scale.y());
    }
  } else {
    scale.Scale(state_.basic_state.buffer_scale);
  }

  bool are_contents_opaque =
      !current_resource_has_alpha_ ||
      state_.basic_state.blend_mode == SkBlendMode::kSrc ||
      state_.basic_state.opaque_region.Contains(
          gfx::ToEnclosedRect(output_rect));

  gfx::MaskFilterInfo msk;
  if (!state_.rounded_corners_bounds.IsEmpty()) {
    // `state_.rounded_corners_bounds` should be on local surface coordinates
    // but the deprecated implementation still uses root surface coordinates.
    // If so, we skip translating into the root surface coordinates to keep the
    // old behavior.
    // TODO(crbug.com/1470955): Remove this.
    auto rounded_corners_rect_offset =
        state_.rounded_corners_is_root_coordinates ? gfx::Vector2d()
                                                   : origin.OffsetFromOrigin();

    // Set the mask.
    msk = gfx::MaskFilterInfo(state_.rounded_corners_bounds +
                              rounded_corners_rect_offset);

    if (device_scale_factor.has_value()) {
      msk.ApplyTransform(
          gfx::Transform::MakeScale(device_scale_factor.value()));
    }
  }

  // Compute the total transformation from post-transform buffer coordinates to
  // target coordinates.
  // Scale and offset the normalized space to fit the content size rectangle.
  gfx::Transform viewport_to_target_transform(
      gfx::AxisTransform2d::FromScaleAndTranslation(
          scale, origin.OffsetFromOrigin() + translate));
  viewport_to_target_transform.PostConcat(state_.surface_transform);

  if (device_scale_factor.has_value()) {
    // Convert from DPs to pixels.
    viewport_to_target_transform.PostScale(device_scale_factor.value());
  }

  gfx::Transform quad_to_target_transform(buffer_transform_);
  quad_to_target_transform.PostConcat(viewport_to_target_transform);

  // The overdraw algorithm in 'Display::RemoveOverdrawQuads' operates in
  // content space and, due to the discretized nature of the |gfx::Rect|, cannot
  // work with 0,0 1x1 quads. This also means that quads that do not fall on
  // pixel boundaries (rotated or subpixel rects) cannot be removed by the
  // algorithm.
  if (quad_to_target_transform.Preserves2dAxisAlignment()) {
    gfx::RectF target_space_rect =
        quad_to_target_transform.MapRect(gfx::RectF(quad_rect));
    // This simple rect representation cannot mathematically express a rotation
    // (and currently does not express flip/mirror) hence the
    // 'IsPositiveScaleOrTranslation' check.
    if (gfx::IsNearestRectWithinDistance(target_space_rect, 0.001f) &&
        quad_to_target_transform.IsPositiveScaleOrTranslation()) {
      quad_rect = gfx::ToNearestRect(target_space_rect);
      // Later in 'SurfaceAggregator' this transform will have 2d translation.
      quad_to_target_transform = gfx::Transform();
    }
  }

  if (current_resource_.id) {
    gfx::RectF uv_crop(gfx::SizeF(1, 1));
    if (!state_.basic_state.crop.IsEmpty()) {
      // The crop rectangle is a post-transformation rectangle. To get the UV
      // coordinates, we need to convert it to normalized buffer coordinates and
      // pass them through the inverse of the buffer transformation.
      uv_crop = gfx::RectF(state_.basic_state.crop);
      gfx::Size transformed_buffer_size(ToTransformedSize(
          current_resource_.size, state_.basic_state.buffer_transform));
      if (!transformed_buffer_size.IsEmpty()) {
        uv_crop.InvScale(transformed_buffer_size.width(),
                         transformed_buffer_size.height());
      }
      uv_crop = buffer_transform_.InverseMapRect(uv_crop).value_or(uv_crop);
    }

    SkColor4f background_color = SkColors::kTransparent;
    if (state_.basic_state.background_color.has_value())
      background_color = state_.basic_state.background_color.value();
    else if (current_resource_has_alpha_ && are_contents_opaque)
      background_color = SkColors::kBlack;  // Avoid writing alpha < 1

    // If this surface is being replaced by a SurfaceId emit a SurfaceDrawQuad.
    if (get_current_surface_id_) {
      auto current_surface_id = get_current_surface_id_.Run();
      // If the surface ID is valid update it, otherwise keep showing the old
      // one for now.
      if (current_surface_id.is_valid()) {
        latest_embedded_surface_id_ = current_surface_id;
        if (!current_surface_id.HasSameEmbedTokenAs(
                first_embedded_surface_id_)) {
          first_embedded_surface_id_ = current_surface_id;
        }
      }
      if (latest_embedded_surface_id_.is_valid() &&
          !embedded_surface_size_.IsEmpty()) {
        viz::SharedQuadState* quad_state =
            render_pass->CreateAndAppendSharedQuadState();
        quad_state->SetAll(quad_to_target_transform, quad_rect, quad_rect, msk,
                           quad_clip_rect, are_contents_opaque,
                           state_.basic_state.alpha, SkBlendMode::kSrcOver,
                           /*sorting_context=*/0, /*layer_id=*/0u,
                           /*fast_rounded_corner=*/false);
        if (!state_.basic_state.crop.IsEmpty()) {
          quad_state->clip_rect = gfx::ToEnclosedRect(output_rect);
        }
        viz::SurfaceDrawQuad* surface_quad =
            render_pass->CreateAndAppendDrawQuad<viz::SurfaceDrawQuad>();
        surface_quad->SetNew(quad_state, quad_rect, quad_rect,
                             viz::SurfaceRange(first_embedded_surface_id_,
                                               latest_embedded_surface_id_),
                             background_color,
                             /*stretch_content_to_fill_bounds=*/false);
      }
      // A resource was still produced for this so we still need to release it
      // later.
      frame->resource_list.push_back(current_resource_);
    } else if (state_.basic_state.alpha != 0.0f) {
      const viz::SharedQuadState* quad_state = AppendOrCreateSharedQuadState(
          state_.basic_state.alpha, render_pass, quad_to_target_transform,
          quad_rect, msk, quad_clip_rect, are_contents_opaque);

      // Draw quad is only needed if buffer is not fully transparent.
      const bool requires_texture_draw_quad =
          state_.basic_state.only_visible_on_secure_output ||
          state_.overlay_priority_hint != OverlayPriority::LOW;

      if (requires_texture_draw_quad) {
        viz::TextureDrawQuad* texture_quad =
            render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
        float vertex_opacity[4] = {1.0, 1.0, 1.0, 1.0};
        texture_quad->SetNew(
            quad_state, quad_rect, quad_rect,
            /* needs_blending=*/!are_contents_opaque, current_resource_.id,
            /* premultiplied*/ true, uv_crop.origin(), uv_crop.bottom_right(),
            background_color, vertex_opacity,
            /* flipped=*/false, /* nearest*/ false,
            state_.basic_state.only_visible_on_secure_output,
            gfx::ProtectedVideoType::kClear);
        if (current_resource_.is_overlay_candidate)
          texture_quad->set_resource_size_in_pixels(current_resource_.size);

        switch (state_.overlay_priority_hint) {
          case OverlayPriority::LOW:
            texture_quad->overlay_priority_hint = viz::OverlayPriority::kLow;
            break;
          case OverlayPriority::REQUIRED:
            texture_quad->overlay_priority_hint =
                viz::OverlayPriority::kRequired;
            break;
          case OverlayPriority::REGULAR:
            texture_quad->overlay_priority_hint =
                viz::OverlayPriority::kRegular;
            break;
        }

#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
        if (state_.basic_state.only_visible_on_secure_output &&
            state_.buffer.has_value() && state_.buffer->buffer() &&
            state_.buffer->buffer()->NeedsHardwareProtection()) {
          texture_quad->protected_video_type =
              gfx::ProtectedVideoType::kHardwareProtected;
        }
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

        if (!damage_rect.IsEmpty()) {
          texture_quad->damage_rect = gfx::ToEnclosedRect(damage_rect);
          render_pass->has_per_quad_damage = true;
          // Clear handled damage so it will not be added to the |render_pass|.
          damage_rect = gfx::RectF();
        }
      } else {
        viz::TileDrawQuad* tile_quad =
            render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
        // TODO(crbug.com/1339335): Support AA quads coming from exo.
        constexpr bool kForceAntiAliasingOff = true;
        tile_quad->SetNew(
            quad_state, quad_rect, quad_rect,
            /* needs_blending=*/!are_contents_opaque, current_resource_.id,
            gfx::ScaleRect(uv_crop, current_resource_.size.width(),
                           current_resource_.size.height()),
            current_resource_.size,
            /* is_premultiplied=*/true,
            /* nearest_neighbor */ false, kForceAntiAliasingOff);
      }
      frame->resource_list.push_back(current_resource_);
    }
  } else {
    const viz::SharedQuadState* quad_state = AppendOrCreateSharedQuadState(
        state_.basic_state.alpha, render_pass, quad_to_target_transform,
        quad_rect, msk, quad_clip_rect, are_contents_opaque);
    SkColor4f color = state_.buffer.has_value() && state_.buffer->buffer()
                          ? state_.buffer->buffer()->GetColor()
                          : SkColors::kBlack;
    viz::SolidColorDrawQuad* solid_quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    solid_quad->SetNew(quad_state, quad_rect, quad_rect, color,
                       false /* force_anti_aliasing_off */);
  }

  render_pass->damage_rect.Union(gfx::ToEnclosedRect(damage_rect));
}

void Surface::UpdateContentSize() {
  gfx::SizeF content_size;
  // Enable/disable sub-surface based on if it has contents.
  if (has_contents()) {
    if (!state_.basic_state.viewport.IsEmpty()) {
      content_size = state_.basic_state.viewport;
    } else if (!state_.basic_state.crop.IsEmpty()) {
      DLOG_IF(WARNING, !base::IsValueInRangeForNumericType<int>(
                           state_.basic_state.crop.width()) ||
                           !base::IsValueInRangeForNumericType<int>(
                               state_.basic_state.crop.height()))
          << "Crop rectangle size ("
          << state_.basic_state.crop.size().ToString()
          << ") most be expressible using integers when viewport is not set";
      content_size = state_.basic_state.crop.size();
    } else {
      content_size = gfx::ScaleSize(
          gfx::SizeF(ToTransformedSize(
              state_.buffer.has_value() ? state_.buffer->size() : gfx::Size(),
              state_.basic_state.buffer_transform)),
          1.0f / state_.basic_state.buffer_scale);
    }

    // Check that a window has a parent before showing it.
    // For example, aura::Window associated with augmented subsurfaces don't
    // have parents, because they are not part of the tree.
    if (window_->parent()) {
      window_->Show();
    }
  } else {
    window_->Hide();
  }

  if (content_size_ != content_size) {
    content_size_ = content_size;
    // TODO(b/191414141) : Check is temporary to isolate damage issue.
    if (!gfx::ToRoundedSize(content_size_).GetCheckedArea().IsValid()) {
      DCHECK(false) << " content_size_=" << content_size_.ToString();
      constexpr int kMaxSizeScalar = 1 << 15;
      // Forceably restrict |content_size_| to 32kx32k.
      content_size_.SetToMin(gfx::SizeF(kMaxSizeScalar, kMaxSizeScalar));
    }
    window_->SetBounds(gfx::Rect(window_->bounds().origin(),
                                 gfx::ToCeiledSize(content_size_)));

    for (SurfaceObserver& observer : observers_)
      observer.OnContentSizeChanged(this);
  }
}

void Surface::SetFrameLocked(bool lock) {
  for (SurfaceObserver& observer : observers_)
    observer.OnFrameLockingChanged(this, lock);
}

void Surface::OnScaleFactorChanged(float old_scale_factor,
                                   float new_scale_factor) {
  for (SurfaceObserver& observer : observers_) {
    observer.OnScaleFactorChanged(this, old_scale_factor, new_scale_factor);
  }
}

void Surface::OnWindowOcclusionChanged(
    aura::Window::OcclusionState old_occlusion_state,
    aura::Window::OcclusionState new_occlusion_state) {
  if (!state_.basic_state.is_tracking_occlusion)
    return;

  // The first occlusion calculation happens without a buffer yet attached to
  // the surface so ignore this change. This avoids `OcclusionState::HIDDEN`
  // being sent , which will be immediately followed by
  // `OcclusionState::VISIBLE` anyway once buffer is attached.
  if (old_occlusion_state == aura::Window::OcclusionState::UNKNOWN &&
      new_occlusion_state == aura::Window::OcclusionState::HIDDEN) {
    return;
  }

  for (SurfaceObserver& observer : observers_)
    observer.OnWindowOcclusionChanged(this);
}

void Surface::OnDeskChanged(int state) {
  for (SurfaceObserver& observer : observers_)
    observer.OnDeskChanged(this, state);
}

void Surface::OnTooltipShown(const std::u16string& text,
                             const gfx::Rect& bounds) {
  for (SurfaceObserver& observer : observers_)
    observer.OnTooltipShown(this, text, bounds);
}

void Surface::OnTooltipHidden() {
  for (SurfaceObserver& observer : observers_)
    observer.OnTooltipHidden(this);
}

void Surface::MoveToDesk(int desk_index) {
  if (delegate_)
    delegate_->MoveToDesk(desk_index);
}

void Surface::SetVisibleOnAllWorkspaces() {
  if (delegate_)
    delegate_->SetVisibleOnAllWorkspaces();
}

void Surface::SetInitialWorkspace(const char* initial_workspace) {
  if (delegate_)
    delegate_->SetInitialWorkspace(initial_workspace);
}

void Surface::Pin(bool trusted) {
  if (delegate_)
    delegate_->Pin(trusted);
}

void Surface::Unpin() {
  if (delegate_)
    delegate_->Unpin();
}

void Surface::ThrottleFrameRate(bool on) {
  for (SurfaceObserver& observer : observers_)
    observer.ThrottleFrameRate(on);
}

void Surface::SetKeyboardShortcutsInhibited(bool inhibited) {
  if (keyboard_shortcuts_inhibited_ == inhibited)
    return;

  keyboard_shortcuts_inhibited_ = inhibited;

  // Also set kCanConsumeSystemKeysKey property, so that the key event
  // is also forwarded to exo::Keyboard.
  // TODO(hidehiko): Support capability on migrating ARC/Crostini.
  window_->SetProperty(ash::kCanConsumeSystemKeysKey, inhibited);
}

SecurityDelegate* Surface::GetSecurityDelegate() {
  if (delegate_)
    return delegate_->GetSecurityDelegate();
  return nullptr;
}

void Surface::SetClientAccessibilityId(int id) {
  if (!window_) {
    return;
  }

  if (id >= 0) {
    exo::SetShellClientAccessibilityId(window_.get(), id);
  } else {
    exo::SetShellClientAccessibilityId(window_.get(), absl::nullopt);
  }
}

void Surface::SetTopInset(int height) {
  if (delegate_) {
    delegate_->SetTopInset(height);
  }
}

void Surface::OnFullscreenStateChanged(bool fullscreen) {
  for (SurfaceObserver& observer : observers_) {
    observer.OnFullscreenStateChanged(fullscreen);
  }
  for (const auto& [surface, point] : sub_surfaces_) {
    surface->OnFullscreenStateChanged(fullscreen);
  }
}

Buffer* Surface::GetBuffer() {
  if (state_.buffer.has_value()) {
    return state_.buffer->buffer().get();
  }
  return nullptr;
}

}  // namespace exo
