// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/events/pointer_details.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace wl {
enum class EventDispatchPolicy;
}

namespace ui {

class WaylandConnection;
class WaylandWindow;

class WaylandTouch {
 public:
  class Delegate;

  WaylandTouch(wl_touch* touch,
               WaylandConnection* connection,
               Delegate* delegate);

  WaylandTouch(const WaylandTouch&) = delete;
  WaylandTouch& operator=(const WaylandTouch&) = delete;

  ~WaylandTouch();

  uint32_t id() const { return obj_.id(); }

 private:
  // wl_touch_listener
  static void Down(void* data,
                   wl_touch* obj,
                   uint32_t serial,
                   uint32_t time,
                   struct wl_surface* surface,
                   int32_t id,
                   wl_fixed_t x,
                   wl_fixed_t y);
  static void Up(void* data,
                 wl_touch* obj,
                 uint32_t serial,
                 uint32_t time,
                 int32_t id);
  static void Motion(void* data,
                     wl_touch* obj,
                     uint32_t time,
                     int32_t id,
                     wl_fixed_t x,
                     wl_fixed_t y);
  static void Shape(void* data,
                    wl_touch* obj,
                    int32_t id,
                    wl_fixed_t major,
                    wl_fixed_t minor);
  static void Orientation(void* data,
                          wl_touch* obj,
                          int32_t id,
                          wl_fixed_t orientation);
  static void Cancel(void* data, wl_touch* obj);
  static void Frame(void* data, wl_touch* obj);

  void SetupStylus();

  // zcr_touch_stylus_v2_listener
  static void Tool(void* data,
                   struct zcr_touch_stylus_v2* obj,
                   uint32_t id,
                   uint32_t type);
  static void Force(void* data,
                    struct zcr_touch_stylus_v2* obj,
                    uint32_t time,
                    uint32_t id,
                    wl_fixed_t force);
  static void Tilt(void* data,
                   struct zcr_touch_stylus_v2* obj,
                   uint32_t time,
                   uint32_t id,
                   wl_fixed_t tilt_x,
                   wl_fixed_t tilt_y);

  wl::Object<wl_touch> obj_;
  wl::Object<zcr_touch_stylus_v2> zcr_touch_stylus_v2_;
  const raw_ptr<WaylandConnection> connection_;
  const raw_ptr<Delegate> delegate_;
};

class WaylandTouch::Delegate {
 public:
  virtual void OnTouchPressEvent(WaylandWindow* window,
                                 const gfx::PointF& location,
                                 base::TimeTicks timestamp,
                                 PointerId id,
                                 wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchReleaseEvent(base::TimeTicks timestamp,
                                   PointerId id,
                                   wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchMotionEvent(const gfx::PointF& location,
                                  base::TimeTicks timestamp,
                                  PointerId id,
                                  wl::EventDispatchPolicy dispatch_policy) = 0;
  virtual void OnTouchCancelEvent() = 0;
  virtual void OnTouchFrame() = 0;
  virtual void OnTouchFocusChanged(WaylandWindow* window) = 0;
  virtual std::vector<PointerId> GetActiveTouchPointIds() = 0;
  virtual const WaylandWindow* GetTouchTarget(PointerId id) const = 0;
  virtual void OnTouchStylusToolChanged(PointerId pointer_id,
                                        EventPointerType pointer_type) = 0;
  virtual void OnTouchStylusForceChanged(PointerId pointer_id, float force) = 0;
  virtual void OnTouchStylusTiltChanged(PointerId pointer_id,
                                        const gfx::Vector2dF& tilt) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_TOUCH_H_
