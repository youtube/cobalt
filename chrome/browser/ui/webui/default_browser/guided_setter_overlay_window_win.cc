// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/guided_setter_overlay_window_win.h"

#include <utility>

#include "cc/paint/paint_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

class OverlayArrowView : public views::View {
  METADATA_HEADER(OverlayArrowView, views::View)
 public:
  OverlayArrowView() = default;
  OverlayArrowView(const OverlayArrowView&) = delete;
  OverlayArrowView& operator=(const OverlayArrowView&) = delete;
  ~OverlayArrowView() override = default;

  void SetEndpoints(const gfx::Point& start, const gfx::Point& end) {
    start_ = start;
    end_ = end;
    SchedulePaint();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    // TODO(https://crbug.com/454597786): Load the SVG arrow from Figma.
  }

 private:
  gfx::Point start_;
  gfx::Point end_;
};

BEGIN_METADATA(OverlayArrowView)
END_METADATA

}  // namespace

GuidedSetterOverlayWindowWin::GuidedSetterOverlayWindowWin(
    gfx::NativeWindow parent_context) {
  CreateWidget(parent_context);
}

GuidedSetterOverlayWindowWin::~GuidedSetterOverlayWindowWin() {
  arrow_view_ = nullptr;
  widget_.reset();
}

void GuidedSetterOverlayWindowWin::Hide() {
  widget_->Hide();
}

void GuidedSetterOverlayWindowWin::UpdateAndShow(const gfx::Rect& bounds_screen,
                                                 const gfx::Point& start_screen,
                                                 const gfx::Point& end_screen) {
  widget_->SetBounds(bounds_screen);

  gfx::Point start_local = start_screen - bounds_screen.OffsetFromOrigin();
  gfx::Point end_local = end_screen - bounds_screen.OffsetFromOrigin();

  static_cast<OverlayArrowView*>(arrow_view_.get())
      ->SetEndpoints(start_local, end_local);

  widget_->ShowInactive();
}

void GuidedSetterOverlayWindowWin::CreateWidget(
    gfx::NativeWindow parent_context) {
  widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.accept_events = false;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.show_state = ui::mojom::WindowShowState::kInactive;
  params.context = parent_context;
  params.name = "GuidedSetterOverlay";

  widget_->Init(std::move(params));

  arrow_view_ = widget_->SetContentsView(std::make_unique<OverlayArrowView>());
}
