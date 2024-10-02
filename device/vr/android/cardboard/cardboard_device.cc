// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_device.h"

#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "device/vr/android/cardboard/cardboard_image_transport.h"
#include "device/vr/android/cardboard/cardboard_render_loop.h"

namespace device {

namespace {
const std::vector<mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<mojom::XRSessionFeature>>
      kSupportedFeatures{{
          mojom::XRSessionFeature::REF_SPACE_VIEWER,
          mojom::XRSessionFeature::REF_SPACE_LOCAL,
          mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
      }};

  return *kSupportedFeatures;
}
}  // namespace

CardboardDevice::CardboardDevice(
    std::unique_ptr<CardboardSdk> cardboard_sdk,
    std::unique_ptr<MailboxToSurfaceBridgeFactory>
        mailbox_to_surface_bridge_factory,
    std::unique_ptr<XrJavaCoordinator> xr_java_coordinator,
    std::unique_ptr<CompositorDelegateProvider> compositor_delegate_provider)
    : VRDeviceBase(mojom::XRDeviceId::CARDBOARD_DEVICE_ID),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      cardboard_sdk_(std::move(cardboard_sdk)),
      mailbox_to_surface_bridge_factory_(
          std::move(mailbox_to_surface_bridge_factory)),
      xr_java_coordinator_(std::move(xr_java_coordinator)),
      compositor_delegate_provider_(std::move(compositor_delegate_provider)) {
  SetSupportedFeatures(GetSupportedFeatures());
}

CardboardDevice::~CardboardDevice() {
  // Notify any outstanding session requests that they have failed.
  OnCreateSessionResult(nullptr);

  // Ensure that any active sessions are terminated.
  OnSessionEnded();
}

void CardboardDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  // We can only have one exclusive session or serve one pending request session
  // request at a time.
  if (HasExclusiveSession() || pending_session_request_callback_) {
    std::move(callback).Run(nullptr);
    return;
  }

  pending_session_request_callback_ = std::move(callback);

  // Set HasExclusiveSession status to true. This lasts until OnSessionEnded.
  OnStartPresenting();

  render_loop_ = std::make_unique<CardboardRenderLoop>(
      std::make_unique<CardboardImageTransportFactory>(),
      mailbox_to_surface_bridge_factory_->Create());

  // Start the render loop now. Any tasks that we post to it won't run until it
  // finishes starting.
  render_loop_->Start();

  auto ready_callback = base::BindRepeating(
      &CardboardDevice::OnDrawingSurfaceReady, weak_ptr_factory_.GetWeakPtr());
  auto touch_callback = base::BindRepeating(
      &CardboardDevice::OnDrawingSurfaceTouch, weak_ptr_factory_.GetWeakPtr());
  auto destroyed_callback =
      base::BindOnce(&CardboardDevice::OnDrawingSurfaceDestroyed,
                     weak_ptr_factory_.GetWeakPtr());

  // Need to grab the render_process_id and render_frame_id off of the options_
  // object before we save it. It's only used in OnDrawingSurfaceReady, but
  // stashing it as a member allows us to control its lifetime relative to the
  // callback which can prevent some hard-to-debug issues.
  int render_process_id = options->render_process_id;
  int render_frame_id = options->render_frame_id;
  options_ = std::move(options);

  xr_java_coordinator_->RequestVrSession(
      render_process_id, render_frame_id, *compositor_delegate_provider_.get(),
      std::move(ready_callback), std::move(touch_callback),
      std::move(destroyed_callback));
}

void CardboardDevice::OnDrawingSurfaceReady(gfx::AcceleratedWidget window,
                                            gpu::SurfaceHandle surface_handle,
                                            ui::WindowAndroid* root_window,
                                            display::Display::Rotation rotation,
                                            const gfx::Size& frame_size) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__ << ": size=" << frame_size.width() << "x"
           << frame_size.height() << " rotation=" << static_cast<int>(rotation);
  auto session_shutdown_callback = base::BindPostTask(
      main_thread_task_runner_,
      base::BindOnce(&CardboardDevice::OnDrawingSurfaceDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
  auto session_result_callback =
      base::BindPostTask(main_thread_task_runner_,
                         base::BindOnce(&CardboardDevice::OnCreateSessionResult,
                                        weak_ptr_factory_.GetWeakPtr()));

  render_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CardboardRenderLoop::CreateSession,
                     render_loop_->GetWeakPtr(),
                     std::move(session_result_callback),
                     std::move(session_shutdown_callback),
                     xr_java_coordinator_.get(), cardboard_sdk_.get(), window,
                     frame_size, rotation, std::move(options_)));
}

void CardboardDevice::OnDrawingSurfaceTouch(bool is_primary,
                                            bool touching,
                                            int32_t pointer_id,
                                            const gfx::PointF& location) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(3) << __func__ << ": pointer_id=" << pointer_id
           << " is_primary=" << is_primary << " touching=" << touching;

  // TODO(https://crbug.com/1429087): Process Touch Events.
}

void CardboardDevice::OnDrawingSurfaceDestroyed() {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << __func__;

  // This is really the only mechanism that the java code has to signal to us
  // that session creation failed. So in the event that there's still a pending
  // session request, we need to notify our caller that it failed.
  OnCreateSessionResult(nullptr);

  OnSessionEnded();
}

void CardboardDevice::OnCreateSessionResult(
    mojom::XRRuntimeSessionResultPtr result) {
  if (pending_session_request_callback_) {
    std::move(pending_session_request_callback_).Run(std::move(result));
  }
}

void CardboardDevice::ShutdownSession(
    mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DVLOG(1) << __func__;
  OnDrawingSurfaceDestroyed();
  std::move(on_completed).Run();
}

void CardboardDevice::OnSessionEnded() {
  DVLOG(1) << __func__;

  if (!HasExclusiveSession()) {
    return;
  }

  // The render loop destructor stops itself, so we don't need to stop it here.
  render_loop_.reset();

  // This may be a no-op in case session end was initiated from the Java side.
  xr_java_coordinator_->EndSession();

  // This sets HasExclusiveSession status to false.
  OnExitPresent();
}

}  // namespace device
