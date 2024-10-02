// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class PointF;
class Vector2dF;
}  // namespace gfx

namespace wl {
enum class EventDispatchPolicy;
}  // namespace wl

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Wraps the wl_pointer object and injects event data through
// |WaylandPointer::Delegate| interface.
class WaylandPointer {
 public:
  class Delegate;

  WaylandPointer(wl_pointer* pointer,
                 WaylandConnection* connection,
                 Delegate* delegate);

  WaylandPointer(const WaylandPointer&) = delete;
  WaylandPointer& operator=(const WaylandPointer&) = delete;

  virtual ~WaylandPointer();

  uint32_t id() const { return obj_.id(); }
  wl_pointer* wl_object() const { return obj_.get(); }

 private:
  // wl_pointer_listener
  static void Enter(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_fixed_t surface_x,
                    wl_fixed_t surface_y);
  static void Leave(void* data,
                    wl_pointer* obj,
                    uint32_t serial,
                    wl_surface* surface);
  static void Motion(void* data,
                     wl_pointer* obj,
                     uint32_t time,
                     wl_fixed_t surface_x,
                     wl_fixed_t surface_y);
  static void Button(void* data,
                     wl_pointer* obj,
                     uint32_t serial,
                     uint32_t time,
                     uint32_t button,
                     uint32_t state);
  static void Axis(void* data,
                   wl_pointer* obj,
                   uint32_t time,
                   uint32_t axis,
                   wl_fixed_t value);
  static void Frame(void* data, wl_pointer* obj);
  static void AxisSource(void* data, wl_pointer* obj, uint32_t axis_source);
  static void AxisStop(void* data,
                       wl_pointer* obj,
                       uint32_t time,
                       uint32_t axis);
  static void AxisDiscrete(void* data,
                           wl_pointer* obj,
                           uint32_t axis,
                           int32_t discrete);
  static void AxisValue120(void* data,
                           wl_pointer* obj,
                           uint32_t axis,
                           int32_t value120);

  void SetupStylus();

  // zcr_pointer_stylus_v2_listener
  static void Tool(void* data, struct zcr_pointer_stylus_v2* x, uint32_t y);
  static void Force(void* data,
                    struct zcr_pointer_stylus_v2* x,
                    uint32_t y,
                    wl_fixed_t z);
  static void Tilt(void* data,
                   struct zcr_pointer_stylus_v2* x,
                   uint32_t y,
                   wl_fixed_t z,
                   wl_fixed_t a);

  wl::Object<wl_pointer> obj_;
  wl::Object<zcr_pointer_stylus_v2> zcr_pointer_stylus_v2_;
  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<Delegate> delegate_;

  // Whether the axis source event has been received for the current frame.
  //
  // The axis source event is optional, and the frame event can be sent with no
  // source set previously.  However, the delegate expects the axis source to be
  // set explicitly for the axis events.  Hence, we set the default source when
  // possible so that the sequence of pointer events has it set.
  bool axis_source_received_ = false;
};

class WaylandPointer::Delegate {
 public:
  virtual void OnPointerFocusChanged(
      WaylandWindow* window,
      const gfx::PointF& location,
      wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnPointerButtonEvent(
      EventType evtype,
      int changed_button,
      WaylandWindow* window,
      wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnPointerButtonEvent(EventType evtype,
                                    int changed_button,
                                    WaylandWindow* window,
                                    wl::EventDispatchPolicy dispatch_policy,
                                    bool allow_release_of_unpressed_button) = 0;
  virtual void OnPointerMotionEvent(
      const gfx::PointF& location,
      wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnPointerAxisEvent(const gfx::Vector2dF& offset) = 0;
  virtual void OnPointerFrameEvent() = 0;
  virtual void OnPointerAxisSourceEvent(uint32_t axis_source) = 0;
  virtual void OnPointerAxisStopEvent(uint32_t axis) = 0;
  virtual void OnResetPointerFlags() = 0;
  virtual const gfx::PointF& GetPointerLocation() const = 0;
  virtual bool IsPointerButtonPressed(EventFlags button) const = 0;
  virtual void OnPointerStylusToolChanged(EventPointerType pointer_type) = 0;
  virtual void OnPointerStylusForceChanged(float force) = 0;
  virtual void OnPointerStylusTiltChanged(const gfx::Vector2dF& tilt) = 0;
  virtual const WaylandWindow* GetPointerTarget() const = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POINTER_H_
