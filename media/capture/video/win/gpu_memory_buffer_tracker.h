// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_H_
#define MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_H_

#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/capture/video/video_capture_buffer_tracker.h"

#include <d3d11.h>
#include <wrl.h>

namespace gfx {
class Size;
}  // namespace gfx

namespace media {

// Tracker specifics for Windows GpuMemoryBuffer.
class CAPTURE_EXPORT GpuMemoryBufferTracker final
    : public VideoCaptureBufferTracker {
 public:
  explicit GpuMemoryBufferTracker(
      scoped_refptr<DXGIDeviceManager> dxgi_device_manager);

  GpuMemoryBufferTracker(const GpuMemoryBufferTracker&) = delete;
  GpuMemoryBufferTracker& operator=(const GpuMemoryBufferTracker&) = delete;

  ~GpuMemoryBufferTracker() override;

  // Implementation of VideoCaptureBufferTracker:
  bool Init(const gfx::Size& dimensions,
            VideoPixelFormat format,
            const mojom::PlaneStridesPtr& strides) override;
  bool IsReusableForFormat(const gfx::Size& dimensions,
                           VideoPixelFormat format,
                           const mojom::PlaneStridesPtr& strides) override;
  uint32_t GetMemorySizeInBytes() override;
  std::unique_ptr<VideoCaptureBufferHandle> GetMemoryMappedAccess() override;
  base::UnsafeSharedMemoryRegion DuplicateAsUnsafeRegion() override;
  mojo::ScopedSharedBufferHandle DuplicateAsMojoBuffer() override;
  gfx::GpuMemoryBufferHandle GetGpuMemoryBufferHandle() override;

 private:
  std::unique_ptr<gpu::GpuMemoryBufferImplDXGI> buffer_;
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
  base::UnsafeSharedMemoryRegion region_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  gfx::Size buffer_size_;
  bool CreateBufferInternal();
  bool EnsureD3DDevice();
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_GPU_MEMORY_BUFFER_TRACKER_H_