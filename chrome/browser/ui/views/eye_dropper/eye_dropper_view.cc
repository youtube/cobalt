// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/eye_dropper/eye_dropper_view.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class EyeDropperView::ViewPositionHandler {
 public:
  explicit ViewPositionHandler(EyeDropperView* owner);
  ViewPositionHandler(const ViewPositionHandler&) = delete;
  ViewPositionHandler& operator=(const ViewPositionHandler&) = delete;
  ~ViewPositionHandler();

 private:
  void UpdateViewPosition();

  // Timer used for updating the window location.
  base::RepeatingTimer timer_;

  raw_ptr<EyeDropperView> owner_;
};

EyeDropperView::ViewPositionHandler::ViewPositionHandler(EyeDropperView* owner)
    : owner_(owner) {
  // TODO(iopopesc): Use SetCapture instead of a timer when support for
  // activating the eye dropper without closing the color popup is added.
  timer_.Start(FROM_HERE, base::Hertz(60), this,
               &EyeDropperView::ViewPositionHandler::UpdateViewPosition);
}

EyeDropperView::ViewPositionHandler::~ViewPositionHandler() {
  timer_.AbandonAndStop();
}

void EyeDropperView::ViewPositionHandler::UpdateViewPosition() {
  owner_->UpdatePosition();
}

class EyeDropperView::ScreenCapturer
    : public webrtc::DesktopCapturer::Callback {
 public:
  ScreenCapturer();
  ScreenCapturer(const ScreenCapturer&) = delete;
  ScreenCapturer& operator=(const ScreenCapturer&) = delete;
  ~ScreenCapturer() override = default;

  // webrtc::DesktopCapturer::Callback:
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  SkBitmap GetBitmap() const;
  SkColor GetColor(int x, int y) const;
  int original_offset_x() const;
  int original_offset_y() const;

 private:
  std::unique_ptr<webrtc::DesktopCapturer> capturer_;
  SkBitmap frame_;
  int original_offset_x_;
  int original_offset_y_;
};

EyeDropperView::ScreenCapturer::ScreenCapturer() {
  // TODO(iopopesc): Update the captured frame after a period of time to match
  // latest content on screen.
  capturer_ = content::desktop_capture::CreateScreenCapturer();
  if (capturer_) {
    capturer_->Start(this);
    capturer_->CaptureFrame();
  }
}

void EyeDropperView::ScreenCapturer::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  if (result != webrtc::DesktopCapturer::Result::SUCCESS)
    return;

  frame_.allocN32Pixels(frame->size().width(), frame->size().height(), true);
  memcpy(frame_.getAddr32(0, 0), frame->data(),
         frame->size().height() * frame->stride());
  frame_.setImmutable();

  // The captured frame is in full desktop coordinates. E.g. the top left
  // monitor should start from (0, 0), so we need to compute the correct
  // origins.
  original_offset_x_ = 0;
  original_offset_y_ = 0;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
#if BUILDFLAG(IS_WIN)
    // The window parameter is intentionally passed as nullptr on Windows
    // because a non-null window parameter causes errors when restoring windows
    // to saved positions in variable-DPI situations. See
    // https://crbug.com/1224715 for details.
    gfx::Rect scaled_bounds =
        display::Screen::GetScreen()->DIPToScreenRectInWindow(
            /*window=*/nullptr, display.bounds());
#else
    gfx::Rect scaled_bounds = gfx::ScaleToEnclosingRect(
        display.bounds(), display.device_scale_factor());
#endif
    if (scaled_bounds.origin().x() < original_offset_x_) {
      original_offset_x_ = scaled_bounds.origin().x();
    }
    if (scaled_bounds.origin().y() < original_offset_y_) {
      original_offset_y_ = scaled_bounds.origin().y();
    }
  }
}

SkBitmap EyeDropperView::ScreenCapturer::GetBitmap() const {
  return frame_;
}

SkColor EyeDropperView::ScreenCapturer::GetColor(int x, int y) const {
  // It's not clear how control can reach here with out-of-bounds coordinates,
  // but avoid a crash if it does.
  return (x < 0 || x >= frame_.width() || y < 0 || y >= frame_.height())
             ? gfx::kPlaceholderColor
             : frame_.getColor(x, y);
}

int EyeDropperView::ScreenCapturer::original_offset_x() const {
  return original_offset_x_;
}

int EyeDropperView::ScreenCapturer::original_offset_y() const {
  return original_offset_y_;
}

EyeDropperView::EyeDropperView(content::RenderFrameHost* frame,
                               content::EyeDropperListener* listener)
    : render_frame_host_(frame),
      listener_(listener),
      view_position_handler_(std::make_unique<ViewPositionHandler>(this)),
      screen_capturer_(std::make_unique<ScreenCapturer>()) {
  SetModalType(ui::MODAL_TYPE_WINDOW);
  // This is owned as a unique_ptr<EyeDropper> elsewhere.
  SetOwnedByWidget(false);
  // TODO(pbos): Remove this, perhaps by separating the contents view from the
  // EyeDropper/WidgetDelegate.
  set_owned_by_client();
  SetPreferredSize(GetSize());
#if BUILDFLAG(IS_LINUX)
  // Use TYPE_MENU for Linux to ensure that the eye dropper view is displayed
  // above the color picker.
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_MENU);
#else
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
#endif
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // Use software compositing to prevent situations when the widget is not
  // translucent when moved fast.
  // TODO(iopopesc): Investigate if this is a compositor bug or this is indeed
  // an intentional limitation.
  params.force_software_compositing = true;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.name = "MagnifierHost";
  params.parent = content::WebContents::FromRenderFrameHost(render_frame_host_)
                      ->GetNativeView();
  params.delegate = this;
  views::Widget* widget = new views::Widget();
  widget->Init(std::move(params));
  widget->SetContentsView(this);
  MoveViewToFront();
  HideCursor();
  pre_dispatch_handler_ = std::make_unique<PreEventDispatchHandler>(
      this, content::WebContents::FromRenderFrameHost(render_frame_host_)
                ->GetNativeView());
  widget->Show();
  CaptureInputIfNeeded();
  // The ignore selection time should be long enough to allow the user to see
  // the UI.
  ignore_selection_time_ = base::TimeTicks::Now() + base::Milliseconds(500);
}

EyeDropperView::~EyeDropperView() {
  if (GetWidget())
    GetWidget()->CloseNow();
}

void EyeDropperView::OnPaint(gfx::Canvas* view_canvas) {
  if (screen_capturer_->GetBitmap().drawsNothing())
    return;

  const float diameter = GetDiameter();
  constexpr float kPixelSize = 10;
  const gfx::Size padding((size().width() - diameter) / 2,
                          (size().height() - diameter) / 2);

  if (GetWidget()->IsTranslucentWindowOpacitySupported()) {
    // Clip circle for magnified projection only when the widget
    // supports translucency.
    SkPath clip_path;
    clip_path.addOval(SkRect::MakeXYWH(padding.width(), padding.height(),
                                       diameter, diameter));
    clip_path.close();
    view_canvas->ClipPath(clip_path, true);
  }

  // Project pixels.
  const int pixel_count = diameter / kPixelSize;
  const SkBitmap frame = screen_capturer_->GetBitmap();
  // The captured frame is not scaled so we need to use widget's bounds in
  // pixels to have the magnified region match cursor position.
  gfx::Point center_position =
      display::Screen::GetScreen()
          ->DIPToScreenRectInWindow(GetWidget()->GetNativeWindow(),
                                    GetWidget()->GetWindowBoundsInScreen())
          .CenterPoint();
  center_position.Offset(-screen_capturer_->original_offset_x(),
                         -screen_capturer_->original_offset_y());
  view_canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(frame),
                            center_position.x() - pixel_count / 2,
                            center_position.y() - pixel_count / 2, pixel_count,
                            pixel_count, padding.width(), padding.height(),
                            diameter, diameter, false);

  // Store the pixel color under the cursor as it is the last color seen
  // by the user before selection.
  selected_color_ =
      screen_capturer_->GetColor(center_position.x(), center_position.y());

  // Paint grid.
  const auto* color_provider = GetColorProvider();
  cc::PaintFlags flags;
  flags.setStrokeWidth(1);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(color_provider->GetColor(kColorEyedropperGrid));
  for (int i = 0; i < pixel_count; ++i) {
    view_canvas->DrawLine(
        gfx::PointF(padding.width() + i * kPixelSize, padding.height()),
        gfx::PointF(padding.width() + i * kPixelSize,
                    size().height() - padding.height()),
        flags);
    view_canvas->DrawLine(
        gfx::PointF(padding.width(), padding.height() + i * kPixelSize),
        gfx::PointF(size().width() - padding.width(),
                    padding.height() + i * kPixelSize),
        flags);
  }

  // Paint central pixel.
  gfx::RectF pixel((size().width() - kPixelSize) / 2,
                   (size().height() - kPixelSize) / 2, kPixelSize, kPixelSize);
  flags.setAntiAlias(true);
  flags.setColor(
      color_provider->GetColor(kColorEyedropperCentralPixelOuterRing));
  flags.setStrokeWidth(2);
  pixel.Inset(-0.5f);
  view_canvas->DrawRect(pixel, flags);
  flags.setColor(
      color_provider->GetColor(kColorEyedropperCentralPixelInnerRing));
  flags.setStrokeWidth(1);
  pixel.Inset(0.5f);
  view_canvas->DrawRect(pixel, flags);

  // Paint outline.
  flags.setStrokeWidth(2);
  flags.setColor(color_provider->GetColor(kColorEyedropperBoundary));
  flags.setAntiAlias(true);
  if (GetWidget()->IsTranslucentWindowOpacitySupported()) {
    view_canvas->DrawCircle(
        gfx::PointF(size().width() / 2, size().height() / 2), diameter / 2,
        flags);
  } else {
    view_canvas->DrawRect(bounds(), flags);
  }

  OnPaintBorder(view_canvas);
}

void EyeDropperView::WindowClosing() {
  ShowCursor();
  pre_dispatch_handler_.reset();
}

void EyeDropperView::OnWidgetMove() {
  // Trigger a repaint since because the widget was moved, the content of the
  // view needs to be updated.
  SchedulePaint();
}

void EyeDropperView::UpdatePosition() {
  if (screen_capturer_->GetBitmap().drawsNothing() || !GetWidget())
    return;

  gfx::Point cursor_position =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  if (cursor_position == GetWidget()->GetWindowBoundsInScreen().CenterPoint())
    return;

  GetWidget()->SetBounds(
      gfx::Rect(gfx::Point(cursor_position.x() - size().width() / 2,
                           cursor_position.y() - size().height() / 2),
                size()));
}

void EyeDropperView::OnColorSelected() {
  if (!selected_color_.has_value()) {
    listener_->ColorSelectionCanceled();
    return;
  }

  // Prevent the user from selecting a color for a period of time.
  if (base::TimeTicks::Now() <= ignore_selection_time_)
    return;

  // Use the last selected color and notify listener.
  listener_->ColorSelected(selected_color_.value());
}

void EyeDropperView::OnColorSelectionCanceled() {
  listener_->ColorSelectionCanceled();
}

BEGIN_METADATA(EyeDropperView, views::WidgetDelegateView)
ADD_READONLY_PROPERTY_METADATA(gfx::Size, Size)
ADD_READONLY_PROPERTY_METADATA(float, Diameter)
END_METADATA
