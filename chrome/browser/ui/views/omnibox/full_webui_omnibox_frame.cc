// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/full_webui_omnibox_frame.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#endif

class ResultsViewTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  explicit ResultsViewTargeterDelegate(RoundedOmniboxResultsFrame* frame)
      : frame_(frame) {}

  ResultsViewTargeterDelegate(const ResultsViewTargeterDelegate&) = delete;
  ResultsViewTargeterDelegate& operator=(const ResultsViewTargeterDelegate&) =
      delete;
  ~ResultsViewTargeterDelegate() override = default;

  views::View* TargetForRect(views::View* root,
                             const gfx::Rect& rect) override {
    if (frame_->forward_mouse_events()) {
      int top_inset =
          frame_->GetInsets().top() +
          RoundedOmniboxResultsFrame::GetLocationBarAlignmentInsets().top() +
          GetLayoutConstant(LayoutConstant::kLocationBarHeight);
      if (rect.y() < top_inset) {
        return root;
      }
    }
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }

 private:
  raw_ptr<RoundedOmniboxResultsFrame> frame_;
};

FullWebUIOmniboxFrame::FullWebUIOmniboxFrame(views::View* contents,
                                             LocationBar* location_bar,
                                             bool forward_mouse_events)
    : RoundedOmniboxResultsFrame(contents, location_bar, forward_mouse_events) {
}

FullWebUIOmniboxFrame::~FullWebUIOmniboxFrame() = default;

void FullWebUIOmniboxFrame::SetElevation(int elevation) {
  const int corner_radius = views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::ShapeContextTokens::kOmniboxExpandedRadius);
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::Arrow::NONE,
      elevation == 0 ? views::BubbleBorder::Shadow::NO_SHADOW
                     : views::BubbleBorder::Shadow::STANDARD_SHADOW);
  border->set_rounded_corners(gfx::RoundedCornersF(corner_radius));
  if (elevation > 0) {
    border->set_md_shadow_elevation(elevation);
  }
  SetBorder(std::move(border));
}

void FullWebUIOmniboxFrame::SetForwardMouseEvents(bool forward) {
  set_forward_mouse_events(forward);
#if defined(USE_AURA)
  if (GetWidget() && GetWidget()->GetNativeWindow()) {
    if (forward_mouse_events()) {
      // Use a ui::EventTargeter that allows mouse and touch events in the top
      // portion of the Widget to pass through to the omnibox beneath it.
      auto results_targeter = std::make_unique<aura::WindowTargeter>();
      results_targeter->SetInsets(GetEventForwardingInsets());
      GetWidget()->GetNativeWindow()->SetEventTargeter(
          std::move(results_targeter));
    } else {
      GetWidget()->GetNativeWindow()->SetEventTargeter(nullptr);
    }
  }
#endif
}

void FullWebUIOmniboxFrame::AddedToWidget() {
#if defined(USE_AURA)
  if (!forward_mouse_events()) {
    return;
  }
  // Use a ui::EventTargeter that allows mouse and touch events in the top
  // portion of the Widget to pass through to the omnibox beneath it.
  auto results_targeter = std::make_unique<aura::WindowTargeter>();
  gfx::Insets insets = GetEventForwardingInsets();
  results_targeter->SetInsets(insets);
  GetWidget()->GetNativeWindow()->SetEventTargeter(std::move(results_targeter));
#else
  SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<ResultsViewTargeterDelegate>(this)));
#endif  // USE_AURA
}

// Note: The OnMouseMoved function is only called for the shadow area, as mouse-
// moved events are not dispatched through the view hierarchy but are direct-
// dispatched by RootView. This OnMouseEvent function is on the dispatch path
// for all mouse events of the window, so be careful to correctly mark events as
// "handled" above in subviews.
#if !defined(USE_AURA)

void FullWebUIOmniboxFrame::OnMouseEvent(ui::MouseEvent* event) {
  RoundedOmniboxResultsFrame::OnMouseEvent(event);
  if (forward_mouse_events()) {
    event->SetHandled();
  }
}

#endif  // !USE_AURA

gfx::Insets FullWebUIOmniboxFrame::GetEventForwardingInsets() {
  int top_inset = GetInsets().top() + GetLocationBarAlignmentInsets().top() +
                  GetLayoutConstant(LayoutConstant::kLocationBarHeight);
  gfx::Insets insets = gfx::Insets::TLBR(top_inset, 0, 0, 0);
  return insets;
}

BEGIN_METADATA(FullWebUIOmniboxFrame)
END_METADATA
