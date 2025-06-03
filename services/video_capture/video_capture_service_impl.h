// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "services/video_capture/ash/video_capture_device_factory_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "services/viz/public/cpp/gpu/gpu.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

namespace video_capture {

class VirtualDeviceEnabledDeviceFactory;
class VideoSourceProviderImpl;

class VideoCaptureServiceImpl : public mojom::VideoCaptureService {
 public:
  VideoCaptureServiceImpl(
      mojo::PendingReceiver<mojom::VideoCaptureService> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  VideoCaptureServiceImpl(const VideoCaptureServiceImpl&) = delete;
  VideoCaptureServiceImpl& operator=(const VideoCaptureServiceImpl&) = delete;

  ~VideoCaptureServiceImpl() override;

  // mojom::VideoCaptureService implementation.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InjectGpuDependencies(mojo::PendingRemote<mojom::AcceleratorFactory>
                                 accelerator_factory) override;
  void ConnectToCameraAppDeviceBridge(
      mojo::PendingReceiver<cros::mojom::CameraAppDeviceBridge> receiver)
      override;
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory> receiver)
      override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<mojom::VideoSourceProvider> receiver) override;
  void BindControlsForTesting(
      mojo::PendingReceiver<mojom::TestingControls> receiver) override;
#if BUILDFLAG(IS_WIN)
  void OnGpuInfoUpdate(const CHROME_LUID& luid) override;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  void SetVizGpu(std::unique_ptr<viz::Gpu> viz_gpu);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
 private:
  class GpuDependenciesContext;

  void LazyInitializeGpuDependenciesContext();
  void LazyInitializeDeviceFactory();
  void LazyInitializeVideoSourceProvider();
  void OnLastSourceProviderClientDisconnected();

  mojo::Receiver<mojom::VideoCaptureService> receiver_;
  std::unique_ptr<VirtualDeviceEnabledDeviceFactory> device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> video_source_provider_;
  std::unique_ptr<GpuDependenciesContext> gpu_dependencies_context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Must be destroyed before |device_factory_|.
  std::unique_ptr<crosapi::VideoCaptureDeviceFactoryAsh>
      device_factory_ash_adapter_;
  // Must be destroyed before |device_factory_ash_adapter_|.
  mojo::ReceiverSet<crosapi::mojom::VideoCaptureDeviceFactory>
      factory_receivers_ash_;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  class VizGpuContextProvider;
  std::unique_ptr<VizGpuContextProvider> viz_gpu_context_provider_;
  std::unique_ptr<viz::Gpu> viz_gpu_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_IMPL_H_
