// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <aura-shell-client-protocol.h>
#include <stdint.h>
#include <wayland-cursor.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

namespace ui {
namespace {

using mojom::CursorType;
using mojom::DragOperation;

bool OverlayStackOrderCompare(const wl::WaylandOverlayConfig& i,
                              const wl::WaylandOverlayConfig& j) {
  return i.z_order < j.z_order;
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      frame_manager_(std::make_unique<WaylandFrameManager>(this, connection)),
      wayland_overlay_delegation_enabled_(
          connection->viewporter() && connection->ShouldUseOverlayDelegation()),
      accelerated_widget_(
          connection->window_manager()->AllocateAcceleratedWidget()),
      ui_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandWindow::~WaylandWindow() {
  CHECK(ui_task_runner_->BelongsToCurrentThread());
  shutting_down_ = true;

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  if (wayland_overlay_delegation_enabled_) {
    connection_->window_manager()->RemoveSubsurface(GetWidget(),
                                                    primary_subsurface_.get());
    connection_->window_manager()->RecycleSubsurface(
        std::move(primary_subsurface_));
  }
  for (const auto& widget_subsurface : wayland_subsurfaces()) {
    connection_->window_manager()->RemoveSubsurface(GetWidget(),
                                                    widget_subsurface.get());
  }
  if (root_surface_) {
    connection_->window_manager()->RemoveWindow(GetWidget());
  }

  // This might have already been hidden and another window has been shown.
  // Thus, the parent will have another child window. Do not reset it.
  if (parent_window_ && parent_window_->child_window() == this) {
    parent_window_->set_child_window(nullptr);
  }

  if (child_window_) {
    child_window_->set_parent_window(nullptr);
  }
}

void WaylandWindow::OnWindowLostCapture() {
  delegate_->OnLostCapture();
}

void WaylandWindow::UpdateWindowScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());

  const auto* output_manager = connection_->wayland_output_manager();
  auto preferred_outputs_id = GetPreferredEnteredOutputId();
  if (!preferred_outputs_id.has_value()) {
    // If no output was entered yet, use primary output. This is similar to what
    // PlatformScreen implementation is expected to return to higher layer.
    auto* primary_output = output_manager->GetPrimaryOutput();
    // Primary output is unknown. i.e: WaylandScreen was not created yet.
    if (!primary_output) {
      return;
    }
    preferred_outputs_id = primary_output->output_id();
  }

  auto* output = output_manager->GetOutput(preferred_outputs_id.value());
  // There can be a race between sending leave output event and destroying
  // wl_outputs. Thus, explicitly check if the output exist.
  if (!output || !output->IsReady()) {
    return;
  }

  float new_scale = output->scale_factor();
  ui_scale_ = output->GetUIScaleFactor();
  SetWindowScale(new_scale);

  // Propagate update to the child windows
  if (child_window_) {
    child_window_->UpdateWindowScale(update_bounds);
  }
}

WaylandZAuraSurface* WaylandWindow::GetZAuraSurface() {
  return root_surface_ ? root_surface_->zaura_surface() : nullptr;
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  return accelerated_widget_;
}

void WaylandWindow::SetWindowScale(float new_scale) {
  DCHECK_GE(new_scale, 0.f);
  auto state = GetLatestRequestedState();
  state.window_scale = new_scale;
  // Note that we still need to call this even if the state does not change,
  // because we want requests directly from the client (us) to be applied
  // immediately, since that's what PlatformWindow expects. Also, RequestState
  // may modify the state before applying it.
  RequestStateFromClient(state);
}

absl::optional<WaylandOutput::Id> WaylandWindow::GetPreferredEnteredOutputId() {
  // Child windows don't store entered outputs. Instead, take the window's
  // root parent window and use its preferred output.
  if (parent_window_) {
    return GetRootParentWindow()->GetPreferredEnteredOutputId();
  }

  // It can be either a toplevel window that hasn't entered any outputs yet, or
  // still a non toplevel window that doesn't have a parent (for example, a
  // wl_surface that is being dragged).
  if (root_surface_->entered_outputs().empty()) {
    // The nullcheck is necessary because some tests create mock screen
    // instead of emulating at wayland level.
    if (IsScreenCoordinatesEnabled() &&
        connection_->wayland_output_manager()->wayland_screen()) {
      // If the surface hasn't entered any output yet, but the
      // screen coordinates is enabled, try to find the screen that
      // matches the window's bounds.
      return connection_->wayland_output_manager()
          ->wayland_screen()
          ->GetOutputIdMatching(GetBoundsInDIP());
    }
    return absl::nullopt;
  }

  // PlatformWindowType::kPopup are created as toplevel windows as well.
  DCHECK(type() == PlatformWindowType::kWindow ||
         type() == PlatformWindowType::kPopup);

  WaylandOutput::Id preferred_id = root_surface_->entered_outputs().front();
  const auto* output_manager = connection_->wayland_output_manager();

  // A window can be located on two or more displays. Thus, return the id of the
  // output that has the biggest scale factor. Otherwise, use the very first one
  // that was entered. This way, we can be sure that the contents of the Window
  // are rendered at correct dpi when a user moves the window between displays.
  for (WaylandOutput::Id output_id : root_surface_->entered_outputs()) {
    auto* output = output_manager->GetOutput(output_id);
    auto* preferred_output = output_manager->GetOutput(preferred_id);
    // The compositor may have told the surface to enter the output that the
    // client is not aware of.  In such an event, we cannot evaluate scales, and
    // can only return the default, which means falling back to the primary
    // display in the code that calls this. DCHECKS below are kept for trying to
    // catch the situation in developer's builds and find the way to reproduce
    // the issue. See crbug.com/1323635.
    DCHECK(output) << " output " << output_id << " not found!";
    DCHECK(preferred_output) << " output " << preferred_id << " not found!";
    if (!output || !preferred_output) {
      return absl::nullopt;
    }

    if (output->scale_factor() > preferred_output->scale_factor()) {
      preferred_id = output_id;
    }
  }

  return preferred_id;
}

void WaylandWindow::OnPointerFocusChanged(bool focused) {
  // Whenever the window gets the pointer focus back, the cursor shape must be
  // updated. Otherwise, it is invalidated upon wl_pointer::leave and is not
  // restored by the Wayland compositor.
#if BUILDFLAG(IS_LINUX)
  if (focused && async_cursor_) {
    async_cursor_->AddCursorLoadedCallback(
        base::BindOnce(&WaylandWindow::OnCursorLoaded,
                       weak_ptr_factory_.GetWeakPtr(), async_cursor_));
  }
#else
  if (focused && cursor_) {
    UpdateCursorShape(cursor_);
  }
#endif
}

bool WaylandWindow::HasPointerFocus() const {
  return this ==
         connection_->window_manager()->GetCurrentPointerFocusedWindow();
}

bool WaylandWindow::HasKeyboardFocus() const {
  return this ==
         connection_->window_manager()->GetCurrentKeyboardFocusedWindow();
}

void WaylandWindow::RemoveEnteredOutput(uint32_t output_id) {
  root_surface_->RemoveEnteredOutput(output_id);
}

bool WaylandWindow::StartDrag(
    const ui::OSExchangeData& data,
    int operations,
    mojom::DragEventSource source,
    gfx::NativeCursor cursor,
    bool can_grab_pointer,
    WmDragHandler::DragFinishedCallback drag_finished_callback,
    WmDragHandler::LocationDelegate* location_delegate) {
  if (!connection_->data_drag_controller()->StartSession(data, operations,
                                                         source)) {
    return false;
  }

  DCHECK(drag_finished_callback_.is_null());
  drag_finished_callback_ = std::move(drag_finished_callback);

  base::RunLoop drag_loop(base::RunLoop::Type::kNestableTasksAllowed);
  drag_loop_quit_closure_ = drag_loop.QuitClosure();

  auto alive = weak_ptr_factory_.GetWeakPtr();
  drag_loop.Run();
  if (!alive) {
    return false;
  }
  return true;
}

void WaylandWindow::UpdateDragImage(const gfx::ImageSkia& image,
                                    const gfx::Vector2d& offset) {
  if (connection_->data_drag_controller()->state() !=
      WaylandDataDragController::State::kIdle) {
    connection_->data_drag_controller()->UpdateDragImage(image, offset);
  }
}

void WaylandWindow::CancelDrag() {
  if (drag_loop_quit_closure_.is_null()) {
    return;
  }
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandWindow::Show(bool inactive) {
  // Initially send the window geometry. After this, we only update window
  // geometry when the value in latched_state_ updates.
  SetWindowGeometry(latched_state_.bounds_dip.size());
  frame_manager_->MaybeProcessPendingFrame();
}

void WaylandWindow::Hide() {
  received_configure_event_ = false;

  if (primary_subsurface_) {
    primary_subsurface()->Hide();
  }
  for (auto& subsurface : wayland_subsurfaces_) {
    subsurface->Hide();
  }
  frame_manager_->Hide();
}

void WaylandWindow::OnChannelDestroyed() {
  frame_manager_->ClearStates();
  base::circular_deque<std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
      subsurfaces_to_overlays;
  subsurfaces_to_overlays.reserve(wayland_subsurfaces_.size() +
                                  (primary_subsurface() ? 1 : 0));
  if (primary_subsurface()) {
    subsurfaces_to_overlays.emplace_back(primary_subsurface(),
                                         wl::WaylandOverlayConfig());
  }
  for (auto& subsurface : wayland_subsurfaces_) {
    subsurfaces_to_overlays.emplace_back(subsurface.get(),
                                         wl::WaylandOverlayConfig());
  }

  frame_manager_->RecordFrame(
      std::make_unique<WaylandFrame>(root_surface(), wl::WaylandOverlayConfig(),
                                     std::move(subsurfaces_to_overlays)));
}

void WaylandWindow::DumpState(std::ostream& out) const {
  constexpr auto kWindowTypeToString =
      base::MakeFixedFlatMap<PlatformWindowType, const char*>(
          {{PlatformWindowType::kWindow, "window"},
           {PlatformWindowType::kPopup, "popup"},
           {PlatformWindowType::kMenu, "menu"},
           {PlatformWindowType::kTooltip, "tooltip"},
           {PlatformWindowType::kDrag, "drag"},
           {PlatformWindowType::kBubble, "bubble"}});
  out << "type=" << GetMapValueOrDefault(kWindowTypeToString, type_)
      << ", bounds_in_dip=" << GetBoundsInDIP().ToString()
      << ", bounds_in_pixels=" << GetBoundsInPixels().ToString()
      << ", restore_bounds_dip=" << restored_bounds_dip_.ToString()
      << ", overlay_delegation="
      << (wayland_overlay_delegation_enabled_ ? "enabled" : "disabled");
  if (frame_insets_px_) {
    out << ", frame_insets=" << frame_insets_px_->ToString();
  }
  if (has_touch_focus_) {
    out << ", has_touch_focus";
  }
  out << ", ui_scale=" << ui_scale_;
  constexpr auto kOpacityToString =
      base::MakeFixedFlatMap<PlatformWindowOpacity, const char*>(
          {{PlatformWindowOpacity::kInferOpacity, "infer"},
           {PlatformWindowOpacity::kOpaqueWindow, "opaque"},
           {PlatformWindowOpacity::kTranslucentWindow, "translucent"}});
  out << ", opacity=" << GetMapValueOrDefault(kOpacityToString, opacity_);
  if (shutting_down_) {
    out << ", shutting_down";
  }
}

bool WaylandWindow::SupportsConfigureMinimizedState() const {
  return false;
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  NOTREACHED();
  return false;
}

void WaylandWindow::PrepareForShutdown() {
  if (drag_finished_callback_) {
    OnDragSessionClose(DragOperation::kNone);
  }
}

void WaylandWindow::SetBoundsInPixels(const gfx::Rect& bounds_px) {
  // TODO(crbug.com/1306688): This is currently used only by unit tests.
  // Figure out how to migrate to test only methods.
  auto bounds_dip = delegate_->ConvertRectToDIP(bounds_px);
  SetBoundsInDIP(bounds_dip);
}

gfx::Rect WaylandWindow::GetBoundsInPixels() const {
  // TODO(crbug.com/1306688): This is currently used only by unit tests.
  // Figure out how to migrate to test only methods. For now, only the size
  // should be used outside of tests. Make up some reasonable value for origin.
  auto origin =
      delegate_->ConvertRectToPixels(applied_state_.bounds_dip).origin();
  auto size = applied_state_.size_px;
  return gfx::Rect(origin, size);
}

void WaylandWindow::SetBoundsInDIP(const gfx::Rect& bounds_dip) {
  auto state = GetLatestRequestedState();
  state.bounds_dip = bounds_dip;
  // Call this even if the bounds haven't changed, as requesting from the client
  // forces applying the state, which may (currently) not be applied if it was
  // throttled. Also, RequestState may modify the state before applying it.
  RequestStateFromClient(state);
}

gfx::Rect WaylandWindow::GetBoundsInDIP() const {
  return applied_state_.bounds_dip;
}

void WaylandWindow::OnSurfaceConfigureEvent() {
  if (received_configure_event_) {
    return;
  }
  received_configure_event_ = true;
  frame_manager_->MaybeProcessPendingFrame();
}

void WaylandWindow::SetTitle(const std::u16string& title) {}

void WaylandWindow::SetCapture() {
  // Wayland doesn't allow explicit grabs. Instead, it sends events to "entered"
  // windows. That is, if user enters their mouse pointer to a window, that
  // window starts to receive events. However, Chromium may want to reroute
  // these events to another window. In this case, tell the window manager that
  // this specific window has grabbed the events, and they will be rerouted in
  // WaylandWindow::DispatchEvent method.
  if (!HasCapture()) {
    connection_->window_manager()->GrabLocatedEvents(this);
  }
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture()) {
    connection_->window_manager()->UngrabLocatedEvents(this);
  }
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  return connection_->window_manager()->located_events_grabber() == this;
}

void WaylandWindow::SetFullscreen(bool fullscreen, int64_t target_display_id) {}

void WaylandWindow::Maximize() {}

void WaylandWindow::Minimize() {}

void WaylandWindow::Restore() {}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  // Remove normal state for all the other types of windows as it's only the
  // WaylandToplevelWindow that supports state changes.
  return PlatformWindowState::kNormal;
}

void WaylandWindow::Activate() {}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // Do nothing here since only shell surfaces can handle server-side
  // decoration.
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // Always returns false here since only shell surfaces can handle server-side
  // decoration.
  return false;
}

void WaylandWindow::SetCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  DCHECK(platform_cursor);

#if BUILDFLAG(IS_LINUX)
  auto async_cursor = WaylandAsyncCursor::FromPlatformCursor(platform_cursor);

  if (async_cursor_ == async_cursor) {
    return;
  }

  async_cursor_ = async_cursor;
  async_cursor->AddCursorLoadedCallback(
      base::BindOnce(&WaylandWindow::OnCursorLoaded,
                     weak_ptr_factory_.GetWeakPtr(), async_cursor));
#else
  if (cursor_ == platform_cursor) {
    return;
  }

  UpdateCursorShape(BitmapCursor::FromPlatformCursor(platform_cursor));
#endif
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WaylandWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  restored_bounds_dip_ = bounds;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInDIP() const {
  return restored_bounds_dip_;
}

bool WaylandWindow::ShouldWindowContentsBeTransparent() const {
  // Wayland compositors always support translucency.
  return true;
}

void WaylandWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandWindow::IsTranslucentWindowOpacitySupported() const {
  // Wayland compositors always support translucency.
  return true;
}

void WaylandWindow::SetDecorationInsets(const gfx::Insets* insets_px) {
  // TODO(crbug.com/1395267): Add window geometry to WaylandWindow::State.
  if ((!frame_insets_px_ && !insets_px) ||
      (frame_insets_px_ && insets_px && *frame_insets_px_ == *insets_px)) {
    return;
  }
  if (insets_px) {
    frame_insets_px_ = *insets_px;
  } else {
    frame_insets_px_ = absl::nullopt;
  }
}

void WaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SizeConstraintsChanged() {}

bool WaylandWindow::ShouldUpdateWindowShape() const {
  return false;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  return CanAcceptEvent(*event);
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);

  if (event->IsLocatedEvent()) {
    auto* event_grabber =
        connection_->window_manager()->located_events_grabber();
    auto* root_parent_window = GetRootParentWindow();

    // We must reroute the events to the event grabber iff these windows belong
    // to the same root parent window. For example, there are 2 top level
    // Wayland windows. One of them (window_1) has a child menu window that is
    // the event grabber. If the mouse is moved over the window_1, it must
    // reroute the events to the event grabber. If the mouse is moved over the
    // window_2, the events mustn't be rerouted, because that belongs to another
    // stack of windows. Remember that Wayland sends local surface coordinates,
    // and continuing rerouting all the events may result in events sent to the
    // grabber even though the mouse is over another root window.
    //
    bool send_to_grabber =
        event_grabber &&
        root_parent_window == event_grabber->GetRootParentWindow();
    if (send_to_grabber) {
      WaylandEventSource::ConvertEventToTarget(event_grabber,
                                               event->AsLocatedEvent());
      Event::DispatcherApi(event).set_target(event_grabber);
    }

    // Wayland sends locations in DIP but dispatch code expects pixels, so they
    // need to be translated to physical pixels.
    auto scale = applied_state().window_scale;
    event->AsLocatedEvent()->set_location_f(
        gfx::ScalePoint(event->AsLocatedEvent()->location_f(), scale, scale));

    if (send_to_grabber) {
      event_grabber->DispatchEventToDelegate(event);
      // The event should be handled by the grabber, so don't send to next
      // dispacher.
      return POST_DISPATCH_STOP_PROPAGATION;
    }
  }

  // Dispatch all keyboard events to the root window.
  if (event->IsKeyEvent()) {
    return GetRootParentWindow()->DispatchEventToDelegate(event);
  }

  return DispatchEventToDelegate(event);
}

// EventTarget:
bool WaylandWindow::CanAcceptEvent(const Event& event) {
#if DCHECK_IS_ON()
  if (!disable_null_target_dcheck_for_test_) {
    DCHECK(event.target());
  }
#endif
  return this == event.target();
}

EventTarget* WaylandWindow::GetParentTarget() {
  return nullptr;
}

std::unique_ptr<EventTargetIterator> WaylandWindow::GetChildIterator() const {
  NOTREACHED();
  return nullptr;
}

EventTargeter* WaylandWindow::GetEventTargeter() {
  return nullptr;
}

void WaylandWindow::HandleSurfaceConfigure(uint32_t serial) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

std::string WaylandWindow::WindowStates::ToString() const {
  std::string states = "";
  if (is_maximized) {
    states += "maximized ";
  }
  if (is_fullscreen) {
    states += "fullscreen ";
  }
  if (is_activated) {
    states += "activated ";
  }
  if (states.empty()) {
    states = "<default>";
  } else {
    base::TrimString(states, " ", &states);
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  states += "; tiled_edges: ";
  std::string tiled = "";
  if (tiled_edges.left) {
    tiled += "left ";
  }
  if (tiled_edges.right) {
    tiled += "right ";
  }
  if (tiled_edges.top) {
    tiled += "top ";
  }
  if (tiled_edges.bottom) {
    tiled += "bottom ";
  }
  if (tiled.empty()) {
    tiled = "<none>";
  } else {
    base::TrimString(tiled, " ", &tiled);
  }
  states += tiled;
#endif
  return states;
}

void WaylandWindow::HandleToplevelConfigure(int32_t widht,
                                            int32_t height,
                                            const WindowStates& window_states) {
  NOTREACHED()
      << "Only shell toplevels must receive HandleToplevelConfigure calls.";
}

void WaylandWindow::HandleAuraToplevelConfigure(
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    const WindowStates& window_states) {
  NOTREACHED()
      << "Only shell toplevels must receive HandleAuraToplevelConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return;
  }

  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragEnter(point, std::move(data), operation,
                            /*modifiers=*/0);
}

int WaylandWindow::OnDragMotion(const gfx::PointF& point, int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return 0;
  }

  // TODO(crbug.com/1102857): get the real event modifier here.
  return drop_handler->OnDragMotion(point, operation,
                                    /*modifiers=*/0);
}

void WaylandWindow::OnDragDrop() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return;
  }
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragDrop({}, /*modifiers=*/0);
}

void WaylandWindow::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler) {
    return;
  }
  drop_handler->OnDragLeave();
}

void WaylandWindow::OnDragSessionClose(DragOperation operation) {
  if (!drag_finished_callback_) {
    // WaylandWindow::PrepareForShutdown() is already called. This window
    // is about to shut down. Do nothing and return.
    return;
  }
  std::move(drag_finished_callback_).Run(operation);
  connection()->event_source()->ResetPointerFlags();
  std::move(drag_loop_quit_closure_).Run();
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  root_surface_ = std::make_unique<WaylandSurface>(connection_, this);
  if (!root_surface_->Initialize()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }

  PlatformWindowDelegate::State state;
  state.bounds_dip = properties.bounds;

  // Make sure we don't store empty bounds, or else later on we might send an
  // xdg_toplevel.set_window_geometry() request with zero width and height,
  // which will result in a protocol error:
  // "The width and height of the effective window geometry must be greater than
  // zero. Setting an invalid size will raise an invalid_size error."
  // This can happen when a test doesn't set `properties.bounds`, but there have
  // also been crashes in production because of this (crbug.com/1435478).
  if (state.bounds_dip.IsEmpty()) {
    state.bounds_dip = gfx::Rect(0, 0, 1, 1);
  }

  // Properties contain DIP bounds but the buffer scale is initially 1 so it's
  // OK to assign.  The bounds will be recalculated when the buffer scale
  // changes.
  state.size_px = state.bounds_dip.size();

  opacity_ = properties.opacity;
  type_ = properties.type;

  // Lacros currently uses a different approach to support KeyboardLock,
  // which relies on the Exo-specific zcr-keyboard-extension-v1 + a permanent
  // zwp-keyboard-shortcuts-inhibitor-v1. For more details, see comments in
  // OzonePlatformWayland::CreateKeyboardHook function.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  WaylandKeyboard* keyboard =
      connection_->seat() ? connection_->seat()->keyboard() : nullptr;
  if (keyboard && properties.inhibit_keyboard_shortcuts) {
    permanent_keyboard_shortcuts_inhibitor_ =
        keyboard->CreateShortcutsInhibitor(this);
  }
#endif

  connection_->window_manager()->AddWindow(GetWidget(), this);

  if (!OnInitialize(std::move(properties), &state)) {
    return false;
  }

  applied_state_ = state;
  latched_state_ = state;

  if (wayland_overlay_delegation_enabled_) {
    primary_subsurface_ =
        std::make_unique<WaylandSubsurface>(connection_, this);
    if (!primary_subsurface_->surface()) {
      return false;
    }
    connection_->window_manager()->AddSubsurface(GetWidget(),
                                                 primary_subsurface_.get());
  }

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  std::vector<gfx::Rect> region{gfx::Rect{latched_state().size_px}};
  root_surface_->set_opaque_region(region);
  root_surface_->EnableTrustedDamageIfPossible();
  root_surface_->ApplyPendingState();

  connection_->Flush();

  return true;
}

void WaylandWindow::SetWindowGeometry(gfx::Size size_dip) {}

gfx::Vector2d WaylandWindow::GetWindowGeometryOffsetInDIP() const {
  if (!frame_insets_px_.has_value()) {
    return {};
  }

  auto scale = applied_state().window_scale;
  return {static_cast<int>(frame_insets_px_->left() / scale),
          static_cast<int>(frame_insets_px_->top() / scale)};
}

gfx::Insets WaylandWindow::GetDecorationInsetsInDIP() const {
  auto scale = latched_state().window_scale;
  return frame_insets_px_.has_value()
             ? gfx::ScaleToRoundedInsets(*frame_insets_px_, 1.f / scale)
             : gfx::Insets{};
}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::OnEnteredOutput() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup()) {
    return;
  }

  UpdateWindowScale(true);
}

void WaylandWindow::OnLeftOutput() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup()) {
    return;
  }

  UpdateWindowScale(true);
}

WaylandWindow* WaylandWindow::GetTopMostChildWindow() {
  return child_window_ ? child_window_->GetTopMostChildWindow() : this;
}

bool WaylandWindow::IsOpaqueWindow() const {
  return opacity_ == ui::PlatformWindowOpacity::kOpaqueWindow;
}

bool WaylandWindow::IsActive() const {
  // Please read the comment where the IsActive method is declared.
  return false;
}

WaylandPopup* WaylandWindow::AsWaylandPopup() {
  return nullptr;
}

WaylandToplevelWindow* WaylandWindow::AsWaylandToplevelWindow() {
  return nullptr;
}

bool WaylandWindow::IsScreenCoordinatesEnabled() const {
  return false;
}

uint32_t WaylandWindow::DispatchEventToDelegate(
    const PlatformEvent& native_event) {
  bool handled = DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return handled ? POST_DISPATCH_STOP_PROPAGATION : POST_DISPATCH_NONE;
}

std::unique_ptr<WaylandSurface> WaylandWindow::TakeWaylandSurface() {
  DCHECK(shutting_down_);
  DCHECK(root_surface_);
  root_surface_->UnsetRootWindow();
  return std::move(root_surface_);
}

bool WaylandWindow::RequestSubsurface() {
  auto subsurface = std::make_unique<WaylandSubsurface>(connection_, this);
  if (!subsurface->surface()) {
    return false;
  }
  connection_->window_manager()->AddSubsurface(GetWidget(), subsurface.get());
  subsurface_stack_above_.push_back(subsurface.get());
  auto result = wayland_subsurfaces_.emplace(std::move(subsurface));
  DCHECK(result.second);
  return true;
}

bool WaylandWindow::ArrangeSubsurfaceStack(size_t above, size_t below) {
  while (wayland_subsurfaces_.size() < above + below) {
    if (!RequestSubsurface()) {
      return false;
    }
  }

  DCHECK(subsurface_stack_below_.size() + subsurface_stack_above_.size() >=
         above + below);

  if (subsurface_stack_above_.size() < above) {
    auto splice_start = subsurface_stack_below_.begin();
    for (size_t i = 0; i < below; ++i) {
      ++splice_start;
    }
    subsurface_stack_above_.splice(subsurface_stack_above_.end(),
                                   subsurface_stack_below_, splice_start,
                                   subsurface_stack_below_.end());

  } else if (subsurface_stack_below_.size() < below) {
    auto splice_start = subsurface_stack_above_.end();
    for (size_t i = 0; i < below - subsurface_stack_below_.size(); ++i) {
      --splice_start;
    }
    subsurface_stack_below_.splice(subsurface_stack_below_.end(),
                                   subsurface_stack_above_, splice_start,
                                   subsurface_stack_above_.end());
  }

  DCHECK(subsurface_stack_below_.size() >= below);
  DCHECK(subsurface_stack_above_.size() >= above);
  return true;
}

bool WaylandWindow::CommitOverlays(
    uint32_t frame_id,
    int64_t seq,
    std::vector<wl::WaylandOverlayConfig>& overlays) {
  if (overlays.empty()) {
    return true;
  }

  // |overlays| is sorted from bottom to top.
  std::sort(overlays.begin(), overlays.end(), OverlayStackOrderCompare);

  // Find the location where z_oder becomes non-negative.
  wl::WaylandOverlayConfig value;
  auto split = std::lower_bound(overlays.begin(), overlays.end(), value,
                                OverlayStackOrderCompare);
  DCHECK(split == overlays.end() || (*split).z_order >= 0);
  size_t num_primary_planes =
      (split != overlays.end() && (*split).z_order == 0) ? 1 : 0;
  size_t num_background_planes =
      (overlays.front().z_order == INT32_MIN) ? 1 : 0;

  size_t above = (overlays.end() - split) - num_primary_planes;
  size_t below = (split - overlays.begin()) - num_background_planes;

  // Re-arrange the list of subsurfaces to fit the |overlays|. Request extra
  // subsurfaces if needed.
  if (!ArrangeSubsurfaceStack(above, below)) {
    return false;
  }

  gfx::SizeF visual_size = (*overlays.begin()).bounds_rect.size();
  float buffer_scale = (*overlays.begin()).surface_scale_factor;
  auto& rounded_clip_bounds = (*overlays.begin()).rounded_clip_bounds;

  if (!wayland_overlay_delegation_enabled_) {
    DCHECK_EQ(overlays.size(), 1u);
    frame_manager_->RecordFrame(std::make_unique<WaylandFrame>(
        frame_id, seq, root_surface(), std::move(*overlays.begin())));
    return true;
  }

  base::circular_deque<std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
      subsurfaces_to_overlays;
  subsurfaces_to_overlays.reserve(
      std::max(overlays.size() - num_background_planes,
               wayland_subsurfaces_.size() + 1));

  subsurfaces_to_overlays.emplace_back(
      primary_subsurface(),
      num_primary_planes ? std::move(*split) : wl::WaylandOverlayConfig());

  {
    // Iterate through |subsurface_stack_below_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    auto overlay_iter = split - 1;
    for (auto iter = subsurface_stack_below_.begin();
         iter != subsurface_stack_below_.end(); ++iter, --overlay_iter) {
      if (overlay_iter >= overlays.begin() + num_background_planes) {
        subsurfaces_to_overlays.emplace_front(*iter, std::move(*overlay_iter));
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        subsurfaces_to_overlays.emplace_front(*iter,
                                              wl::WaylandOverlayConfig());
      }
    }

    // Iterate through |subsurface_stack_above_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    overlay_iter = split + num_primary_planes;
    for (auto iter = subsurface_stack_above_.begin();
         iter != subsurface_stack_above_.end(); ++iter, ++overlay_iter) {
      if (overlay_iter < overlays.end()) {
        subsurfaces_to_overlays.emplace_back(*iter, std::move(*overlay_iter));
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        subsurfaces_to_overlays.emplace_back(*iter, wl::WaylandOverlayConfig());
      }
    }
  }

  // Configuration of the root_surface
  wl::WaylandOverlayConfig root_config;
  if (num_background_planes) {
    root_config = std::move(overlays.front());
  } else {
    root_config = wl::WaylandOverlayConfig(
        gfx::OverlayPlaneData(
            INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
            gfx::RectF(visual_size), gfx::RectF(),
            root_surface()->use_blending(), gfx::Rect(),
            root_surface()->opacity(), gfx::OverlayPriorityHint::kNone,
            rounded_clip_bounds.value_or(gfx::RRectF()),
            gfx::ColorSpace::CreateSRGB(), absl::nullopt),
        nullptr, root_surface()->buffer_id(), buffer_scale);
  }

  frame_manager_->RecordFrame(std::make_unique<WaylandFrame>(
      frame_id, seq, root_surface(), std::move(root_config),
      std::move(subsurfaces_to_overlays)));

  return true;
}

void WaylandWindow::UpdateCursorShape(scoped_refptr<BitmapCursor> cursor) {
  DCHECK(cursor);
  CHECK(connection_->surface_submission_in_pixel_coordinates() ||
        cursor->type() == CursorType::kNone ||
        base::IsValueInRangeForNumericType<int>(
            cursor->cursor_image_scale_factor()));

  absl::optional<int32_t> shape =
      WaylandZcrCursorShapes::ShapeFromType(cursor->type());

  // Round cursor scale factor to ceil as wl_surface.set_buffer_scale accepts
  // only integers.
  if (cursor->type() == CursorType::kNone) {  // Hide the cursor.
    connection_->SetCursorBitmap(
        {}, gfx::Point(), std::ceil(cursor->cursor_image_scale_factor()));
  } else if (cursor->platform_data()) {  // Check for theme-provided cursor.
    connection_->SetPlatformCursor(
        reinterpret_cast<wl_cursor*>(cursor->platform_data()),
        std::ceil(cursor->cursor_image_scale_factor()));
  } else if (connection_->zcr_cursor_shapes() &&
             shape.has_value()) {  // Check for Wayland server-side cursor
                                   // support (e.g. exo for lacros).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Lacros should not load image assets for default cursors. See
    // `BitmapCursorFactory::GetDefaultCursor()`.
    DCHECK(cursor->bitmaps().empty());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    connection_->zcr_cursor_shapes()->SetCursorShape(shape.value());
  } else {  // Use client-side bitmap cursors as fallback.
    // Translate physical pixels to DIPs.
    gfx::Point hotspot_in_dips = gfx::ScaleToRoundedPoint(
        cursor->hotspot(), 1.0f / cursor->cursor_image_scale_factor());
    connection_->SetCursorBitmap(
        cursor->bitmaps(), hotspot_in_dips,
        std::ceil(cursor->cursor_image_scale_factor()));
  }
#if !BUILDFLAG(IS_LINUX)
  cursor_ = cursor;
#endif
}

#if BUILDFLAG(IS_LINUX)
void WaylandWindow::OnCursorLoaded(scoped_refptr<WaylandAsyncCursor> cursor,
                                   scoped_refptr<BitmapCursor> bitmap_cursor) {
  if (HasPointerFocus() && async_cursor_ == cursor && bitmap_cursor) {
    UpdateCursorShape(bitmap_cursor);
  }
}
#endif

void WaylandWindow::ProcessPendingConfigureState(uint32_t serial) {
  // For values not specified in pending_configure_state_, use the latest
  // requested values.
  auto state = GetLatestRequestedState();
  if (pending_configure_state_.bounds_dip.has_value()) {
    state.bounds_dip = pending_configure_state_.bounds_dip.value();
  }
  if (pending_configure_state_.size_px.has_value()) {
    state.size_px = pending_configure_state_.size_px.value();
  }
  if (pending_configure_state_.raster_scale.has_value()) {
    state.raster_scale = pending_configure_state_.raster_scale.value();
  }

  if (state.bounds_dip.IsEmpty() &&
      GetPlatformWindowState() == PlatformWindowState::kMinimized &&
      in_flight_requests_.empty()) {
    // In exo, widget creation is deferred until the surface has contents and
    // |initial_show_state_| for a widget is ignored. Exo sends a configure
    // callback with empty bounds expecting client to suggest a size.
    // For the window activated from minimized state,
    // the saved window placement should be set as window geometry.
    state.bounds_dip = GetBoundsInDIP();
    // As per spec, width and height must be greater than zero.
    if (state.bounds_dip.IsEmpty()) {
      state.bounds_dip = gfx::Rect(0, 0, 1, 1);
    }
  }

  RequestStateFromServer(state, serial);

  // Reset values.
  pending_configure_state_ = PendingConfigureState();
}

void WaylandWindow::RequestStateFromServer(PlatformWindowDelegate::State state,
                                           int64_t serial) {
  RequestState(state, serial, /*force=*/false);
}

void WaylandWindow::RequestStateFromClient(
    PlatformWindowDelegate::State state) {
  // In general, client requested changes should not be throttled so force
  // apply this.
  RequestState(state, /*serial=*/-1, /*force=*/true);
}

void WaylandWindow::RequestState(PlatformWindowDelegate::State state,
                                 int64_t serial,
                                 bool force) {
  LOG_IF(WARNING, in_flight_requests_.size() > 100u)
      << "The queue of configures is longer than 100!";

  // If there are no in-flight requests, then the applied state should be the
  // latched state, because in flight configure requests are only removed on
  // latch.
  if (in_flight_requests_.empty()) {
    DCHECK_EQ(applied_state_, latched_state_);
  }

  // Adjust state values if necessary.
  state.bounds_dip = AdjustBoundsToConstraintsDIP(state.bounds_dip);
  state.size_px =
      gfx::ScaleToEnclosingRect(state.bounds_dip, state.window_scale).size();

  StateRequest req{.state = state, .serial = serial};
  if (in_flight_requests_.empty()) {
    in_flight_requests_.push_back(req);
  } else {
    // Propagate largest serial number so far, if we have one, since we
    // can have configure requests with no serial number (value -1).
    req.serial = std::max(req.serial, in_flight_requests_.back().serial);

    if (!in_flight_requests_.back().applied) {
      // If the last request has not been applied yet, overwrite it since
      // there's no point in requesting an old state.
      in_flight_requests_.back() = req;
    } else if (in_flight_requests_.back().state == req.state) {
      // If we already asked for this configure state, we can send back a higher
      // wayland serial for ack while needing a lower viz_seq.
      in_flight_requests_.back().serial = req.serial;
    } else {
      in_flight_requests_.push_back(req);
    }
  }

  MaybeApplyLatestStateRequest(force);
}

void WaylandWindow::ProcessSequencePoint(int64_t viz_seq) {
  // If the sequence number is -1, it means there was a GPU process crash.
  // In this case, latch any existing frames.
  if (viz_seq == -1) {
    viz_seq = INT64_MAX;
  }

  // Find last applied configure request satisfied by |seq|.
  auto iter = in_flight_requests_.end();
  for (auto i = in_flight_requests_.begin(); i != in_flight_requests_.end();
       ++i) {
    // The sequence number of each request should strictly monotonically
    // increase, since each request needs to produce a new sequence point.
    // Any requests that don't have a sequence id (-1) will be treated as
    // done if they have been applied. To latch a request, our sequence
    // number must be greater than or equal to the request's sequence
    // number.
    if (i->viz_seq > viz_seq && i->viz_seq != -1) {
      break;
    }
    if (i->applied) {
      iter = i;
    }
  }

  if (iter == in_flight_requests_.end()) {
    return;
  }

  // Latch the latest state which was actually applied.
  LatchStateRequest(*iter);

  in_flight_requests_.erase(in_flight_requests_.begin(), ++iter);

  // Now a new state is latched we may want to apply previously throttled
  // requests.
  MaybeApplyLatestStateRequest(/*force=*/false);
}

gfx::Rect WaylandWindow::AdjustBoundsToConstraintsPx(
    const gfx::Rect& bounds_px) {
  gfx::Rect adjusted_bounds_px = bounds_px;
  if (const auto min_size = delegate_->GetMinimumSizeForWindow()) {
    gfx::Size min_size_in_px =
        delegate()->ConvertRectToPixels(gfx::Rect(*min_size)).size();
    if (min_size_in_px.width() > 0 &&
        adjusted_bounds_px.width() < min_size_in_px.width()) {
      adjusted_bounds_px.set_width(min_size_in_px.width());
    }
    if (min_size_in_px.height() > 0 &&
        adjusted_bounds_px.height() < min_size_in_px.height()) {
      adjusted_bounds_px.set_height(min_size_in_px.height());
    }
  }
  if (const auto max_size = delegate_->GetMaximumSizeForWindow()) {
    gfx::Size max_size_in_px =
        delegate()->ConvertRectToPixels(gfx::Rect(*max_size)).size();
    if (max_size_in_px.width() > 0 &&
        adjusted_bounds_px.width() > max_size_in_px.width()) {
      adjusted_bounds_px.set_width(max_size_in_px.width());
    }
    if (max_size_in_px.height() > 0 &&
        adjusted_bounds_px.height() > max_size_in_px.height()) {
      adjusted_bounds_px.set_height(max_size_in_px.height());
    }
  }
  return adjusted_bounds_px;
}

gfx::Rect WaylandWindow::AdjustBoundsToConstraintsDIP(
    const gfx::Rect& bounds_dip) {
  gfx::Rect adjusted_bounds_dip = bounds_dip;
  if (const auto min_size_dip = delegate_->GetMinimumSizeForWindow()) {
    if (min_size_dip->width() > 0 &&
        adjusted_bounds_dip.width() < min_size_dip->width()) {
      adjusted_bounds_dip.set_width(min_size_dip->width());
    }
    if (min_size_dip->height() > 0 &&
        adjusted_bounds_dip.height() < min_size_dip->height()) {
      adjusted_bounds_dip.set_height(min_size_dip->height());
    }
  }
  if (const auto max_size_dip = delegate_->GetMaximumSizeForWindow()) {
    if (max_size_dip->width() > 0 &&
        adjusted_bounds_dip.width() > max_size_dip->width()) {
      adjusted_bounds_dip.set_width(max_size_dip->width());
    }
    if (max_size_dip->height() > 0 &&
        adjusted_bounds_dip.height() > max_size_dip->height()) {
      adjusted_bounds_dip.set_height(max_size_dip->height());
    }
  }
  return adjusted_bounds_dip;
}

void WaylandWindow::LatchStateRequest(const StateRequest& req) {
  // Latch the most up to date state we have a frame back for.
  auto old_state = latched_state_;
  latched_state_ = req.state;

  if (req.state.bounds_dip.size() != old_state.bounds_dip.size()) {
    SetWindowGeometry(req.state.bounds_dip.size());
  }
  UpdateWindowMask();
  if (req.serial != -1) {
    AckConfigure(req.serial);
  }
}

void WaylandWindow::MaybeApplyLatestStateRequest(bool force) {
  if (in_flight_requests_.empty()) {
    return;
  }

  if (!force) {
    int in_flight_applied = base::ranges::count_if(
        in_flight_requests_,
        [](const StateRequest& req) { return req.applied; });

    // Allow at most 3 configure requests to be waited on at a time.
    constexpr int MAX_IN_FLIGHT_REQUESTS = 3;
    if (in_flight_applied >= MAX_IN_FLIGHT_REQUESTS) {
      return;
    }
  }

  auto& latest = in_flight_requests_.back();
  if (latest.applied) {
    return;
  }
  latest.applied = true;

  // Set the applied state here so it can be used by e.g. OnBoundsChanged to
  // pick up the new bounds.
  auto old = applied_state_;
  applied_state_ = latest.state;

  // OnStateUpdate may return -1 if the state update does not require a new
  // frame to be considered synchronized. For example, this can happen if the
  // old and new states are the same, or it only changes the origin of the
  // bounds.
  latest.viz_seq = delegate()->OnStateUpdate(old, latest.state);

  // If we have state requests which don't require synchronization to latch, or
  // if no frames will be produced, ack them immediately. Using -2 (or any
  // negative number that isn't -1) will cause all requests with viz_seq==-1 to
  // be latched. We don't use -1 because ProcessSequencePoint has a special case
  // to re-map -1 to a large number to handle GPU process crashes.
  constexpr int64_t kLatchAllWithoutVizSeq = -2;
  ProcessSequencePoint(kLatchAllWithoutVizSeq);

  // Latch in tests immediately if the test config is set.
  // Otherwise, such tests as interactive_ui_tests fail.
  if (UseTestConfigForPlatformWindows()) {
    ProcessSequencePoint(INT64_MAX);
  }
}

}  // namespace ui
