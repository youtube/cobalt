// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/staging_buffer_pool.h"
#include "components/viz/client/client_resource_provider.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
class RasterContextProvider;
}  // namespace viz

namespace cc {
struct StagingBuffer;
class StagingBufferPool;

class CC_EXPORT OneCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  OneCopyRasterBufferProvider(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      viz::ContextProvider* compositor_context_provider,
      viz::RasterContextProvider* worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      int max_copy_texture_chromium_size,
      bool use_partial_raster,
      bool use_gpu_memory_buffer_resources,
      int max_staging_buffer_usage_in_bytes,
      viz::SharedImageFormat tile_format);
  OneCopyRasterBufferProvider(const OneCopyRasterBufferProvider&) = delete;
  ~OneCopyRasterBufferProvider() override;

  OneCopyRasterBufferProvider& operator=(const OneCopyRasterBufferProvider&) =
      delete;

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const ResourcePool::InUsePoolResource& resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id,
      bool depends_on_at_raster_decodes,
      bool depends_on_hardware_accelerated_jpeg_candidates,
      bool depends_on_hardware_accelerated_webp_candidates) override;
  void Flush() override;
  viz::SharedImageFormat GetFormat() const override;
  bool IsResourcePremultiplied() const override;
  bool CanPartialRasterIntoProvidedResource() const override;
  bool IsResourceReadyToDraw(
      const ResourcePool::InUsePoolResource& resource) const override;
  uint64_t SetReadyToDrawCallback(
      const std::vector<const ResourcePool::InUsePoolResource*>& resources,
      base::OnceClosure callback,
      uint64_t pending_callback_id) const override;
  void SetShutdownEvent(base::WaitableEvent* shutdown_event) override;
  void Shutdown() override;

  // Playback raster source and copy result into |resource|.
  gpu::SyncToken PlaybackAndCopyOnWorkerThread(
      gpu::Mailbox* mailbox,
      GLenum mailbox_texture_target,
      bool mailbox_texture_is_overlay_candidate,
      const gpu::SyncToken& sync_token,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      const gfx::AxisTransform2d& transform,
      const gfx::Size& resource_size,
      viz::SharedImageFormat format,
      const gfx::ColorSpace& color_space,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);

 private:
  class OneCopyGpuBacking;

  class RasterBufferImpl : public RasterBuffer {
   public:
    RasterBufferImpl(OneCopyRasterBufferProvider* client,
                     gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                     const ResourcePool::InUsePoolResource& in_use_resource,
                     OneCopyGpuBacking* backing,
                     uint64_t previous_content_id);
    RasterBufferImpl(const RasterBufferImpl&) = delete;
    ~RasterBufferImpl() override;

    RasterBufferImpl& operator=(const RasterBufferImpl&) = delete;

    // Overridden from RasterBuffer:
    void Playback(const RasterSource* raster_source,
                  const gfx::Rect& raster_full_rect,
                  const gfx::Rect& raster_dirty_rect,
                  uint64_t new_content_id,
                  const gfx::AxisTransform2d& transform,
                  const RasterSource::PlaybackSettings& playback_settings,
                  const GURL& url) override;
    bool SupportsBackgroundThreadPriority() const override;

   private:
    // These fields may only be used on the compositor thread.
    const raw_ptr<OneCopyRasterBufferProvider> client_;
    raw_ptr<OneCopyGpuBacking> backing_;

    // These fields are for use on the worker thread.
    const gfx::Size resource_size_;
    const viz::SharedImageFormat format_;
    const gfx::ColorSpace color_space_;
    const uint64_t previous_content_id_;
    const gpu::SyncToken before_raster_sync_token_;
    gpu::Mailbox mailbox_;
    const GLenum mailbox_texture_target_;
    const bool mailbox_texture_is_overlay_candidate_;
    // A SyncToken to be returned from the worker thread, and waited on before
    // using the rastered resource.
    gpu::SyncToken after_raster_sync_token_;
  };

  void PlaybackToStagingBuffer(
      StagingBuffer* staging_buffer,
      const RasterSource* raster_source,
      const gfx::Rect& raster_full_rect,
      const gfx::Rect& raster_dirty_rect,
      const gfx::AxisTransform2d& transform,
      viz::SharedImageFormat format,
      const gfx::ColorSpace& dst_color_space,
      const RasterSource::PlaybackSettings& playback_settings,
      uint64_t previous_content_id,
      uint64_t new_content_id);
  gpu::SyncToken CopyOnWorkerThread(StagingBuffer* staging_buffer,
                                    const RasterSource* raster_source,
                                    const gfx::Rect& rect_to_copy,
                                    viz::SharedImageFormat format,
                                    const gfx::Size& resource_size,
                                    gpu::Mailbox* mailbox,
                                    GLenum mailbox_texture_target,
                                    bool mailbox_texture_is_overlay_candidate,
                                    const gpu::SyncToken& sync_token,
                                    const gfx::ColorSpace& color_space);

  const raw_ptr<viz::ContextProvider> compositor_context_provider_;
  const raw_ptr<viz::RasterContextProvider> worker_context_provider_;
  const raw_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  raw_ptr<base::WaitableEvent> shutdown_event_ = nullptr;
  const int max_bytes_per_copy_operation_;
  const bool use_partial_raster_;
  const bool use_gpu_memory_buffer_resources_;

  // Context lock must be acquired when accessing this member.
  int bytes_scheduled_since_last_flush_;

  const viz::SharedImageFormat tile_format_;
  StagingBufferPool staging_pool_;
};

}  // namespace cc

#endif  // CC_RASTER_ONE_COPY_RASTER_BUFFER_PROVIDER_H_
