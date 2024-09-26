// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/painter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char kWidgetNativeViewHostKey[] = "WidgetNativeViewHost";

////////////////////////////////////////////////////////////////////////////////
// NativeViewHost, public:

NativeViewHost::NativeViewHost() {
  set_suppress_default_focus_handling();
}

NativeViewHost::~NativeViewHost() {
  // As part of deleting NativeViewHostWrapper the native view is unparented.
  // Make sure the FocusManager is updated.
  ClearFocus();
}

void NativeViewHost::Attach(gfx::NativeView native_view) {
  DCHECK(native_view);
  DCHECK(!native_view_);
  native_view_ = native_view;
  native_wrapper_->AttachNativeView();
  InvalidateLayout();
  // The call to InvalidateLayout() triggers an async call to Layout(), which
  // updates the visibility of the NativeView. The call to Layout() only happens
  // if |this| is drawn. Call hide if not drawn as otherwise the NativeView
  // could be visible when |this| is not.
  if (!IsDrawn())
    native_wrapper_->HideWidget();

  Widget* widget = Widget::GetWidgetForNativeView(native_view);
  if (widget)
    widget->SetNativeWindowProperty(kWidgetNativeViewHostKey, this);
}

void NativeViewHost::Detach() {
  Detach(false);
}

void NativeViewHost::SetParentAccessible(gfx::NativeViewAccessible accessible) {
  if (!native_wrapper_)
    return;
  native_wrapper_->SetParentAccessible(accessible);
}

gfx::NativeViewAccessible NativeViewHost::GetParentAccessible() {
  if (!native_wrapper_)
    return nullptr;
  return native_wrapper_->GetParentAccessible();
}

bool NativeViewHost::SetCornerRadii(const gfx::RoundedCornersF& corner_radii) {
  return native_wrapper_->SetCornerRadii(corner_radii);
}

bool NativeViewHost::SetCustomMask(std::unique_ptr<ui::LayerOwner> mask) {
  DCHECK(native_wrapper_);
  return native_wrapper_->SetCustomMask(std::move(mask));
}

void NativeViewHost::SetHitTestTopInset(int top_inset) {
  native_wrapper_->SetHitTestTopInset(top_inset);
}

int NativeViewHost::GetHitTestTopInset() const {
  return native_wrapper_->GetHitTestTopInset();
}

void NativeViewHost::SetNativeViewSize(const gfx::Size& size) {
  if (native_view_size_ == size)
    return;
  native_view_size_ = size;
  InvalidateLayout();
}

gfx::NativeView NativeViewHost::GetNativeViewContainer() const {
  return native_view_ ? native_wrapper_->GetNativeViewContainer() : nullptr;
}

void NativeViewHost::NativeViewDestroyed() {
  // Detach so we can clear our state and notify the native_wrapper_ to release
  // ref on the native view.
  Detach(true);
}

void NativeViewHost::SetBackgroundColorWhenClipped(
    absl::optional<SkColor> color) {
  background_color_when_clipped_ = color;
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHost, View overrides:

void NativeViewHost::Layout() {
  if (!native_view_ || !native_wrapper_.get())
    return;

  gfx::Rect vis_bounds = GetVisibleBounds();
  bool visible = !vis_bounds.IsEmpty();

  if (visible && !fast_resize_) {
    if (vis_bounds.size() != size()) {
      // Only a portion of the Widget is really visible.
      int x = vis_bounds.x();
      int y = vis_bounds.y();
      native_wrapper_->InstallClip(x, y, vis_bounds.width(),
                                   vis_bounds.height());
    } else if (native_wrapper_->HasInstalledClip()) {
      // The whole widget is visible but we installed a clip on the widget,
      // uninstall it.
      native_wrapper_->UninstallClip();
    }
  }

  if (visible) {
    // Since widgets know nothing about the View hierarchy (they are direct
    // children of the Widget that hosts our View hierarchy) they need to be
    // positioned in the coordinate system of the Widget, not the current
    // view.  Also, they should be positioned respecting the border insets
    // of the native view.
    gfx::Rect local_bounds = ConvertRectToWidget(GetContentsBounds());
    gfx::Size native_view_size =
        native_view_size_.IsEmpty() ? local_bounds.size() : native_view_size_;
    native_wrapper_->ShowWidget(local_bounds.x(), local_bounds.y(),
                                local_bounds.width(), local_bounds.height(),
                                native_view_size.width(),
                                native_view_size.height());
  } else {
    native_wrapper_->HideWidget();
  }
}

void NativeViewHost::OnPaint(gfx::Canvas* canvas) {
  // Paint background if there is one. NativeViewHost needs to paint
  // a background when it is hosted in a TabbedPane. For Gtk implementation,
  // NativeTabbedPaneGtk uses a NativeWidgetGtk as page container and because
  // NativeWidgetGtk hook "expose" with its root view's paint, we need to
  // fill the content. Otherwise, the tab page's background is not properly
  // cleared. For Windows case, it appears okay to not paint background because
  // we don't have a container window in-between. However if you want to use
  // customized background, then this becomes necessary.
  OnPaintBackground(canvas);

  // The area behind our window is black, so during a fast resize (where our
  // content doesn't draw over the full size of our native view, and the native
  // view background color doesn't show up), we need to cover that blackness
  // with something so that fast resizes don't result in black flash.
  //
  // Affected views should set the desired color using
  // SetBackgroundColorWhenClipped(), otherwise the background is left
  // transparent to let the lower layers show through.
  if (native_wrapper_->HasInstalledClip()) {
    if (background_color_when_clipped_) {
      canvas->FillRect(GetLocalBounds(), *background_color_when_clipped_);
    }
  }
}

void NativeViewHost::VisibilityChanged(View* starting_from, bool is_visible) {
  // This does not use InvalidateLayout() to ensure the visibility state is
  // correctly set (if this View isn't visible, Layout() won't be called).
  Layout();
}

bool NativeViewHost::GetNeedsNotificationWhenVisibleBoundsChange() const {
  // The native widget is placed relative to the root. As such, we need to
  // know when the position of any ancestor changes, or our visibility relative
  // to other views changed as it'll effect our position relative to the root.
  return true;
}

void NativeViewHost::OnVisibleBoundsChanged() {
  InvalidateLayout();
}

void NativeViewHost::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  views::Widget* this_widget = GetWidget();

  // A non-NULL |details.move_view| indicates a move operation i.e. |this| is
  // is being reparented.  If the previous and new parents belong to the same
  // widget, don't remove |this| from the widget.  This saves resources from
  // removing from widget and immediately followed by adding to widget; in
  // particular, there wouldn't be spurious visibilitychange events for web
  // contents of |WebView|.
  if (details.move_view && this_widget &&
      details.move_view->GetWidget() == this_widget) {
    return;
  }

  if (details.is_add && this_widget) {
    if (!native_wrapper_.get())
      native_wrapper_.reset(NativeViewHostWrapper::CreateWrapper(this));
    native_wrapper_->AddedToWidget();
  } else if (!details.is_add && native_wrapper_) {
    native_wrapper_->RemovedFromWidget();
  }
}

void NativeViewHost::OnFocus() {
  if (native_view_)
    native_wrapper_->SetFocus();
  NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

gfx::NativeViewAccessible NativeViewHost::GetNativeViewAccessible() {
  if (native_wrapper_.get()) {
    gfx::NativeViewAccessible accessible_view =
        native_wrapper_->GetNativeViewAccessible();
    if (accessible_view)
      return accessible_view;
  }

  return View::GetNativeViewAccessible();
}

ui::Cursor NativeViewHost::GetCursor(const ui::MouseEvent& event) {
  return native_wrapper_->GetCursor(event.x(), event.y());
}

void NativeViewHost::SetVisible(bool visible) {
  if (native_view_)
    native_wrapper_->SetVisible(visible);
  View::SetVisible(visible);
}

bool NativeViewHost::OnMousePressed(const ui::MouseEvent& event) {
  // In the typical case the attached NativeView receives the events directly
  // from the system and this function is not called. There are scenarios
  // where that may not happen. For example, if the NativeView is configured
  // not to receive events, then this function will be called. An additional
  // scenario is if the WidgetDelegate overrides
  // ShouldDescendIntoChildForEventHandling(). In that case the NativeView
  // will not receive the events, and this function will be called. Regardless,
  // this function does not need to forward to the NativeView, because it is
  // expected to be done by the system, and the only cases where this is called
  // is if the NativeView should not receive events.
  return View::OnMousePressed(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHost, private:

void NativeViewHost::Detach(bool destroyed) {
  if (native_view_) {
    if (!destroyed) {
      Widget* widget = Widget::GetWidgetForNativeView(native_view_);
      if (widget)
        widget->SetNativeWindowProperty(kWidgetNativeViewHostKey, nullptr);
      ClearFocus();
    }
    native_wrapper_->NativeViewDetaching(destroyed);
    native_view_ = nullptr;
  }
}

void NativeViewHost::ClearFocus() {
  FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager || !focus_manager->GetFocusedView())
    return;

  Widget::Widgets widgets;
  Widget::GetAllChildWidgets(native_view(), &widgets);
  for (auto* widget : widgets) {
    focus_manager->ViewRemoved(widget->GetRootView());
    if (!focus_manager->GetFocusedView())
      return;
  }
}

BEGIN_METADATA(NativeViewHost, View)
END_METADATA

}  // namespace views
