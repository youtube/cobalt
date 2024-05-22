// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/shared_memory_buffer_tracker.h"

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace {

// A local VideoCaptureBufferHandle implementation used with
// GetHandleForInProcessAccess. This does not own the mapping, so the tracker
// that generates this must outlive it.
class SharedMemoryBufferTrackerHandle : public media::VideoCaptureBufferHandle {
 public:
  explicit SharedMemoryBufferTrackerHandle(
      const base::WritableSharedMemoryMapping& mapping)
      : mapped_size_(mapping.size()),
        data_(mapping.GetMemoryAsSpan<uint8_t>().data()) {}

  size_t mapped_size() const final { return mapped_size_; }
  uint8_t* data() const final { return data_; }
  const uint8_t* const_data() const final { return data_; }

 private:
  const size_t mapped_size_;
  raw_ptr<uint8_t> data_;
};

size_t CalculateRequiredBufferSize(
    const gfx::Size& dimensions,
    media::VideoPixelFormat format,
    const media::mojom::PlaneStridesPtr& strides) {
  if (strides) {
    size_t result = 0u;
    for (size_t plane_index = 0;
         plane_index < media::VideoFrame::NumPlanes(format); plane_index++) {
      result +=
          strides->stride_by_plane[plane_index] *
          media::VideoFrame::Rows(plane_index, format, dimensions.height());
    }
    return result;
  } else {
    const auto& frame_format =
        media::VideoCaptureFormat(dimensions, 0.0f, format);
    return media::VideoFrame::AllocationSize(frame_format.pixel_format,
                                             frame_format.frame_size);
  }
}

}  // namespace

namespace media {

SharedMemoryBufferTracker::SharedMemoryBufferTracker(bool strict_pixel_format)
    : strict_pixel_format_(strict_pixel_format) {}

SharedMemoryBufferTracker::~SharedMemoryBufferTracker() = default;

bool SharedMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                     VideoPixelFormat format,
                                     const mojom::PlaneStridesPtr& strides) {
  DCHECK(!region_.IsValid());
  const size_t buffer_size =
      CalculateRequiredBufferSize(dimensions, format, strides);
  region_ = base::UnsafeSharedMemoryRegion::Create(buffer_size);
  mapping_ = {};
  format_ = format;
  return region_.IsValid();
}

bool SharedMemoryBufferTracker::IsReusableForFormat(
    const gfx::Size& dimensions,
    VideoPixelFormat format,
    const mojom::PlaneStridesPtr& strides) {
  return (!strict_pixel_format_ || format == format_) &&
         GetMemorySizeInBytes() >=
             CalculateRequiredBufferSize(dimensions, format, strides);
}

std::unique_ptr<VideoCaptureBufferHandle>
SharedMemoryBufferTracker::GetMemoryMappedAccess() {
  DCHECK(region_.IsValid());
  if (!mapping_.IsValid()) {
    mapping_ = region_.Map();
  }
  DCHECK(mapping_.IsValid());
  return std::make_unique<SharedMemoryBufferTrackerHandle>(mapping_);
}

base::UnsafeSharedMemoryRegion
SharedMemoryBufferTracker::DuplicateAsUnsafeRegion() {
  DCHECK(region_.IsValid());
  return region_.Duplicate();
}

mojo::ScopedSharedBufferHandle
SharedMemoryBufferTracker::DuplicateAsMojoBuffer() {
  DCHECK(region_.IsValid());
  return mojo::WrapUnsafeSharedMemoryRegion(region_.Duplicate());
}

gfx::GpuMemoryBufferHandle
SharedMemoryBufferTracker::GetGpuMemoryBufferHandle() {
  NOTREACHED() << "Unsupported operation";
  return gfx::GpuMemoryBufferHandle();
}

uint32_t SharedMemoryBufferTracker::GetMemorySizeInBytes() {
  DCHECK(region_.IsValid());
  return region_.GetSize();
}

}  // namespace media
