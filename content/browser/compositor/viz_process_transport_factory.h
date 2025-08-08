// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id_allocator.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "ui/compositor/compositor.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class SingleThreadTaskGraphRunner;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace viz {
class CompositingModeReporterImpl;
class HostDisplayClient;
class RasterContextProvider;
}

namespace viz {
class ContextProviderCommandBuffer;
}

namespace content {

// Interface implementations to interact with the display compositor in the viz
// process.
class VizProcessTransportFactory : public ui::ContextFactory,
                                   public ImageTransportFactory {
 public:
  VizProcessTransportFactory(
      gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
      viz::CompositingModeReporterImpl* compositing_mode_reporter);

  VizProcessTransportFactory(const VizProcessTransportFactory&) = delete;
  VizProcessTransportFactory& operator=(const VizProcessTransportFactory&) =
      delete;

  ~VizProcessTransportFactory() override;

  // Connects HostFrameSinkManager to FrameSinkManagerImpl in viz process.
  void ConnectHostFrameSinkManager();

  // ui::ContextFactory implementation.
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override;

  void RemoveCompositor(ui::Compositor* compositor) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  viz::FrameSinkId AllocateFrameSinkId() override;
  viz::SubtreeCaptureId AllocateSubtreeCaptureId() override;
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override;

  // ImageTransportFactory implementation.
  void DisableGpuCompositing() override;
  ui::ContextFactory* GetContextFactory() override;

 private:
  struct CompositorData {
    CompositorData();
    CompositorData(CompositorData&& other);
    CompositorData& operator=(CompositorData&& other);
    ~CompositorData();

    std::unique_ptr<viz::HostDisplayClient> display_client;
  };

  // Disables GPU compositing. This notifies UI and renderer compositors to drop
  // LayerTreeFrameSinks and request new ones. If fallback happens while
  // creating a new LayerTreeFrameSink for UI compositor it should be passed in
  // as |guilty_compositor| to avoid extra work and reentrancy problems.
  void DisableGpuCompositing(ui::Compositor* guilty_compositor);

  // Provided as a callback when the GPU process has crashed.
  void OnGpuProcessLost();

  // Finishes creation of LayerTreeFrameSink after GPU channel has been
  // established.
  void OnEstablishedGpuChannel(
      base::WeakPtr<ui::Compositor> compositor_weak_ptr,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // Tries to create the raster and main thread ContextProviders. If the
  // ContextProviders already exist and haven't been lost then this will do
  // nothing. Also verifies |gpu_channel_host| and checks if GPU compositing is
  // blacklisted.
  //
  // Returns kSuccess if caller can use GPU compositing, kTransientFailure if
  // caller should try again or kFatalFailure/kSurfaceFailure if caller should
  // fallback to software compositing.
  gpu::ContextResult TryCreateContextsForGpuCompositing(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  const raw_ptr<gpu::GpuChannelEstablishFactory> gpu_channel_establish_factory_;

  // Controls the compositing mode based on what mode the display compositors
  // are using.
  [[maybe_unused]] const raw_ptr<viz::CompositingModeReporterImpl>
      compositing_mode_reporter_;

  // ContextProvider used on worker threads for rasterization.
  scoped_refptr<cc::RasterContextProviderWrapper>
      worker_context_provider_wrapper_;

  // ContextProvider used on the main thread. Shared by ui::Compositors and also
  // returned from GetSharedMainThreadContextProvider().
  scoped_refptr<viz::ContextProviderCommandBuffer> main_context_provider_;

  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;

  base::flat_map<ui::Compositor*, CompositorData> compositor_data_map_;

  viz::FrameSinkIdAllocator frame_sink_id_allocator_;
  viz::SubtreeCaptureIdAllocator subtree_capture_id_allocator_;
  const raw_ptr<viz::HostFrameSinkManager> host_frame_sink_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> const resize_task_runner_;

  bool is_gpu_compositing_disabled_ = false;

  base::WeakPtrFactory<VizProcessTransportFactory> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
