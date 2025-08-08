// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"

#include "cc/paint/paint_canvas.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

class ScopedScrollbarPainter {
  STACK_ALLOCATED();

 public:
  explicit ScopedScrollbarPainter(cc::PaintCanvas& canvas) : canvas_(canvas) {}
  ~ScopedScrollbarPainter() { canvas_.drawPicture(builder_->EndRecording()); }

  GraphicsContext& Context() { return builder_->Context(); }

 private:
  cc::PaintCanvas& canvas_;
  PaintRecordBuilder* builder_ = MakeGarbageCollected<PaintRecordBuilder>();
};

}  // namespace

ScrollbarLayerDelegate::ScrollbarLayerDelegate(blink::Scrollbar& scrollbar)
    : scrollbar_(&scrollbar) {
  // Custom scrollbars are either non-composited or use cc::PictureLayers
  // which don't need ScrollbarLayerDelegate.
  DCHECK(!scrollbar.IsCustomScrollbar());
}

ScrollbarLayerDelegate::~ScrollbarLayerDelegate() = default;

bool ScrollbarLayerDelegate::IsSame(const cc::Scrollbar& other) const {
  return scrollbar_.Get() ==
         static_cast<const ScrollbarLayerDelegate&>(other).scrollbar_.Get();
}

cc::ScrollbarOrientation ScrollbarLayerDelegate::Orientation() const {
  if (scrollbar_->Orientation() == kHorizontalScrollbar)
    return cc::ScrollbarOrientation::kHorizontal;
  return cc::ScrollbarOrientation::kVertical;
}

bool ScrollbarLayerDelegate::IsLeftSideVerticalScrollbar() const {
  return scrollbar_->IsLeftSideVerticalScrollbar();
}

bool ScrollbarLayerDelegate::HasThumb() const {
  return scrollbar_->GetTheme().HasThumb(*scrollbar_);
}

bool ScrollbarLayerDelegate::IsSolidColor() const {
  return scrollbar_->GetTheme().IsSolidColor();
}

SkColor4f ScrollbarLayerDelegate::GetSolidColor() const {
  return scrollbar_->GetTheme().GetSolidColor(
      scrollbar_->ScrollbarThumbColor());
}

bool ScrollbarLayerDelegate::IsOverlay() const {
  return scrollbar_->IsOverlayScrollbar();
}

bool ScrollbarLayerDelegate::IsFluentOverlayScrollbarMinimalMode() const {
  return scrollbar_->IsFluentOverlayScrollbarMinimalMode();
}

gfx::Rect ScrollbarLayerDelegate::ShrinkMainThreadedMinimalModeThumbRect(
    gfx::Rect& rect) const {
  return scrollbar_->GetTheme().ShrinkMainThreadedMinimalModeThumbRect(
      *scrollbar_, rect);
}

gfx::Rect ScrollbarLayerDelegate::ThumbRect() const {
  gfx::Rect thumb_rect = scrollbar_->GetTheme().ThumbRect(*scrollbar_);
  thumb_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return thumb_rect;
}

gfx::Rect ScrollbarLayerDelegate::TrackRect() const {
  gfx::Rect track_rect = scrollbar_->GetTheme().TrackRect(*scrollbar_);
  track_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return track_rect;
}

bool ScrollbarLayerDelegate::SupportsDragSnapBack() const {
  return scrollbar_->GetTheme().SupportsDragSnapBack();
}

bool ScrollbarLayerDelegate::JumpOnTrackClick() const {
  return scrollbar_->GetTheme().JumpOnTrackClick();
}

bool ScrollbarLayerDelegate::IsOpaque() const {
  return scrollbar_->IsOpaque();
}

gfx::Rect ScrollbarLayerDelegate::BackButtonRect() const {
  gfx::Rect back_button_rect =
      scrollbar_->GetTheme().BackButtonRect(*scrollbar_);
  if (!back_button_rect.IsEmpty())
    back_button_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return back_button_rect;
}

gfx::Rect ScrollbarLayerDelegate::ForwardButtonRect() const {
  gfx::Rect forward_button_rect =
      scrollbar_->GetTheme().ForwardButtonRect(*scrollbar_);
  if (!forward_button_rect.IsEmpty())
    forward_button_rect.Offset(-scrollbar_->Location().OffsetFromOrigin());
  return forward_button_rect;
}

float ScrollbarLayerDelegate::Opacity() const {
  return scrollbar_->GetTheme().Opacity(*scrollbar_);
}

bool ScrollbarLayerDelegate::NeedsRepaintPart(cc::ScrollbarPart part) const {
  if (part == cc::ScrollbarPart::kThumb) {
    return scrollbar_->ThumbNeedsRepaint();
  }
  return scrollbar_->TrackNeedsRepaint();
}

bool ScrollbarLayerDelegate::NeedsUpdateDisplay() const {
  return scrollbar_->NeedsUpdateDisplay();
}

void ScrollbarLayerDelegate::ClearNeedsUpdateDisplay() {
  scrollbar_->ClearNeedsUpdateDisplay();
}

bool ScrollbarLayerDelegate::UsesNinePatchThumbResource() const {
  return scrollbar_->GetTheme().UsesNinePatchThumbResource();
}

gfx::Size ScrollbarLayerDelegate::NinePatchThumbCanvasSize() const {
  DCHECK(scrollbar_->GetTheme().UsesNinePatchThumbResource());
  return scrollbar_->GetTheme().NinePatchThumbCanvasSize(*scrollbar_);
}

gfx::Rect ScrollbarLayerDelegate::NinePatchThumbAperture() const {
  DCHECK(scrollbar_->GetTheme().UsesNinePatchThumbResource());
  return scrollbar_->GetTheme().NinePatchThumbAperture(*scrollbar_);
}

bool ScrollbarLayerDelegate::ShouldPaint() const {
  // TODO(crbug.com/860499): Remove this condition, it should not occur.
  // Layers may exist and be painted for a |scrollbar_| that has had its
  // ScrollableArea detached. This seems weird because if the area is detached
  // the layer should be destroyed but here we are. https://crbug.com/860499.
  if (!scrollbar_->GetScrollableArea())
    return false;
  // When the frame is throttled, the scrollbar will not be painted because
  // the frame has not had its lifecycle updated. Thus the actual value of
  // HasTickmarks can't be known and may change once the frame is unthrottled.
  if (scrollbar_->GetScrollableArea()->IsThrottled())
    return false;
  return true;
}

bool ScrollbarLayerDelegate::HasTickmarks() const {
  return ShouldPaint() && scrollbar_->HasTickmarks();
}

void ScrollbarLayerDelegate::PaintPart(cc::PaintCanvas* canvas,
                                       cc::ScrollbarPart part,
                                       const gfx::Rect& rect) {
  if (!ShouldPaint())
    return;

  auto& theme = scrollbar_->GetTheme();
  ScopedScrollbarPainter painter(*canvas);
  // The canvas coordinate space is relative to the part's origin.
  switch (part) {
    case cc::ScrollbarPart::kThumb:
      theme.PaintThumb(painter.Context(), *scrollbar_, gfx::Rect(rect));
      scrollbar_->ClearThumbNeedsRepaint();
      break;
    case cc::ScrollbarPart::kTrackButtonsTickmarks: {
      DCHECK_EQ(rect.size(), scrollbar_->FrameRect().size());
      gfx::Vector2d offset = rect.origin() - scrollbar_->FrameRect().origin();
      theme.PaintTrackButtonsTickmarks(painter.Context(), *scrollbar_, offset);
      scrollbar_->ClearTrackNeedsRepaint();
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace blink
