// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/components/camera_app_ui/document_scanner_service_client.h"
#include "media/base/video_transformation.h"
#include "media/capture/capture_export.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/mojom/camera3.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_common.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

namespace gpu {

class GpuMemoryBufferImpl;

}  // namespace gpu

namespace media {

class CameraDeviceContext;

struct ReprocessTask {
 public:
  ReprocessTask();
  ReprocessTask(ReprocessTask&& other);
  ~ReprocessTask();
  cros::mojom::Effect effect;
  base::OnceCallback<void(int32_t, media::mojom::BlobPtr)> callback;
  std::vector<cros::mojom::CameraMetadataEntryPtr> extra_metadata;
};

using ReprocessTaskQueue = base::queue<ReprocessTask>;

// TODO(shik): Get the keys from VendorTagOps by names instead (b/130774415).
constexpr uint32_t kPortraitModeVendorKey = 0x80000000;
constexpr uint32_t kPortraitModeSegmentationResultVendorKey = 0x80000001;
constexpr int32_t kReprocessSuccess = 0;

// Implementation of CameraAppDevice that is used as the ommunication bridge
// between Chrome Camera App (CCA) and the ChromeOS Video Capture Device. By
// using this, we can do more complicated operations on cameras which is not
// supported by Chrome API.
class CAPTURE_EXPORT CameraAppDeviceImpl : public cros::mojom::CameraAppDevice {
 public:
  // Retrieve the return code for reprocess |effect| from the |metadata|.
  static int GetReprocessReturnCode(
      cros::mojom::Effect effect,
      const cros::mojom::CameraMetadataPtr* metadata);

  // Construct a ReprocessTaskQueue for regular capture with
  // |take_photo_callback|.
  static ReprocessTaskQueue GetSingleShotReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback);

  CameraAppDeviceImpl(const std::string& device_id,
                      cros::mojom::CameraInfoPtr camera_info);

  CameraAppDeviceImpl(const CameraAppDeviceImpl&) = delete;
  CameraAppDeviceImpl& operator=(const CameraAppDeviceImpl&) = delete;

  ~CameraAppDeviceImpl() override;

  // Binds the mojo receiver to this implementation.
  void BindReceiver(
      mojo::PendingReceiver<cros::mojom::CameraAppDevice> receiver);

  // All the weak pointers should be retrieved, dereferenced and invalidated on
  // the camera device ipc thread.
  base::WeakPtr<CameraAppDeviceImpl> GetWeakPtr();

  // Resets things which need to be handled on device IPC thread, including
  // invalidating all the existing weak pointers, and then triggers |callback|.
  // When |should_disable_new_ptrs| is set to true, no more weak pointers can be
  // created. It is used when tearing down the CameraAppDeviceImpl instance.
  void ResetOnDeviceIpcThread(base::OnceClosure callback,
                              bool should_disable_new_ptrs);

  // Consumes all the pending reprocess tasks if there is any and eventually
  // generates a ReprocessTaskQueue which contains:
  //   1. A regular capture task with |take_photo_callback|.
  //   2. One or more reprocess tasks if there is any.
  // And passes the generated ReprocessTaskQueue through |consumption_callback|.
  void ConsumeReprocessOptions(
      media::mojom::ImageCapture::TakePhotoCallback take_photo_callback,
      base::OnceCallback<void(ReprocessTaskQueue)> consumption_callback);

  // Retrieves the fps range if it is specified by the app.
  absl::optional<gfx::Range> GetFpsRange();

  // Retrieves the corresponding capture resolution which is specified by the
  // app.
  gfx::Size GetStillCaptureResolution();

  // Gets the capture intent which is specified by the app.
  cros::mojom::CaptureIntent GetCaptureIntent();

  // Delivers the result |metadata| with its |stream_type| to the metadata
  // observers.
  void OnResultMetadataAvailable(const cros::mojom::CameraMetadataPtr& metadata,
                                 const cros::mojom::StreamType stream_type);

  // Notifies the camera event observers that the shutter is finished.
  void OnShutterDone();

  // Sets the pointer to the camera device context instance associated with the
  // opened camera.  Used to configure and query camera frame rotation.
  void SetCameraDeviceContext(CameraDeviceContext* device_context);

  // Detect document corners on the frame given by its gpu memory buffer if it
  // is supported.
  void MaybeDetectDocumentCorners(std::unique_ptr<gpu::GpuMemoryBufferImpl> gmb,
                                  VideoRotation rotation);

  // cros::mojom::CameraAppDevice implementations.
  void GetCameraInfo(GetCameraInfoCallback callback) override;
  void SetReprocessOptions(
      const std::vector<cros::mojom::Effect>& effects,
      mojo::PendingRemote<cros::mojom::ReprocessResultListener> listener,
      SetReprocessOptionsCallback callback) override;
  void SetFpsRange(const gfx::Range& fps_range,
                   SetFpsRangeCallback callback) override;
  void SetStillCaptureResolution(
      const gfx::Size& resolution,
      SetStillCaptureResolutionCallback callback) override;
  void SetCaptureIntent(cros::mojom::CaptureIntent capture_intent,
                        SetCaptureIntentCallback callback) override;
  void AddResultMetadataObserver(
      mojo::PendingRemote<cros::mojom::ResultMetadataObserver> observer,
      cros::mojom::StreamType streamType,
      AddResultMetadataObserverCallback callback) override;
  void AddCameraEventObserver(
      mojo::PendingRemote<cros::mojom::CameraEventObserver> observer,
      AddCameraEventObserverCallback callback) override;
  void SetCameraFrameRotationEnabledAtSource(
      bool is_enabled,
      SetCameraFrameRotationEnabledAtSourceCallback callback) override;
  void GetCameraFrameRotation(GetCameraFrameRotationCallback callback) override;
  void RegisterDocumentCornersObserver(
      mojo::PendingRemote<cros::mojom::DocumentCornersObserver> observer,
      RegisterDocumentCornersObserverCallback callback) override;

 private:
  static void DisableEeNr(ReprocessTask* task);

  void OnMojoConnectionError();

  bool IsCloseToPreviousDetectionRequest();

  void DetectDocumentCornersOnMojoThread(
      std::unique_ptr<gpu::GpuMemoryBufferImpl> image,
      VideoRotation rotation);

  void OnDetectedDocumentCornersOnMojoThread(
      VideoRotation rotation,
      bool success,
      const std::vector<gfx::PointF>& corners);

  void SetReprocessResultOnMojoThread(cros::mojom::Effect effect,
                                      const int32_t status,
                                      media::mojom::BlobPtr blob);

  void NotifyShutterDoneOnMojoThread();
  void NotifyResultMetadataOnMojoThread(cros::mojom::CameraMetadataPtr metadata,
                                        cros::mojom::StreamType streamType);

  std::string device_id_;

  // If it is set to false, no weak pointers for this instance can be generated
  // for IPC thread.
  bool allow_new_ipc_weak_ptrs_;

  mojo::ReceiverSet<cros::mojom::CameraAppDevice> receivers_;

  cros::mojom::CameraInfoPtr camera_info_;

  // It is used for calls which should run on the mojo thread.
  scoped_refptr<base::SingleThreadTaskRunner> mojo_task_runner_;

  // The queue will be enqueued and dequeued from different threads.
  base::Lock reprocess_tasks_lock_;
  base::queue<ReprocessTask> reprocess_task_queue_
      GUARDED_BY(reprocess_tasks_lock_);
  mojo::Remote<cros::mojom::ReprocessResultListener> reprocess_listener_
      GUARDED_BY(reprocess_tasks_lock_);

  // It will be inserted and read from different threads.
  base::Lock fps_ranges_lock_;
  absl::optional<gfx::Range> specified_fps_range_ GUARDED_BY(fps_ranges_lock_);

  // It will be inserted and read from different threads.
  base::Lock still_capture_resolution_lock_;
  gfx::Size still_capture_resolution_
      GUARDED_BY(still_capture_resolution_lock_);

  // It will be modified and read from different threads.
  base::Lock capture_intent_lock_;
  cros::mojom::CaptureIntent capture_intent_ GUARDED_BY(capture_intent_lock_);

  // Those maps will be changed and used only on the mojo thread.
  std::map<cros::mojom::StreamType,
           mojo::RemoteSet<cros::mojom::ResultMetadataObserver>>
      stream_to_metadata_observers_map_;

  mojo::RemoteSet<cros::mojom::CameraEventObserver> camera_event_observers_;

  base::Lock camera_device_context_lock_;
  CameraDeviceContext* camera_device_context_
      GUARDED_BY(camera_device_context_lock_);

  mojo::RemoteSet<cros::mojom::DocumentCornersObserver>
      document_corners_observers_;
  bool has_ongoing_document_detection_task_ = false;
  std::unique_ptr<base::ElapsedTimer> document_detection_timer_ = nullptr;

  // Client to connect to document detection service. It should only be
  // used/destructed on the Mojo thread.
  std::unique_ptr<chromeos::DocumentScannerServiceClient>
      document_scanner_service_;

  // The weak pointers should be dereferenced and invalidated on camera device
  // ipc thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_{this};

  // The weak pointers should be dereferenced and invalidated on the Mojo
  // thread.
  base::WeakPtrFactory<CameraAppDeviceImpl> weak_ptr_factory_for_mojo_{this};
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CAMERA_APP_DEVICE_IMPL_H_
