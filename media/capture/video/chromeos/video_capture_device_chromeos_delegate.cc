// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_device_chromeos_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_app_device_bridge_impl.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

class VideoCaptureDeviceChromeOSDelegate::PowerManagerClientProxy
    : public base::RefCountedThreadSafe<PowerManagerClientProxy>,
      public chromeos::PowerManagerClient::Observer {
 public:
  PowerManagerClientProxy() = default;

  void Init(base::WeakPtr<VideoCaptureDeviceChromeOSDelegate> device,
            scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
            scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner) {
    device_ = std::move(device);
    device_task_runner_ = std::move(device_task_runner);
    dbus_task_runner_ = std::move(dbus_task_runner);

    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::InitOnDBusThread, this));
  }

  void Shutdown() {
    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::ShutdownOnDBusThread, this));
  }

  void UnblockSuspend(const base::UnguessableToken& unblock_suspend_token) {
    dbus_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PowerManagerClientProxy::UnblockSuspendOnDBusThread,
                       this, unblock_suspend_token));
  }

 private:
  friend class base::RefCountedThreadSafe<PowerManagerClientProxy>;

  ~PowerManagerClientProxy() override = default;

  void InitOnDBusThread() {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->AddObserver(this);
  }

  void ShutdownOnDBusThread() {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  }

  void UnblockSuspendOnDBusThread(
      const base::UnguessableToken& unblock_suspend_token) {
    DCHECK(dbus_task_runner_->RunsTasksInCurrentSequence());
    chromeos::PowerManagerClient::Get()->UnblockSuspend(unblock_suspend_token);
  }

  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) final {
    auto token = base::UnguessableToken::Create();
    chromeos::PowerManagerClient::Get()->BlockSuspend(
        token, "VideoCaptureDeviceChromeOSDelegate");
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceChromeOSDelegate::CloseDevice,
                       device_, token));
  }

  void SuspendDone(base::TimeDelta sleep_duration) final {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoCaptureDeviceChromeOSDelegate::OpenDevice,
                       device_));
  }

  base::WeakPtr<VideoCaptureDeviceChromeOSDelegate> device_;
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> dbus_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerManagerClientProxy);
};

VideoCaptureDeviceChromeOSDelegate::VideoCaptureDeviceChromeOSDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const VideoCaptureDeviceDescriptor& device_descriptor,
    scoped_refptr<CameraHalDelegate> camera_hal_delegate,
    base::OnceClosure cleanup_callback)
    : device_descriptor_(device_descriptor),
      camera_hal_delegate_(std::move(camera_hal_delegate)),
      capture_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      camera_device_ipc_thread_(std::string("CameraDeviceIpcThread") +
                                device_descriptor.device_id),
      screen_observer_delegate_(
          ScreenObserverDelegate::Create(this, ui_task_runner)),
      lens_facing_(device_descriptor.facing),
      // External cameras have lens_facing as MEDIA_VIDEO_FACING_NONE.
      // We don't want to rotate the frame even if the device rotates.
      rotates_with_device_(lens_facing_ !=
                           VideoFacingMode::MEDIA_VIDEO_FACING_NONE),
      rotation_(0),
      cleanup_callback_(std::move(cleanup_callback)),
      device_closed_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      power_manager_client_proxy_(
          base::MakeRefCounted<PowerManagerClientProxy>()) {
  power_manager_client_proxy_->Init(weak_ptr_factory_.GetWeakPtr(),
                                    capture_task_runner_,
                                    std::move(ui_task_runner));
}

VideoCaptureDeviceChromeOSDelegate::~VideoCaptureDeviceChromeOSDelegate() {}

void VideoCaptureDeviceChromeOSDelegate::Shutdown() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!HasDeviceClient()) {
    DCHECK(!camera_device_ipc_thread_.IsRunning());
    screen_observer_delegate_->RemoveObserver();
    power_manager_client_proxy_->Shutdown();
    camera_hal_delegate_->DisableAllVirtualDevices();
    capture_task_runner_->PostTask(FROM_HERE, std::move(cleanup_callback_));
  }
}

bool VideoCaptureDeviceChromeOSDelegate::HasDeviceClient() {
  return device_context_ && device_context_->HasClient();
}

void VideoCaptureDeviceChromeOSDelegate::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client,
    ClientType client_type) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!HasDeviceClient()) {
    TRACE_EVENT0("camera", "Start Device");
    if (!camera_device_ipc_thread_.Start()) {
      std::string error_msg = "Failed to start device thread";
      LOG(ERROR) << error_msg;
      client->OnError(
          media::VideoCaptureError::kCrosHalV3FailedToStartDeviceThread,
          FROM_HERE, error_msg);
      return;
    }

    device_context_ = std::make_unique<CameraDeviceContext>();
    if (device_context_->AddClient(client_type, std::move(client))) {
      capture_params_[client_type] = params;
      camera_device_delegate_ = std::make_unique<CameraDeviceDelegate>(
          device_descriptor_, camera_hal_delegate_,
          camera_device_ipc_thread_.task_runner());
      OpenDevice();
    }
    CameraAppDeviceBridgeImpl::GetInstance()->OnVideoCaptureDeviceCreated(
        device_descriptor_.device_id, camera_device_ipc_thread_.task_runner());
  } else {
    if (device_context_->AddClient(client_type, std::move(client))) {
      capture_params_[client_type] = params;
      ReconfigureStreams();
    }
  }
}

void VideoCaptureDeviceChromeOSDelegate::StopAndDeAllocate(
    ClientType client_type) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);
  device_context_->RemoveClient(client_type);
  if (!HasDeviceClient()) {
    CloseDevice(base::UnguessableToken());
    CameraAppDeviceBridgeImpl::GetInstance()->OnVideoCaptureDeviceClosing(
        device_descriptor_.device_id);
    camera_device_ipc_thread_.Stop();
    camera_device_delegate_.reset();
    device_context_.reset();
  }
}

void VideoCaptureDeviceChromeOSDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::TakePhoto,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::GetPhotoState,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::SetPhotoOptions,
                                camera_device_delegate_->GetWeakPtr(),
                                std::move(settings), std::move(callback)));
}

void VideoCaptureDeviceChromeOSDelegate::OpenDevice() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    return;
  }
  // It's safe to pass unretained |device_context_| here since
  // VideoCaptureDeviceChromeOSDelegate owns |camera_device_delegate_| and makes
  // sure |device_context_| outlives |camera_device_delegate_|.
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::AllocateAndStart,
                     camera_device_delegate_->GetWeakPtr(), capture_params_,
                     base::Unretained(device_context_.get())));
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::SetRotation,
                     camera_device_delegate_->GetWeakPtr(), rotation_));
}

void VideoCaptureDeviceChromeOSDelegate::ReconfigureStreams() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(camera_device_delegate_);

  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::ReconfigureStreams,
                     camera_device_delegate_->GetWeakPtr(), capture_params_));
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraDeviceDelegate::SetRotation,
                     camera_device_delegate_->GetWeakPtr(), rotation_));
}

void VideoCaptureDeviceChromeOSDelegate::CloseDevice(
    base::UnguessableToken unblock_suspend_token) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!camera_device_delegate_) {
    if (!unblock_suspend_token.is_empty())
      power_manager_client_proxy_->UnblockSuspend(unblock_suspend_token);
    return;
  }
  // We do our best to allow the camera HAL cleanly shut down the device.  In
  // general we don't trust the camera HAL so if the device does not close in
  // time we simply terminate the Mojo channel by resetting
  // |camera_device_delegate_|.
  //
  // VideoCaptureDeviceChromeOSDelegate owns both |camera_device_delegate_| and
  // |device_closed_| and it stops |camera_device_ipc_thread_| in
  // StopAndDeAllocate, so it's safe to pass |device_closed_| as unretained in
  // the callback.
  device_closed_.Reset();
  camera_device_ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CameraDeviceDelegate::StopAndDeAllocate,
                                camera_device_delegate_->GetWeakPtr(),
                                base::BindOnce(
                                    [](base::WaitableEvent* device_closed) {
                                      device_closed->Signal();
                                    },
                                    base::Unretained(&device_closed_))));
  base::TimeDelta kWaitTimeoutSecs = base::Seconds(3);
  device_closed_.TimedWait(kWaitTimeoutSecs);
  if (!unblock_suspend_token.is_empty())
    power_manager_client_proxy_->UnblockSuspend(unblock_suspend_token);
}

void VideoCaptureDeviceChromeOSDelegate::SetDisplayRotation(
    const display::Display& display) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (display.IsInternal())
    SetRotation(display.rotation() * 90);
}

void VideoCaptureDeviceChromeOSDelegate::SetRotation(int rotation) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  if (!rotates_with_device_) {
    rotation = 0;
  } else if (lens_facing_ == VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT) {
    // Original frame when |rotation| = 0
    // -----------------------
    // |          *          |
    // |         * *         |
    // |        *   *        |
    // |       *******       |
    // |      *       *      |
    // |     *         *     |
    // -----------------------
    //
    // |rotation| = 90, this is what back camera sees
    // -----------------------
    // |    ********         |
    // |       *   ****      |
    // |       *      ***    |
    // |       *      ***    |
    // |       *   ****      |
    // |    ********         |
    // -----------------------
    //
    // |rotation| = 90, this is what front camera sees
    // -----------------------
    // |         ********    |
    // |      ****   *       |
    // |    ***      *       |
    // |    ***      *       |
    // |      ****   *       |
    // |         ********    |
    // -----------------------
    //
    // Therefore, for back camera, we need to rotate (360 - |rotation|).
    rotation = (360 - rotation) % 360;
  }
  rotation_ = rotation;
  if (camera_device_ipc_thread_.IsRunning()) {
    camera_device_ipc_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraDeviceDelegate::SetRotation,
                       camera_device_delegate_->GetWeakPtr(), rotation_));
  }
}

}  // namespace media
