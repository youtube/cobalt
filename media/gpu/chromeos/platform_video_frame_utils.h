// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

// Create GpuMemoryBuffer-based media::VideoFrame with |buffer_usage|.
// See //media/base/video_frame.h for other parameters.
// If |gpu_memory_buffer_factory| is not null, it's used to allocate the
// GpuMemoryBuffer and it must outlive the returned VideoFrame. If it's null,
// the buffer is allocated using the render node (this is intended to be used
// only for the internals of video encoding when the usage is
// VEA_READ_CAMERA_AND_CPU_READ_WRITE). It's safe to call this function
// concurrently from multiple threads (as long as either
// |gpu_memory_buffer_factory| is thread-safe or nullptr).
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Create platform dependent media::VideoFrame with |buffer_usage|.
// See //media/base/video_frame.h for other parameters.
// If |gpu_memory_buffer_factory| is not null, it's used to allocate the
// video frame's storage and it must outlive the returned VideoFrame. If it's
// null, the buffer is allocated using the render node (this is intended to be
// used only for the internals of video encoding when the usage is
// VEA_READ_CAMERA_AND_CPU_READ_WRITE). It's safe to call this function
// concurrently from multiple threads (as long as either
// |gpu_memory_buffer_factory| is thread-safe or nullptr).
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Get VideoFrameLayout of platform dependent video frame with |pixel_format|,
// |coded_size| and |buffer_usage|. This function is not cost-free as this
// allocates a platform dependent video frame.
// If |gpu_memory_buffer_factory| is not null, it's used to allocate the
// video frame's storage. If it's null, the storage is allocated using the
// render node (this is intended to be used only for the internals of video
// encoding when the usage is VEA_READ_CAMERA_AND_CPU_READ_WRITE). It's
// safe to call this function concurrently from multiple threads (as long as
// either |gpu_memory_buffer_factory| is thread-safe or nullptr).
MEDIA_GPU_EXPORT absl::optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage);

// Create a shared GPU memory handle to the |video_frame|'s data.
MEDIA_GPU_EXPORT gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame);

// Create a NativePixmap that references the DMA Bufs of |video_frame|. The
// returned pixmap is only a DMA Buf container and should not be used for
// compositing/scanout.
MEDIA_GPU_EXPORT scoped_refptr<gfx::NativePixmapDmaBuf>
CreateNativePixmapDmaBuf(const VideoFrame* video_frame);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
