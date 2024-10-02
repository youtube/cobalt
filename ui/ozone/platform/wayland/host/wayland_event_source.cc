// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_source.h"

#include <memory>

#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/platform/wayland/wayland_event_watcher.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

bool HasAnyPointerButtonFlag(int flags) {
  return (flags & (EF_LEFT_MOUSE_BUTTON | EF_MIDDLE_MOUSE_BUTTON |
                   EF_RIGHT_MOUSE_BUTTON | EF_BACK_MOUSE_BUTTON |
                   EF_FORWARD_MOUSE_BUTTON)) != 0;
}

std::vector<uint8_t> ToLittleEndianByteVector(uint32_t value) {
  return {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
          static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
}

EventTarget* GetRootTarget(EventTarget* target) {
  EventTarget* parent = target->GetParentTarget();
  return parent ? GetRootTarget(parent) : target;
}

gfx::Point GetOriginInScreen(WaylandWindow* target) {
  // The origin for located events and positions of popup windows is the window
  // geometry.
  // See https://crbug.com/1292486
  gfx::Point origin = target->GetBoundsInDIP().origin() -
                      target->GetWindowGeometryOffsetInDIP();
  auto* parent = static_cast<WaylandWindow*>(target->GetParentTarget());
  while (parent) {
    origin += parent->GetBoundsInDIP().origin().OffsetFromOrigin();
    parent = static_cast<WaylandWindow*>(parent->GetParentTarget());
  }
  return origin;
}

gfx::Point GetLocationInScreen(LocatedEvent* event) {
  auto* root_window =
      static_cast<WaylandWindow*>(GetRootTarget(event->target()));
  return event->root_location() +
         root_window->GetBoundsInDIP().origin().OffsetFromOrigin();
}

void SetRootLocation(LocatedEvent* event) {
  gfx::PointF location = event->location_f();
  auto* target = static_cast<WaylandWindow*>(event->target());

  while (target->GetParentTarget()) {
    location += target->GetBoundsInDIP().origin().OffsetFromOrigin();
    target = static_cast<WaylandWindow*>(target->GetParentTarget());
  }
  event->set_root_location_f(location);
}

// Number of fingers for scroll gestures.
constexpr int kGestureScrollFingerCount = 2;

// Maximum size of the latest pointer scroll data set to be stored.
constexpr int kPointerScrollDataSetMaxSize = 3;

// Maximum time delta between last scroll event and lifting of fingers.
constexpr int kFlingStartTimeoutMs = 200;

}  // namespace

struct WaylandEventSource::TouchPoint {
  TouchPoint(gfx::PointF location, WaylandWindow* current_window);
  ~TouchPoint() = default;

  raw_ptr<WaylandWindow, DanglingUntriaged> window;
  gfx::PointF last_known_location;
};

WaylandEventSource::TouchPoint::TouchPoint(gfx::PointF location,
                                           WaylandWindow* current_window)
    : window(current_window), last_known_location(location) {
  DCHECK(window);
}

// WaylandEventSource::PointerScrollData implementation
WaylandEventSource::PointerScrollData::PointerScrollData() = default;
WaylandEventSource::PointerScrollData::PointerScrollData(
    const PointerScrollData&) = default;
WaylandEventSource::PointerScrollData::PointerScrollData(PointerScrollData&&) =
    default;
WaylandEventSource::PointerScrollData::~PointerScrollData() = default;

WaylandEventSource::PointerScrollData&
WaylandEventSource::PointerScrollData::operator=(const PointerScrollData&) =
    default;
WaylandEventSource::PointerScrollData&
WaylandEventSource::PointerScrollData::operator=(PointerScrollData&&) = default;

// WaylandEventSource::FrameData implementation
WaylandEventSource::FrameData::FrameData(const Event& e,
                                         base::OnceCallback<void()> cb)
    : event(e.Clone()), completion_cb(std::move(cb)) {}

WaylandEventSource::FrameData::~FrameData() = default;

// WaylandEventSource implementation

// static
void WaylandEventSource::ConvertEventToTarget(const EventTarget* new_target,
                                              LocatedEvent* event) {
  auto* current_target = static_cast<WaylandWindow*>(event->target());
  gfx::Vector2d diff = GetOriginInScreen(current_target) -
                       GetOriginInScreen(static_cast<WaylandWindow*>(
                           const_cast<EventTarget*>(new_target)));
  event->set_location_f(event->location_f() + diff);
}

WaylandEventSource::WaylandEventSource(wl_display* display,
                                       wl_event_queue* event_queue,
                                       WaylandWindowManager* window_manager,
                                       WaylandConnection* connection)
    : window_manager_(window_manager),
      connection_(connection),
      event_watcher_(
          WaylandEventWatcher::CreateWaylandEventWatcher(display,
                                                         event_queue)) {
  DCHECK(window_manager_);

  // Observes remove changes to know when touch points can be removed.
  window_manager_->AddObserver(this);
}

WaylandEventSource::~WaylandEventSource() = default;

void WaylandEventSource::SetShutdownCb(base::OnceCallback<void()> shutdown_cb) {
  event_watcher_->SetShutdownCb(std::move(shutdown_cb));
}

void WaylandEventSource::StartProcessingEvents() {
  event_watcher_->StartProcessingEvents();
}

void WaylandEventSource::OnKeyboardFocusChanged(WaylandWindow* window,
                                                bool focused) {
  DCHECK(window);
#if DCHECK_IS_ON()
  if (!focused)
    DCHECK_EQ(window, window_manager_->GetCurrentKeyboardFocusedWindow());
#endif
  window_manager_->SetKeyboardFocusedWindow(focused ? window : nullptr);
}

void WaylandEventSource::OnKeyboardModifiersChanged(int modifiers) {
  keyboard_modifiers_ = modifiers;
}

uint32_t WaylandEventSource::OnKeyboardKeyEvent(
    EventType type,
    DomCode dom_code,
    bool repeat,
    absl::optional<uint32_t> serial,
    base::TimeTicks timestamp,
    int device_id,
    WaylandKeyboard::KeyEventKind kind) {
  DCHECK(type == ET_KEY_PRESSED || type == ET_KEY_RELEASED);

  DomKey dom_key;
  KeyboardCode key_code;
  auto* layout_engine = KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();
  if (!layout_engine || !layout_engine->Lookup(dom_code, keyboard_modifiers_,
                                               &dom_key, &key_code)) {
    LOG(ERROR) << "Failed to decode key event.";
    return POST_DISPATCH_NONE;
  }

#if BUILDFLAG(USE_GTK)
  // GTK expects the state of a key event to be the mask of modifier keys
  // _prior_ to this event. Some IMEs rely on this behavior. See
  // https://crbug.com/1086946#c11.
  int state_before_event = keyboard_modifiers_;
#endif

  KeyEvent event(type, key_code, dom_code,
                 keyboard_modifiers_ | (repeat ? EF_IS_REPEAT : 0), dom_key,
                 timestamp);
  event.set_source_device_id(device_id);

  auto* focus = window_manager_->GetCurrentKeyboardFocusedWindow();
  if (!focus)
    return POST_DISPATCH_STOP_PROPAGATION;

  Event::DispatcherApi(&event).set_target(focus);

  Event::Properties properties;
#if BUILDFLAG(USE_GTK)
  // GTK uses XKB keycodes.
  uint32_t converted_key_code =
      ui::KeycodeConverter::DomCodeToXkbKeycode(dom_code);
  properties.emplace(
      kPropertyKeyboardHwKeyCode,
      std::vector<uint8_t>{static_cast<unsigned char>(converted_key_code)});
  // Save state before event. The flags have different values than what GTK
  // expects, but GtkUiPlatformWayland::GetGdkKeyEventState() takes care of the
  // conversion.
  properties.emplace(kPropertyKeyboardState,
                     ToLittleEndianByteVector(state_before_event));
#endif

  if (serial.has_value()) {
    properties.emplace(WaylandKeyboard::kPropertyWaylandSerial,
                       ToLittleEndianByteVector(serial.value()));
  }

  if (kind == WaylandKeyboard::KeyEventKind::kKey) {
    // Mark that this is the key event which IME did not consume.
    SetKeyboardImeFlagProperty(&properties, kPropertyKeyboardImeIgnoredFlag);
  }
  event.SetProperties(properties);
  return DispatchEvent(&event);
}

void WaylandEventSource::OnPointerFocusChanged(
    WaylandWindow* window,
    const gfx::PointF& location,
    wl::EventDispatchPolicy dispatch_policy) {
  bool focused = !!window;
  if (focused) {
    // Save new pointer location.
    pointer_location_ = location;
    window_manager_->SetPointerFocusedWindow(window);
  }

  auto closure = focused ? base::NullCallback()
                         : base::BindOnce(
                               [](WaylandWindowManager* wwm) {
                                 wwm->SetPointerFocusedWindow(nullptr);
                               },
                               window_manager_);

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  if (target) {
    EventType type = focused ? ET_MOUSE_ENTERED : ET_MOUSE_EXITED;
    MouseEvent event(type, pointer_location_, pointer_location_,
                     EventTimeForNow(), pointer_flags_, 0);
    if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
      SetTargetAndDispatchEvent(&event, target);
    } else {
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, std::move(closure)));
      return;
    }
  }

  if (!closure.is_null())
    std::move(closure).Run();
}

void WaylandEventSource::OnPointerButtonEvent(
    EventType type,
    int changed_button,
    WaylandWindow* window,
    wl::EventDispatchPolicy dispatch_policy) {
  OnPointerButtonEvent(type, changed_button, window, dispatch_policy, false);
}

void WaylandEventSource::OnPointerButtonEvent(
    EventType type,
    int changed_button,
    WaylandWindow* window,
    wl::EventDispatchPolicy dispatch_policy,
    bool allow_release_of_unpressed_button) {
  DCHECK(type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED);
  DCHECK(HasAnyPointerButtonFlag(changed_button));

  // Ignore release events for buttons that aren't currently pressed. Such
  // events should never happen, but there have been compositor bugs before
  // (e.g. crbug.com/1376393).
  if (!allow_release_of_unpressed_button && type == ET_MOUSE_RELEASED &&
      (pointer_flags_ & changed_button) == 0)
    return;

  WaylandWindow* prev_focused_window =
      window_manager_->GetCurrentPointerFocusedWindow();
  if (window)
    window_manager_->SetPointerFocusedWindow(window);

  auto closure = base::BindOnce(
      &WaylandEventSource::OnPointerButtonEventInternal, base::Unretained(this),
      (window ? prev_focused_window : nullptr), type);

  pointer_flags_ = type == ET_MOUSE_PRESSED
                       ? (pointer_flags_ | changed_button)
                       : (pointer_flags_ & ~changed_button);
  last_pointer_button_pressed_ = changed_button;

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  // A window may be deleted when the event arrived from the server.
  if (target) {
    // MouseEvent's flags should contain the button that was released too.
    int flags = pointer_flags_ | keyboard_modifiers_ | changed_button;
    MouseEvent event(type, pointer_location_, pointer_location_,
                     EventTimeForNow(), flags, changed_button);
    if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
      SetTargetAndDispatchEvent(&event, target);
    } else {
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, std::move(closure)));
      return;
    }
  }

  if (!closure.is_null())
    std::move(closure).Run();
}

void WaylandEventSource::OnPointerButtonEventInternal(WaylandWindow* window,
                                                      EventType type) {
  if (window)
    window_manager_->SetPointerFocusedWindow(window);

  if (type == ET_MOUSE_RELEASED)
    last_pointer_stylus_data_.reset();
}

void WaylandEventSource::OnPointerMotionEvent(
    const gfx::PointF& location,
    wl::EventDispatchPolicy dispatch_policy) {
  pointer_location_ = location;

  int flags = pointer_flags_ | keyboard_modifiers_;
  MouseEvent event(ET_MOUSE_MOVED, pointer_location_, pointer_location_,
                   EventTimeForNow(), flags, 0);
  auto* target = window_manager_->GetCurrentPointerFocusedWindow();

  // A window may be deleted when the event arrived from the server.
  if (!target)
    return;

  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTargetAndDispatchEvent(&event, target);
  } else {
    pointer_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  }
}

void WaylandEventSource::OnPointerAxisEvent(const gfx::Vector2dF& offset) {
  EnsurePointerScrollData().dx += offset.x();
  EnsurePointerScrollData().dy += offset.y();
}

void WaylandEventSource::OnResetPointerFlags() {
  ResetPointerFlags();
}

void WaylandEventSource::RoundTripQueue() {
  event_watcher_->RoundTripQueue();
}

const gfx::PointF& WaylandEventSource::GetPointerLocation() const {
  return pointer_location_;
}

void WaylandEventSource::OnPointerFrameEvent() {
  base::TimeTicks now = EventTimeForNow();
  if (pointer_scroll_data_) {
    pointer_scroll_data_->dt = now - last_pointer_frame_time_;
    ProcessPointerScrollData();
  }

  last_pointer_frame_time_ = now;

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  if (!target)
    return;

  while (!pointer_frames_.empty()) {
    // It is safe to pop the first queued event for processing.
    auto pointer_frame = std::move(pointer_frames_.front());
    pointer_frames_.pop_front();

    // In case there are pointer stylus information, override the current
    // 'event' instance, given that PointerDetails is 'const'.
    auto pointer_details_with_stylus_data = AmendStylusData();
    if (pointer_details_with_stylus_data &&
        pointer_frame->event->IsMouseEvent() &&
        pointer_frame->event->AsMouseEvent()->IsOnlyLeftMouseButton()) {
      auto old_event = std::move(pointer_frame->event);
      pointer_frame->event = std::make_unique<MouseEvent>(
          old_event->type(), old_event->AsMouseEvent()->location(),
          old_event->AsMouseEvent()->root_location(), old_event->time_stamp(),
          old_event->flags(), old_event->AsMouseEvent()->changed_button_flags(),
          pointer_details_with_stylus_data.value());
    }

    SetTargetAndDispatchEvent(pointer_frame->event.get(), target);
    if (!pointer_frame->completion_cb.is_null())
      std::move(pointer_frame->completion_cb).Run();
  }
}

void WaylandEventSource::OnPointerAxisSourceEvent(uint32_t axis_source) {
  EnsurePointerScrollData().axis_source = axis_source;
}

void WaylandEventSource::OnPointerAxisStopEvent(uint32_t axis) {
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
    EnsurePointerScrollData().dy = 0;
  } else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
    EnsurePointerScrollData().dx = 0;
  }
  EnsurePointerScrollData().is_axis_stop = true;
}

void WaylandEventSource::OnTouchPressEvent(
    WaylandWindow* window,
    const gfx::PointF& location,
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  DCHECK(window);
  HandleTouchFocusChange(window, true);

  // Make sure this touch point wasn't present before.
  auto success = touch_points_.try_emplace(
      id, std::make_unique<TouchPoint>(location, window));
  if (!success.second) {
    LOG(WARNING) << "Touch down fired with wrong id";
    return;
  }

  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_PRESSED, location, location, timestamp, details,
                   keyboard_modifiers_);
  touch_frames_.push_back(
      std::make_unique<FrameData>(event, base::NullCallback()));
}

void WaylandEventSource::OnTouchReleaseEvent(
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  // Make sure this touch point was present before.
  const auto it = touch_points_.find(id);
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch up fired with no matching touch down";
    return;
  }

  TouchPoint* touch_point = it->second.get();
  gfx::PointF location = touch_point->last_known_location;
  PointerDetails details(EventPointerType::kTouch, id);

  TouchEvent event(ET_TOUCH_RELEASED, location, location, timestamp, details,
                   keyboard_modifiers_);
  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTouchTargetAndDispatchTouchEvent(&event);
    OnTouchReleaseInternal(id);
  } else {
    touch_frames_.push_back(std::make_unique<FrameData>(
        event, base::BindOnce(&WaylandEventSource::OnTouchReleaseInternal,
                              base::Unretained(this), id)));
  }
}

void WaylandEventSource::OnTouchReleaseInternal(PointerId id) {
  // It is possible that an user interaction triggers nested loops
  // in higher levels of the application stack in order to process a
  // given touch down/up action.
  // For instance, a modal dialog might block this execution point,
  // and trigger thread to continue to process events.
  // The auxiliary flow might clear entries in touch_points_.
  //
  // Hence, we check whether the TouchId is still being held.
  const auto it = touch_points_.find(id);
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch has been released during processing.";
    return;
  }

  TouchPoint* touch_point = it->second.get();
  HandleTouchFocusChange(touch_point->window, false, id);
  touch_points_.erase(it);

  // Clean up stylus touch tracking, if any.
  const auto stylus_data_it = last_touch_stylus_data_.find(id);
  if (stylus_data_it != last_touch_stylus_data_.end())
    last_touch_stylus_data_.erase(stylus_data_it);
}

void WaylandEventSource::SetTargetAndDispatchEvent(Event* event,
                                                   EventTarget* target) {
  Event::DispatcherApi(event).set_target(target);
  if (event->IsLocatedEvent()) {
    SetRootLocation(event->AsLocatedEvent());
    auto* cursor_position = connection_->wayland_cursor_position();
    if (cursor_position) {
      cursor_position->OnCursorPositionChanged(
          GetLocationInScreen(event->AsLocatedEvent()));
    }
  }
  DispatchEvent(event);
}

void WaylandEventSource::SetTouchTargetAndDispatchTouchEvent(
    TouchEvent* event) {
  auto iter = touch_points_.find(event->pointer_details().id);
  auto target = iter != touch_points_.end() ? iter->second->window : nullptr;
  // Skip if the touch target has alrady been removed.
  if (!target.get())
    return;
  SetTargetAndDispatchEvent(event, target.get());
}

void WaylandEventSource::OnTouchMotionEvent(
    const gfx::PointF& location,
    base::TimeTicks timestamp,
    PointerId id,
    wl::EventDispatchPolicy dispatch_policy) {
  const auto it = touch_points_.find(id);
  // Make sure this touch point was present before.
  if (it == touch_points_.end()) {
    LOG(WARNING) << "Touch event fired with wrong id";
    return;
  }
  it->second->last_known_location = location;
  PointerDetails details(EventPointerType::kTouch, id);
  TouchEvent event(ET_TOUCH_MOVED, location, location, timestamp, details,
                   keyboard_modifiers_);
  if (dispatch_policy == wl::EventDispatchPolicy::kImmediate) {
    SetTouchTargetAndDispatchTouchEvent(&event);
  } else {
    touch_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  }
}

void WaylandEventSource::OnTouchCancelEvent() {
  // Some compositors emit a TouchCancel event when a drag'n drop
  // session is started on the server, eg Exo.
  // On Chrome, this event would actually abort the whole drag'n drop
  // session on the client side.
  if (connection_->IsDragInProgress())
    return;

  gfx::PointF location;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  for (auto& touch_point : touch_points_) {
    PointerId id = touch_point.first;
    TouchEvent event(ET_TOUCH_CANCELLED, location, location, timestamp,
                     PointerDetails(EventPointerType::kTouch, id));
    SetTouchTargetAndDispatchTouchEvent(&event);
    HandleTouchFocusChange(touch_point.second->window, false);
  }
  touch_points_.clear();
  last_touch_stylus_data_.clear();
}

void WaylandEventSource::OnTouchFrame() {
  while (!touch_frames_.empty()) {
    // It is safe to pop the first queued event for processing.
    auto touch_frame = std::move(touch_frames_.front());
    touch_frames_.pop_front();

    // In case there are touch stylus information, override the current 'event'
    // instance, given that PointerDetails is 'const'.
    auto pointer_details_with_stylus_data = AmendStylusData(
        touch_frame->event->AsTouchEvent()->pointer_details().id);
    if (pointer_details_with_stylus_data) {
      auto old_event = std::move(touch_frame->event);
      touch_frame->event = std::make_unique<TouchEvent>(
          old_event->type(), old_event->AsTouchEvent()->location_f(),
          old_event->AsTouchEvent()->root_location_f(), old_event->time_stamp(),
          pointer_details_with_stylus_data.value(), old_event->flags());
    }
    SetTouchTargetAndDispatchTouchEvent(touch_frame->event->AsTouchEvent());
    if (!touch_frame->completion_cb.is_null())
      std::move(touch_frame->completion_cb).Run();
  }
}

void WaylandEventSource::OnTouchFocusChanged(WaylandWindow* window) {
  window_manager_->SetTouchFocusedWindow(window);
}

std::vector<PointerId> WaylandEventSource::GetActiveTouchPointIds() {
  std::vector<PointerId> pointer_ids;
  for (auto& touch_point : touch_points_)
    pointer_ids.push_back(touch_point.first);
  return pointer_ids;
}

void WaylandEventSource::OnTouchStylusToolChanged(
    PointerId pointer_id,
    EventPointerType pointer_type) {
  StylusData stylus_data = {.type = pointer_type,
                            .tilt = gfx::Vector2dF(),
                            .force = std::numeric_limits<float>::quiet_NaN()};
  bool inserted =
      last_touch_stylus_data_.try_emplace(pointer_id, stylus_data).second;
  DCHECK(inserted);
}

void WaylandEventSource::OnTouchStylusForceChanged(PointerId pointer_id,
                                                   float force) {
  DCHECK(last_touch_stylus_data_[pointer_id].has_value());
  last_touch_stylus_data_[pointer_id]->force = force;
}

void WaylandEventSource::OnTouchStylusTiltChanged(PointerId pointer_id,
                                                  const gfx::Vector2dF& tilt) {
  DCHECK(last_touch_stylus_data_[pointer_id].has_value());
  last_touch_stylus_data_[pointer_id]->tilt = tilt;
}

const WaylandWindow* WaylandEventSource::GetTouchTarget(PointerId id) const {
  const auto it = touch_points_.find(id);
  return it == touch_points_.end() ? nullptr : it->second->window.get();
}

void WaylandEventSource::OnPinchEvent(EventType event_type,
                                      const gfx::Vector2dF& delta,
                                      base::TimeTicks timestamp,
                                      int device_id,
                                      absl::optional<float> scale_delta) {
  GestureEventDetails details(event_type);
  details.set_device_type(GestureDeviceType::DEVICE_TOUCHPAD);
  if (scale_delta)
    details.set_scale(*scale_delta);

  auto location = pointer_location_ + delta;
  GestureEvent event(location.x(), location.y(), 0 /* flags */, timestamp,
                     details);
  event.set_source_device_id(device_id);

  auto* target = window_manager_->GetCurrentPointerFocusedWindow();
  // A window may be deleted when the event arrived from the server.
  if (!target)
    return;

  SetTargetAndDispatchEvent(&event, target);
}

void WaylandEventSource::SetRelativePointerMotionEnabled(bool enabled) {
  if (enabled)
    relative_pointer_location_ = pointer_location_;
  else
    relative_pointer_location_.reset();
}

void WaylandEventSource::OnRelativePointerMotion(const gfx::Vector2dF& delta) {
  DCHECK(relative_pointer_location_.has_value());
  // TODO(oshima): Investigate if we need to scale the delta
  // when surface_submission_in_pixel_coordinates is on.
  relative_pointer_location_ = *relative_pointer_location_ + delta;
  OnPointerMotionEvent(*relative_pointer_location_,
                       wl::EventDispatchPolicy::kImmediate);
}

bool WaylandEventSource::IsPointerButtonPressed(EventFlags button) const {
  DCHECK(HasAnyPointerButtonFlag(button));
  return pointer_flags_ & button;
}

void WaylandEventSource::OnPointerStylusToolChanged(
    EventPointerType pointer_type) {
  // When the reported pointer stylus type is `mouse`, handle it as a regular
  // pointer event.
  //
  // TODO(https://crbug.com/1298504): Better handle the `touch` type, which
  // seems mis-specified in
  // //t_p/wayland-protocols/unstable/stylus/stylus-unstable-v2.xml.
  if (pointer_type == ui::EventPointerType::kMouse) {
    last_pointer_stylus_data_.reset();
    return;
  }

  last_pointer_stylus_data_ = {
      .type = pointer_type,
      .tilt = gfx::Vector2dF(),
      .force = std::numeric_limits<float>::quiet_NaN()};
}

void WaylandEventSource::OnPointerStylusForceChanged(float force) {
  if (!last_pointer_stylus_data_.has_value()) {
    // This is a stray force event that the default tool cannot accept.
    LOG(WARNING) << "Cannot handle force for the default tool!  (the value is "
                 << force << ")";
    return;
  }

  last_pointer_stylus_data_->force = force;
}

void WaylandEventSource::OnPointerStylusTiltChanged(
    const gfx::Vector2dF& tilt) {
  if (!last_pointer_stylus_data_.has_value()) {
    // This is a stray tilt event that the default tool cannot accept.
    LOG(WARNING) << "Cannot handle tilt for the default tool!  (the value is ["
                 << tilt.x() << "," << tilt.y() << "])";
    return;
  }

  last_pointer_stylus_data_->tilt = tilt;
}

const WaylandWindow* WaylandEventSource::GetPointerTarget() const {
  return window_manager_->GetCurrentPointerFocusedWindow();
}

void WaylandEventSource::ResetPointerFlags() {
  pointer_flags_ = 0;
}

void WaylandEventSource::OnDispatcherListChanged() {
  StartProcessingEvents();
}

void WaylandEventSource::OnWindowRemoved(WaylandWindow* window) {
  // A window can be `swallowed` by another window during tab-dragging, which
  // results in OnWindowRemoved() being called.
  //
  // If a window dragging session is active and is touch-based, verify if there
  // is a valid target window to transfer the touch points to.
  if (auto* target_window = window_manager_->GetCurrentTouchFocusedWindow()) {
    auto drag_source = connection_->window_drag_controller()->drag_source();
    if (drag_source && *drag_source == mojom::DragEventSource::kTouch) {
      for (auto& touch_point : touch_points_)
        touch_point.second->window = target_window;
      return;
    }
  }

  // Clear touch-related data.
  base::EraseIf(touch_points_, [window](const auto& point) {
    return point.second->window == window;
  });
}

void WaylandEventSource::HandleTouchFocusChange(WaylandWindow* window,
                                                bool focused,
                                                absl::optional<PointerId> id) {
  DCHECK(window);
  bool actual_focus = id ? !ShouldUnsetTouchFocus(window, id.value()) : focused;
  window->set_touch_focus(actual_focus);
}

// Focus must not be unset if there is another touch point within |window|.
bool WaylandEventSource::ShouldUnsetTouchFocus(WaylandWindow* win,
                                               PointerId id) {
  return base::ranges::none_of(touch_points_, [win, id](auto& p) {
    return p.second->window == win && p.first != id;
  });
}

gfx::Vector2dF WaylandEventSource::ComputeFlingVelocity() {
  struct RegressionSums {
    float tt_;  // Cumulative sum of t^2.
    float t_;   // Cumulative sum of t.
    float tx_;  // Cumulative sum of t * x.
    float ty_;  // Cumulative sum of t * y.
    float x_;   // Cumulative sum of x.
    float y_;   // Cumulative sum of y.
  };

  const size_t count = pointer_scroll_data_set_.size();

  if (count == 0) {
    return gfx::Vector2dF();
  }

  // Prevents small jumps if someone scrolls fast, immediately stops scrolling
  // and then waits a little before lifting fingers from touchpad.
  if (pointer_scroll_data_->dt > base::Milliseconds(kFlingStartTimeoutMs)) {
    return gfx::Vector2dF();
  }

  if (count == 1) {
    const auto& pointer_frame = pointer_scroll_data_set_.front();
    const float dt =
        pointer_frame.dt.InSecondsF() + pointer_scroll_data_->dt.InSecondsF();
    return gfx::Vector2dF(pointer_frame.dx * dt, pointer_frame.dy * dt);
  }

  RegressionSums sums = {0, 0, 0, 0, 0, 0};

  float time = pointer_scroll_data_->dt.InSecondsF();
  float x_coord = 0;
  float y_coord = 0;

  // Formula matches libgestures's RegressScrollVelocity()
  // from src/platform/gestures/src/immediate_interpreter.cc
  for (const auto& frame : pointer_scroll_data_set_) {
    if (frame.axis_source &&
        *frame.axis_source != WL_POINTER_AXIS_SOURCE_FINGER) {
      break;
    }
    time += frame.dt.InSecondsF();
    x_coord += frame.dx;
    y_coord += frame.dy;

    sums.tt_ += time * time;
    sums.t_ += time;
    sums.tx_ += time * x_coord;
    sums.ty_ += time * y_coord;
    sums.x_ += x_coord;
    sums.y_ += y_coord;
  }
  pointer_scroll_data_set_.clear();

  // Note the regression determinant only depends on the values of t, and should
  // never be zero so long as (1) count > 1, and (2) dt[0] != d[1]. The
  // condition of (1) was already caught at the beginning of the method.
  const float det = count * sums.tt_ - sums.t_ * sums.t_;
  if (!det) {
    // This will return the average scroll value if dt values are
    // non-zero.
    if (sums.t_) {
      return gfx::Vector2dF(x_coord / sums.t_, y_coord / sums.t_);
    }
    return gfx::Vector2dF();
  }

  const float det_inv = 1.0 / det;
  return gfx::Vector2dF((count * sums.tx_ - sums.t_ * sums.x_) * det_inv,
                        (count * sums.ty_ - sums.t_ * sums.y_) * det_inv);
}

absl::optional<PointerDetails> WaylandEventSource::AmendStylusData() const {
  if (!last_pointer_stylus_data_)
    return absl::nullopt;

  DCHECK_NE(last_pointer_stylus_data_->type, EventPointerType::kUnknown);
  return PointerDetails(last_pointer_stylus_data_->type, /*pointer_id=*/0,
                        /*radius_x=*/1.0f,
                        /*radius_y=*/1.0f, last_pointer_stylus_data_->force,
                        /*twist=*/0.0f, last_pointer_stylus_data_->tilt.x(),
                        last_pointer_stylus_data_->tilt.y());
}

absl::optional<PointerDetails> WaylandEventSource::AmendStylusData(
    PointerId pointer_id) const {
  const auto it = last_touch_stylus_data_.find(pointer_id);
  if (it == last_touch_stylus_data_.end() || !it->second ||
      it->second->type == EventPointerType::kTouch) {
    return absl::nullopt;
  }

  // The values below come from the default values in pointer_details.cc|h.
  return PointerDetails(it->second->type, pointer_id,
                        /*radius_x=*/1.0f,
                        /*radius_y=*/1.0f, it->second->force,
                        /*twist=*/0.0f, it->second->tilt.x(),
                        it->second->tilt.y());
}

WaylandEventSource::PointerScrollData&
WaylandEventSource::EnsurePointerScrollData() {
  if (!pointer_scroll_data_)
    pointer_scroll_data_ = PointerScrollData();

  return *pointer_scroll_data_;
}

// This method behaves differently in Exo than in other window managers.
// If you place a finger on the touchpad, Exo dispatches axis events with an
// offset of 0 to indicate that a fling should be aborted. This event does not
// exist in Linux window managers. Instead, some window managers implement
// zwp_pointer_gesture_hold_v1 for this. However, for those who don't implement
// that, it needs to be ensured that flings are aborted when new axis events
// arrive.
void WaylandEventSource::ProcessPointerScrollData() {
  DCHECK(pointer_scroll_data_);
  int flags = pointer_flags_ | keyboard_modifiers_;
  // Dispatch Fling event if pointer.axis_stop is notified and the recent
  // pointer.axis events meets the criteria to start fling scroll.
  if (pointer_scroll_data_->dx == 0 && pointer_scroll_data_->dy == 0
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      && pointer_scroll_data_->is_axis_stop
#endif
  ) {
    gfx::Vector2dF initial_velocity = ComputeFlingVelocity();
    float vx = initial_velocity.x();
    float vy = initial_velocity.y();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ScrollEvent event(pointer_scroll_data_->is_axis_stop
                          ? ET_SCROLL_FLING_START
                          : ET_SCROLL_FLING_CANCEL,
                      pointer_location_, pointer_location_, EventTimeForNow(),
                      flags, vx, vy, vx, vy, kGestureScrollFingerCount);
#else
    // In Linux there is no axis event with 0 delta when start scrolling.
    // A fling is therefore always started at this point.
    ScrollEvent event(ET_SCROLL_FLING_START, pointer_location_,
                      pointer_location_, EventTimeForNow(), flags, vx, vy, vx,
                      vy, kGestureScrollFingerCount);
    is_fling_active_ = true;
#endif
    pointer_frames_.push_back(
        std::make_unique<FrameData>(event, base::NullCallback()));
  } else if (pointer_scroll_data_->axis_source) {
    if (*pointer_scroll_data_->axis_source == WL_POINTER_AXIS_SOURCE_WHEEL ||
        *pointer_scroll_data_->axis_source ==
            WL_POINTER_AXIS_SOURCE_WHEEL_TILT) {
      MouseWheelEvent event(
          gfx::Vector2d(pointer_scroll_data_->dx, pointer_scroll_data_->dy),
          pointer_location_, pointer_location_, EventTimeForNow(), flags, 0);
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, base::NullCallback()));
    } else if (*pointer_scroll_data_->axis_source ==
                   WL_POINTER_AXIS_SOURCE_FINGER ||
               *pointer_scroll_data_->axis_source ==
                   WL_POINTER_AXIS_SOURCE_CONTINUOUS) {
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      // Fling has to be stopped if a new scroll event is received.
      if (is_fling_active_) {
        is_fling_active_ = false;
        ScrollEvent stop_fling_event(
            ET_SCROLL_FLING_CANCEL, pointer_location_, pointer_location_,
            EventTimeForNow(), flags, 0, 0, 0, 0, kGestureScrollFingerCount);
        pointer_frames_.push_back(std::make_unique<FrameData>(
            stop_fling_event, base::NullCallback()));
      }
#endif
      ScrollEvent event(ET_SCROLL, pointer_location_, pointer_location_,
                        EventTimeForNow(), flags, pointer_scroll_data_->dx,
                        pointer_scroll_data_->dy, pointer_scroll_data_->dx,
                        pointer_scroll_data_->dy, kGestureScrollFingerCount);
      pointer_frames_.push_back(
          std::make_unique<FrameData>(event, base::NullCallback()));
    }

    if (pointer_scroll_data_set_.size() + 1 > kPointerScrollDataSetMaxSize)
      pointer_scroll_data_set_.pop_back();
    pointer_scroll_data_set_.push_front(*pointer_scroll_data_);
  }

  pointer_scroll_data_.reset();
}

}  // namespace ui
