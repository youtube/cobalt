// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_H_
#define CC_TEST_FAKE_SCROLLBAR_H_

#include "base/compiler_specific.h"
#include "cc/input/scrollbar.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

class FakeScrollbar : public Scrollbar {
 public:
  FakeScrollbar();

  FakeScrollbar(const FakeScrollbar&) = delete;
  FakeScrollbar& operator=(const FakeScrollbar&) = delete;

  // Scrollbar implementation.
  bool IsSame(const Scrollbar&) const override;
  ScrollbarOrientation Orientation() const override;
  bool IsLeftSideVerticalScrollbar() const override;
  bool IsSolidColor() const override;
  SkColor4f GetSolidColor() const override;
  bool IsOverlay() const override;
  bool IsFluentOverlayScrollbarMinimalMode() const override;
  bool HasThumb() const override;
  gfx::Rect ThumbRect() const override;
  gfx::Rect BackButtonRect() const override;
  gfx::Rect ForwardButtonRect() const override;
  bool JumpOnTrackClick() const override;
  bool SupportsDragSnapBack() const override;
  gfx::Rect TrackRect() const override;
  float Opacity() const override;
  bool NeedsRepaintPart(ScrollbarPart part) const override;
  bool NeedsUpdateDisplay() const override;
  void ClearNeedsUpdateDisplay() override;
  bool HasTickmarks() const override;
  void PaintPart(PaintCanvas* canvas,
                 ScrollbarPart part,
                 const gfx::Rect& rect) override;
  bool UsesNinePatchThumbResource() const override;
  gfx::Size NinePatchThumbCanvasSize() const override;
  gfx::Rect NinePatchThumbAperture() const override;
  gfx::Rect ShrinkMainThreadedMinimalModeThumbRect(
      gfx::Rect& rect) const override;
  bool IsOpaque() const override;

  void set_should_paint(bool b) { should_paint_ = b; }
  void set_has_thumb(bool b) { has_thumb_ = b; }
  void set_has_tickmarks(bool b) { has_tickmarks_ = b; }
  void set_orientation(ScrollbarOrientation o) { orientation_ = o; }
  void set_is_left_side_vertical_scrollbar(bool b) {
    is_left_side_vertical_scrollbar_ = b;
  }
  void set_is_solid_color(bool b) { is_solid_color_ = b; }
  void set_is_overlay(bool b) { is_overlay_ = b; }
  void set_uses_nine_patch_thumb_resource(bool b) {
    uses_nine_patch_thumb_resource_ = b;
  }
  void set_track_rect(const gfx::Rect& track_rect) { track_rect_ = track_rect; }
  void set_thumb_size(const gfx::Size& thumb_size) { thumb_size_ = thumb_size; }
  SkColor paint_fill_color() const { return SK_ColorBLACK | fill_color_; }

  void set_thumb_opacity(float opacity) { thumb_opacity_ = opacity; }
  void set_needs_repaint_thumb(bool needs_repaint) {
    needs_repaint_thumb_ = needs_repaint;
  }
  void set_needs_repaint_track(bool needs_repaint) {
    needs_repaint_track_ = needs_repaint;
  }
  void set_is_opaque(bool b) { is_opaque_ = b; }
  void set_solid_color(SkColor4f color) { solid_color_ = color; }

 protected:
  ~FakeScrollbar() override;

 private:
  bool should_paint_ = false;
  bool has_thumb_ = false;
  bool has_tickmarks_ = false;
  ScrollbarOrientation orientation_ = ScrollbarOrientation::kHorizontal;
  bool is_left_side_vertical_scrollbar_ = false;
  bool is_solid_color_ = false;
  SkColor4f solid_color_ = SkColors::kWhite;
  bool is_overlay_ = false;
  bool uses_nine_patch_thumb_resource_ = false;
  gfx::Size thumb_size_{5, 10};
  float thumb_opacity_ = 1;
  bool needs_repaint_thumb_ = true;
  bool needs_repaint_track_ = true;
  bool needs_update_display_ = true;
  gfx::Rect track_rect_{0, 0, 100, 10};
  gfx::Rect back_button_rect_;
  gfx::Rect forward_button_rect_;
  SkColor fill_color_ = SK_ColorGREEN;
  bool is_opaque_ = true;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_H_
