// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/bitmap_raster_buffer_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/raster/raster_source.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {
namespace {

class BitmapSoftwareBacking : public ResourcePool::SoftwareBacking {
 public:
  ~BitmapSoftwareBacking() override {
    frame_sink->DidDeleteSharedBitmap(shared_bitmap_id);
  }

  void OnMemoryDump(
      base::trace_event::ProcessMemoryDump* pmd,
      const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
      uint64_t tracing_process_id,
      int importance) const override {
    pmd->CreateSharedMemoryOwnershipEdge(buffer_dump_guid, mapping.guid(),
                                         importance);
  }

  raw_ptr<LayerTreeFrameSink> frame_sink;
  base::WritableSharedMemoryMapping mapping;
};

class BitmapRasterBufferImpl : public RasterBuffer {
 public:
  BitmapRasterBufferImpl(const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         void* pixels,
                         uint64_t resource_content_id,
                         uint64_t previous_content_id)
      : resource_size_(size),
        color_space_(color_space),
        pixels_(pixels),
        resource_has_previous_content_(
            resource_content_id && resource_content_id == previous_content_id) {
  }
  BitmapRasterBufferImpl(const BitmapRasterBufferImpl&) = delete;
  BitmapRasterBufferImpl& operator=(const BitmapRasterBufferImpl&) = delete;

  // Overridden from RasterBuffer:
  void Playback(const RasterSource* raster_source,
                const gfx::Rect& raster_full_rect,
                const gfx::Rect& raster_dirty_rect,
                uint64_t new_content_id,
                const gfx::AxisTransform2d& transform,
                const RasterSource::PlaybackSettings& playback_settings,
                const GURL& url) override {
    TRACE_EVENT0("cc", "BitmapRasterBuffer::Playback");
    gfx::Rect playback_rect = raster_full_rect;
    if (resource_has_previous_content_) {
      playback_rect.Intersect(raster_dirty_rect);
    }
    DCHECK(!playback_rect.IsEmpty())
        << "Why are we rastering a tile that's not dirty?";

    size_t stride = 0u;
    RasterBufferProvider::PlaybackToMemory(
        pixels_, viz::SinglePlaneFormat::kRGBA_8888, resource_size_, stride,
        raster_source, raster_full_rect, playback_rect, transform, color_space_,
        /*gpu_compositing=*/false, playback_settings);
  }

  bool SupportsBackgroundThreadPriority() const override { return true; }

 private:
  const gfx::Size resource_size_;
  const gfx::ColorSpace color_space_;

  // `pixels_` is not a raw_ptr<...> for performance reasons: pointee is never
  // protected by BackupRefPtr, because the pointer comes either from using
  // `mmap`, MapViewOfFile or base::AllocPages directly.
  RAW_PTR_EXCLUSION void* const pixels_;
  bool resource_has_previous_content_;
};

}  // namespace

BitmapRasterBufferProvider::BitmapRasterBufferProvider(
    LayerTreeFrameSink* frame_sink)
    : frame_sink_(frame_sink) {}

BitmapRasterBufferProvider::~BitmapRasterBufferProvider() = default;

std::unique_ptr<RasterBuffer>
BitmapRasterBufferProvider::AcquireBufferForRaster(
    const ResourcePool::InUsePoolResource& resource,
    uint64_t resource_content_id,
    uint64_t previous_content_id,
    bool depends_on_at_raster_decodes,
    bool depends_on_hardware_accelerated_jpeg_candidates,
    bool depends_on_hardware_accelerated_webp_candidates) {
  DCHECK_EQ(resource.format(), viz::SinglePlaneFormat::kRGBA_8888);

  const gfx::Size& size = resource.size();
  const gfx::ColorSpace& color_space = resource.color_space();
  if (!resource.software_backing()) {
    auto backing = std::make_unique<BitmapSoftwareBacking>();
    backing->frame_sink = frame_sink_;
    backing->shared_bitmap_id = viz::SharedBitmap::GenerateId();
    base::MappedReadOnlyRegion shm =
        viz::bitmap_allocation::AllocateSharedBitmap(
            size, viz::SinglePlaneFormat::kRGBA_8888);
    backing->mapping = std::move(shm.mapping);
    frame_sink_->DidAllocateSharedBitmap(std::move(shm.region),
                                         backing->shared_bitmap_id);

    resource.set_software_backing(std::move(backing));
  }
  BitmapSoftwareBacking* backing =
      static_cast<BitmapSoftwareBacking*>(resource.software_backing());

  return std::make_unique<BitmapRasterBufferImpl>(
      size, color_space, backing->mapping.memory(), resource_content_id,
      previous_content_id);
}

void BitmapRasterBufferProvider::Flush() {}

viz::SharedImageFormat BitmapRasterBufferProvider::GetFormat() const {
  return viz::SinglePlaneFormat::kRGBA_8888;
}

bool BitmapRasterBufferProvider::IsResourcePremultiplied() const {
  return true;
}

bool BitmapRasterBufferProvider::CanPartialRasterIntoProvidedResource() const {
  return true;
}

bool BitmapRasterBufferProvider::IsResourceReadyToDraw(
    const ResourcePool::InUsePoolResource& resource) const {
  // Bitmap resources are immediately ready to draw.
  return true;
}

uint64_t BitmapRasterBufferProvider::SetReadyToDrawCallback(
    const std::vector<const ResourcePool::InUsePoolResource*>& resources,
    base::OnceClosure callback,
    uint64_t pending_callback_id) const {
  // Bitmap resources are immediately ready to draw.
  return 0;
}

void BitmapRasterBufferProvider::Shutdown() {}

}  // namespace cc
