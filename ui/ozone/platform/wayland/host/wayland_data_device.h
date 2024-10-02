// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

class WaylandDataOffer;
class WaylandConnection;
class WaylandWindow;

// This class provides access to inter-client data transfer mechanisms
// such as copy-and-paste and drag-and-drop mechanisms.
class WaylandDataDevice : public WaylandDataDeviceBase {
 public:
  using RequestDataCallback = base::OnceCallback<void(PlatformClipboard::Data)>;

  // DragDelegate is responsible for handling drag and drop sessions.
  class DragDelegate {
   public:
    virtual bool IsDragSource() const = 0;
    virtual void DrawIcon() = 0;
    virtual void OnDragOffer(std::unique_ptr<WaylandDataOffer> offer) = 0;
    virtual void OnDragEnter(WaylandWindow* window,
                             const gfx::PointF& location,
                             uint32_t serial) = 0;
    virtual void OnDragMotion(const gfx::PointF& location) = 0;
    virtual void OnDragLeave() = 0;
    virtual void OnDragDrop() = 0;

    virtual const WaylandWindow* GetDragTarget() const = 0;

   protected:
    virtual ~DragDelegate() = default;
  };

  WaylandDataDevice(WaylandConnection* connection, wl_data_device* data_device);
  WaylandDataDevice(const WaylandDataDevice&) = delete;
  WaylandDataDevice& operator=(const WaylandDataDevice&) = delete;
  ~WaylandDataDevice() override;

  // Starts a wayland drag and drop session, controlled by |delegate|.
  void StartDrag(const WaylandDataSource& data_source,
                 const WaylandWindow& origin_window,
                 uint32_t serial,
                 wl_surface* icon_surface,
                 DragDelegate* delegate);

  // Resets the drag delegate, assuming there is one set. Any wl_data_device
  // event received after this will be ignored until a new delegate is set.
  void ResetDragDelegate();
  // Resets the drag delegate, only under certain conditions, eg: if it is set
  // and running an incoming dnd session.
  // TODO(crbug.com/1401598): Drop once drag delegate improvements are done.
  void ResetDragDelegateIfNotDragSource();

  // Requests data for an |offer| in a format specified by |mime_type|. The
  // transfer happens asynchronously and |callback| is called when it is done.
  void RequestData(WaylandDataOffer* offer,
                   const std::string& mime_type,
                   RequestDataCallback callback);

  // Returns the underlying wl_data_device singleton object.
  wl_data_device* data_device() const { return data_device_.get(); }

  // wl_data_device::set_selection makes the corresponding wl_data_source the
  // target of future wl_data_device::data_offer events. In non-Wayland terms,
  // this is equivalent to "writing" to the clipboard, although the actual
  // transfer of data happens asynchronously, on-demand-only.
  void SetSelectionSource(WaylandDataSource* source, uint32_t serial);

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, StartDrag);
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, ReceiveDrag);

  void ReadDragDataFromFD(base::ScopedFD fd, RequestDataCallback callback);

  // wl_data_device_listener callbacks
  static void OnOffer(void* data,
                      wl_data_device* data_device,
                      wl_data_offer* id);

  static void OnEnter(void* data,
                      wl_data_device* data_device,
                      uint32_t serial,
                      wl_surface* surface,
                      wl_fixed_t x,
                      wl_fixed_t y,
                      wl_data_offer* offer);

  static void OnMotion(void* data,
                       struct wl_data_device* data_device,
                       uint32_t time,
                       wl_fixed_t x,
                       wl_fixed_t y);

  static void OnDrop(void* data, struct wl_data_device* data_device);

  static void OnLeave(void* data, struct wl_data_device* data_device);

  // Called by the compositor when the window gets pointer or keyboard focus,
  // or clipboard content changes behind the scenes.
  //
  // https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_data_device
  static void OnSelection(void* data,
                          wl_data_device* data_device,
                          wl_data_offer* id);

  // The wl_data_device wrapped by this WaylandDataDevice.
  wl::Object<wl_data_device> data_device_;

  raw_ptr<DragDelegate> drag_delegate_ = nullptr;

  // There are two separate data offers at a time, the drag offer and the
  // selection offer, each with independent lifetimes. When we receive a new
  // offer, it is not immediately possible to know whether the new offer is
  // the drag offer or the selection offer. This variable is used to store
  // new data offers temporarily until it is possible to determine which kind
  // session it belongs to.
  std::unique_ptr<WaylandDataOffer> new_offer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_H_
