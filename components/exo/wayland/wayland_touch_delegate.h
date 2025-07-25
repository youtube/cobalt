// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_TOUCH_DELEGATE_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_TOUCH_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/exo/touch_delegate.h"
#include "components/exo/wayland/wayland_input_delegate.h"

struct wl_client;
struct wl_resource;

namespace exo {
namespace wayland {
class SerialTracker;

// Touch delegate class that accepts events for surfaces owned by the same
// client as a touch resource.
class WaylandTouchDelegate : public WaylandInputDelegate, public TouchDelegate {
 public:
  explicit WaylandTouchDelegate(wl_resource* touch_resource,
                                SerialTracker* serial_tracker);

  WaylandTouchDelegate(const WaylandTouchDelegate&) = delete;
  WaylandTouchDelegate& operator=(const WaylandTouchDelegate&) = delete;

  // Overridden from TouchDelegate:
  void OnTouchDestroying(Touch* touch) override;
  bool CanAcceptTouchEventsForSurface(Surface* surface) const override;
  void OnTouchDown(Surface* surface,
                   base::TimeTicks time_stamp,
                   int id,
                   const gfx::PointF& location) override;
  void OnTouchUp(base::TimeTicks time_stamp, int id) override;
  void OnTouchMotion(base::TimeTicks time_stamp,
                     int id,
                     const gfx::PointF& location) override;
  void OnTouchShape(int id, float major, float minor) override;
  void OnTouchFrame() override;
  void OnTouchCancel() override;

 private:
  // The client who own this touch instance.
  wl_client* client() const;

  // The touch resource associated with the touch.
  const raw_ptr<wl_resource, ExperimentalAsh> touch_resource_;

  // Owned by Server, which always outlives this delegate.
  const raw_ptr<SerialTracker, ExperimentalAsh> serial_tracker_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_TOUCH_DELEGATE_H_
