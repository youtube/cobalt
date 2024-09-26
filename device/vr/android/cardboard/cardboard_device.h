// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_
#define DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/android/cardboard/cardboard_render_loop.h"
#include "device/vr/android/cardboard/cardboard_sdk.h"
#include "device/vr/android/compositor_delegate_provider.h"
#include "device/vr/android/mailbox_to_surface_bridge.h"
#include "device/vr/android/xr_java_coordinator.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/vr_device_base.h"

namespace device {

// Concrete implementation of a `VRDeviceBase` used to talk to the CardboardSDK
// and to manage the creation of a session with that runtime. Modeled after the
// `ArCoreDevice` class.
class COMPONENT_EXPORT(VR_CARDBOARD) CardboardDevice : public VRDeviceBase {
 public:
  CardboardDevice(
      std::unique_ptr<CardboardSdk> cardboard_sdk,
      std::unique_ptr<MailboxToSurfaceBridgeFactory>
          mailbox_to_surface_bridge_factory,
      std::unique_ptr<XrJavaCoordinator> xr_java_coordinator,
      std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider);

  ~CardboardDevice() override;

  CardboardDevice(const CardboardDevice&) = delete;
  CardboardDevice& operator=(const CardboardDevice&) = delete;

  // VRDeviceBase
  void RequestSession(
      mojom::XRRuntimeSessionOptionsPtr options,
      mojom::XRRuntime::RequestSessionCallback callback) override;
  void ShutdownSession(mojom::XRRuntime::ShutdownSessionCallback) override;

 private:
  // OnDrawingSurface* methods are used as callbacks from XrJavaCoordinator and
  // notify us of the state of things on the Java side of things.
  void OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                             gpu::SurfaceHandle surface_handle,
                             ui::WindowAndroid* root_window,
                             display::Display::Rotation rotation,
                             const gfx::Size& frame_size);
  void OnDrawingSurfaceTouch(bool is_primary,
                             bool touching,
                             int32_t pointer_id,
                             const gfx::PointF& location);
  void OnDrawingSurfaceDestroyed();

  void OnSessionEnded();
  void OnCreateSessionResult(mojom::XRRuntimeSessionResultPtr result);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  std::unique_ptr<CardboardSdk> cardboard_sdk_;
  std::unique_ptr<MailboxToSurfaceBridgeFactory>
      mailbox_to_surface_bridge_factory_;
  std::unique_ptr<XrJavaCoordinator> xr_java_coordinator_;
  std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider_;

  // Outstanding Session Request Data
  mojom::XRRuntime::RequestSessionCallback pending_session_request_callback_;
  mojom::XRRuntimeSessionOptionsPtr options_;

  // Active Session Data
  std::unique_ptr<CardboardRenderLoop> render_loop_;

  base::WeakPtrFactory<CardboardDevice> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_CARDBOARD_CARDBOARD_DEVICE_H_
