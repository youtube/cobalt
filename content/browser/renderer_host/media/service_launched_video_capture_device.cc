// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_launched_video_capture_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/token.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

namespace content {

ServiceLaunchedVideoCaptureDevice::ServiceLaunchedVideoCaptureDevice(
    mojo::Remote<video_capture::mojom::VideoSource> source,
    mojo::Remote<video_capture::mojom::PushVideoStreamSubscription>
        subscription,
    base::OnceClosure connection_lost_cb)
    : source_(std::move(source)),
      subscription_(std::move(subscription)),
      connection_lost_cb_(std::move(connection_lost_cb)) {
  // Unretained |this| is safe, because |this| owns |source_|.
  source_.set_disconnect_handler(
      base::BindOnce(&ServiceLaunchedVideoCaptureDevice::
                         OnLostConnectionToSourceOrSubscription,
                     base::Unretained(this)));
  // Unretained |this| is safe, because |this| owns |subscription_|.
  subscription_.set_disconnect_handler(
      base::BindOnce(&ServiceLaunchedVideoCaptureDevice::
                         OnLostConnectionToSourceOrSubscription,
                     base::Unretained(this)));
}

ServiceLaunchedVideoCaptureDevice::~ServiceLaunchedVideoCaptureDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ServiceLaunchedVideoCaptureDevice::GetPhotoState(
    media::VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_->GetPhotoState(base::BindOnce(
      &ServiceLaunchedVideoCaptureDevice::OnGetPhotoStateResponse,
      base::Unretained(this), std::move(callback)));
}

void ServiceLaunchedVideoCaptureDevice::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_->SetPhotoOptions(
      std::move(settings),
      base::BindOnce(
          &ServiceLaunchedVideoCaptureDevice::OnSetPhotoOptionsResponse,
          base::Unretained(this), std::move(callback)));
}

void ServiceLaunchedVideoCaptureDevice::TakePhoto(
    media::VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ServiceLaunchedVideoCaptureDevice::TakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);
  subscription_->TakePhoto(
      base::BindOnce(&ServiceLaunchedVideoCaptureDevice::OnTakePhotoResponse,
                     base::Unretained(this), std::move(callback)));
}

void ServiceLaunchedVideoCaptureDevice::MaybeSuspendDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_->Suspend(base::DoNothing());
}

void ServiceLaunchedVideoCaptureDevice::ResumeDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscription_->Resume();
}

void ServiceLaunchedVideoCaptureDevice::Crop(
    const base::Token& crop_id,
    uint32_t crop_version,
    base::OnceCallback<void(media::mojom::CropRequestResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1264849): Implement if necessary.
  std::move(callback).Run(media::mojom::CropRequestResult::kNotImplemented);
}

void ServiceLaunchedVideoCaptureDevice::RequestRefreshFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do here. The video capture service does not support refresh
  // frames.
}

void ServiceLaunchedVideoCaptureDevice::SetDesktopCaptureWindowIdAsync(
    gfx::NativeViewId window_id,
    base::OnceClosure done_cb) {
  // This method should only be called for desktop capture devices.
  // The video_capture Mojo service does not support desktop capture devices
  // (yet) and should not be used for it.
  NOTREACHED();
}

void ServiceLaunchedVideoCaptureDevice::OnUtilizationReport(
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (feedback != last_feedback_) {
    subscription_->ProcessFeedback(feedback);
    last_feedback_ = feedback;
  }
}

void ServiceLaunchedVideoCaptureDevice::
    OnLostConnectionToSourceOrSubscription() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_.reset();
  subscription_.reset();
  std::move(connection_lost_cb_).Run();
}

void ServiceLaunchedVideoCaptureDevice::OnGetPhotoStateResponse(
    media::VideoCaptureDevice::GetPhotoStateCallback callback,
    media::mojom::PhotoStatePtr capabilities) const {
  if (!capabilities)
    return;
  std::move(callback).Run(std::move(capabilities));
}

void ServiceLaunchedVideoCaptureDevice::OnSetPhotoOptionsResponse(
    media::VideoCaptureDevice::SetPhotoOptionsCallback callback,
    bool success) {
  if (!success)
    return;
  std::move(callback).Run(true);
}

void ServiceLaunchedVideoCaptureDevice::OnTakePhotoResponse(
    media::VideoCaptureDevice::TakePhotoCallback callback,
    media::mojom::BlobPtr blob) {
  if (!blob)
    return;
  std::move(callback).Run(std::move(blob));
}

}  // namespace content
