// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/wm/public/animation_host.h"

namespace aura {
namespace client {
class DragDropClient;
class FocusClient;
}  // namespace client
}  // namespace aura

namespace ui {
enum class DomCode;
class InputMethod;
class KeyboardHook;
}  // namespace ui

namespace wm {
class ScopedTooltipDisabler;
}  // namespace wm

namespace views {
class DesktopDragDropClientWin;
class HWNDMessageHandler;
class NonClientFrameView;

namespace test {
class DesktopWindowTreeHostWinTestApi;
}

class VIEWS_EXPORT DesktopWindowTreeHostWin
    : public DesktopWindowTreeHost,
      public wm::AnimationHost,
      public aura::WindowTreeHost,
      public HWNDMessageHandlerDelegate {
 public:
  DesktopWindowTreeHostWin(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);

  DesktopWindowTreeHostWin(const DesktopWindowTreeHostWin&) = delete;
  DesktopWindowTreeHostWin& operator=(const DesktopWindowTreeHostWin&) = delete;

  ~DesktopWindowTreeHostWin() override;

  // A way of converting an HWND into a content window.
  static aura::Window* GetContentWindowForHWND(HWND hwnd);

  // When DesktopDragDropClientWin starts a touch-initiated drag, it calls
  // this method to record that we're in touch drag mode, and synthesizes
  // right mouse button down and move events to get ::DoDragDrop started.
  void StartTouchDrag(gfx::Point screen_point);

  // If in touch drag mode, this method synthesizes a left mouse button up
  // event to match the left mouse button down event in StartTouchDrag. It
  // also restores the cursor pos to where the drag started, to avoid leaving
  // the cursor outside the Chrome window doing the drag drop. This allows
  // subsequent touch drag drops to succeed. Touch drag drop requires that
  // the cursor be over the same window as the touch drag point.
  // This needs to be called in two cases:
  // 1. The normal case is that ::DoDragDrop starts, we get touch move events,
  // which we turn into mouse move events, and then we get a touch release
  // event. Calling FinishTouchDragIfInDrag generates a mouse up, which stops
  // the drag drop.
  // 2. ::DoDragDrop exits immediately, w/o us handling any touch events. In
  // this case, FinishTouchDragIfInDrag makes sure we have a mouse button up to
  // match the mouse button down, because we won't get a touch release event. We
  // don't know for sure if ::DoDragDrop exited immediately, other than by
  // checking if `in_touch_drag_` has been set to false.
  //
  // So, we always call FinishTouchDragIfInDrag after ::DoDragDrop exits, to
  // make sure it gets called, and we make it handle getting called multiple
  // times. Most of the time, FinishTouchDrag will have already been called when
  // we get a touch release event, in which case the second call needs to be a
  // noop, which is accomplished by checking if `in_touch_drag_` is already
  // false.
  void FinishTouchDrag(gfx::Point screen_point);

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  void OnActiveWindowChanged(bool active) override;
  void OnWidgetInitDone() override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient() override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void Show(ui::WindowShowState show_state,
            const gfx::Rect& restore_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  bool IsStackedAbove(aura::Window* window) override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(std::unique_ptr<Widget::ShapeRects> native_shape) override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void PaintAsActiveChanged() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool HasCapture() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  bool SetWindowTitle(const std::u16string& title) override;
  void ClearNativeFocus() override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  std::unique_ptr<NonClientFrameView> CreateNonClientFrameView() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen, int64_t target_display_id) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio,
                      const gfx::Size& excluded_margin) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;
  bool ShouldUpdateWindowTransparency() const override;
  bool ShouldUseDesktopNativeCursorManager() const override;
  bool ShouldCreateVisibilityController() const override;
  DesktopNativeCursorManager* GetSingletonDesktopNativeCursorManager() override;
  void SetBoundsInDIP(const gfx::Rect& bounds) override;

  // Overridden from aura::WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetBoundsInAcceleratedWidgetPixelCoordinates() override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool CaptureSystemKeyEventsImpl(
      absl::optional<base::flat_set<ui::DomCode>> dom_codes) override;
  void ReleaseSystemKeyEventCapture() override;
  bool IsKeyLocked(ui::DomCode dom_code) override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void OnCursorVisibilityChangedNative(bool show) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
  RequestUnadjustedMovement() override;
  void LockMouse(aura::Window* window) override;
  void UnlockMouse(aura::Window* window) override;

  // Overridden from aura::client::AnimationHost
  void SetHostTransitionOffsets(
      const gfx::Vector2d& top_left_delta,
      const gfx::Vector2d& bottom_right_delta) override;
  void OnWindowHidingAnimationCompleted() override;

  // Overridden from HWNDMessageHandlerDelegate:
  ui::InputMethod* GetHWNDMessageDelegateInputMethod() override;
  bool HasNonClientView() const override;
  FrameMode GetFrameMode() const override;
  bool HasFrame() const override;
  void SchedulePaint() override;
  bool ShouldPaintAsActive() const override;
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanActivate() const override;
  bool WantsMouseEventsWhenInactive() const override;
  bool WidgetSizeIsClientSize() const override;
  bool IsModal() const override;
  int GetInitialShowState() const override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  void GetWindowMask(const gfx::Size& size, SkPath* path) override;
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
  void GetMinMaxSize(gfx::Size* min_size, gfx::Size* max_size) const override;
  gfx::Size GetRootViewSize() const override;
  gfx::Size DIPToScreenSize(const gfx::Size& dip_size) const override;
  void ResetWindowControls() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void HandleActivationChanged(bool active) override;
  bool HandleAppCommand(int command) override;
  void HandleCancelMode() override;
  void HandleCaptureLost() override;
  void HandleClose() override;
  bool HandleCommand(int command) override;
  void HandleAccelerator(const ui::Accelerator& accelerator) override;
  void HandleCreate() override;
  void HandleDestroying() override;
  void HandleDestroyed() override;
  bool HandleInitialFocus(ui::WindowShowState show_state) override;
  void HandleDisplayChange() override;
  void HandleBeginWMSizeMove() override;
  void HandleEndWMSizeMove() override;
  void HandleMove() override;
  void HandleWorkAreaChanged() override;
  void HandleVisibilityChanged(bool visible) override;
  void HandleWindowMinimizedOrRestored(bool restored) override;
  void HandleClientSizeChanged(const gfx::Size& new_size) override;
  void HandleFrameChanged() override;
  void HandleNativeFocus(HWND last_focused_window) override;
  void HandleNativeBlur(HWND focused_window) override;
  bool HandleMouseEvent(ui::MouseEvent* event) override;
  void HandleKeyEvent(ui::KeyEvent* event) override;
  void HandleTouchEvent(ui::TouchEvent* event) override;
  bool HandleIMEMessage(UINT message,
                        WPARAM w_param,
                        LPARAM l_param,
                        LRESULT* result) override;
  void HandleInputLanguageChange(DWORD character_set,
                                 HKL input_language_id) override;
  void HandlePaintAccelerated(const gfx::Rect& invalid_rect) override;
  void HandleMenuLoop(bool in_menu_loop) override;
  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override;
  void PostHandleMSG(UINT message, WPARAM w_param, LPARAM l_param) override;
  bool HandleScrollEvent(ui::ScrollEvent* event) override;
  bool HandleGestureEvent(ui::GestureEvent* event) override;
  void HandleWindowSizeChanging() override;
  void HandleWindowSizeUnchanged() override;
  void HandleWindowScaleFactorChanged(float window_scale_factor) override;
  void HandleHeadlessWindowBoundsChanged(const gfx::Rect& bounds) override;

  Widget* GetWidget();
  const Widget* GetWidget() const;
  HWND GetHWND() const;

 private:
  friend class ::views::test::DesktopWindowTreeHostWinTestApi;

  // Returns true if a modal window is active in the current root window chain.
  bool IsModalWindowActive() const;

  // Called whenever the HWND resizes or moves, to see if the nearest HMONITOR
  // has changed, and, if so, inform the aura::WindowTreeHost.
  void CheckForMonitorChange();

  // Accessor for DesktopNativeWidgetAura::content_window().
  aura::Window* content_window();

  HMONITOR last_monitor_from_window_ = nullptr;

  std::unique_ptr<HWNDMessageHandler> message_handler_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  base::WeakPtr<internal::NativeWidgetDelegate> native_widget_delegate_;

  raw_ptr<DesktopNativeWidgetAura> desktop_native_widget_aura_;

  // Owned by DesktopNativeWidgetAura.
  base::WeakPtr<DesktopDragDropClientWin> drag_drop_client_;

  // When certain windows are being shown, we augment the window size
  // temporarily for animation. The following two members contain the top left
  // and bottom right offsets which are used to enlarge the window.
  gfx::Vector2d window_expansion_top_left_delta_;
  gfx::Vector2d window_expansion_bottom_right_delta_;

  // Windows are enlarged to be at least 64x64 pixels, so keep track of the
  // extra added here.
  gfx::Vector2d window_enlargement_;

  // Whether the window close should be converted to a hide, and then actually
  // closed on the completion of the hide animation. This is cached because
  // the property is set on the contained window which has a shorter lifetime.
  bool should_animate_window_close_;

  // When Close()d and animations are being applied to this window, the close
  // of the window needs to be deferred to when the close animation is
  // completed. This variable indicates that a Close was converted to a Hide,
  // so that when the Hide is completed the host window should be closed.
  bool pending_close_;

  // True if the widget is going to have a non_client_view. We cache this value
  // rather than asking the Widget for the non_client_view so that we know at
  // Init time, before the Widget has created the NonClientView.
  bool has_non_client_view_;

  // True if the window should have the frame removed.
  bool remove_standard_frame_;

  // Visibility of the cursor. On Windows we can have multiple root windows and
  // the implementation of ::ShowCursor() is based on a counter, so making this
  // member static ensures that ::ShowCursor() is always called exactly once
  // whenever the cursor visibility state changes.
  static bool is_cursor_visible_;

  // Captures system key events when keyboard lock is requested.
  std::unique_ptr<ui::KeyboardHook> keyboard_hook_;

  std::unique_ptr<wm::ScopedTooltipDisabler> tooltip_disabler_;

  // Indicates if current window will receive mouse events when should not
  // become activated.
  bool wants_mouse_events_when_inactive_ = false;

  // Set to true when DesktopDragDropClientWin starts a touch-initiated drag
  // drop and false when it finishes. While in touch drag, if touch move events
  // are received, the equivalent mouse events are generated, because ole32
  // ::DoDragDrop does not seem to handle touch events. WinRT drag drop does
  // support touch, but we've been unable to use it in Chrome. See
  // https://crbug.com/1236783 for more info.
  bool in_touch_drag_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_
