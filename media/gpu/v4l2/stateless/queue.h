// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
#define MEDIA_GPU_V4L2_STATELESS_QUEUE_H_

#include <set>

#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/stateless/stateless_device.h"

namespace media {

// V4L2 has two similar queues. Capitalized OUTPUT (for compressed frames)
// and CAPTURE (for uncompressed frames) are the designation that the V4L2
// framework uses. As these are counterintuitive for video decoding this class
// encapsulates the compressed frames into |InputQueue| and uncompressed frames
// into |OutputQueue|.
class MEDIA_GPU_EXPORT BaseQueue {
 public:
  BaseQueue(scoped_refptr<StatelessDevice> device,
            BufferType buffer_type,
            MemoryType memory_type);
  BaseQueue& operator=(const BaseQueue&);
  virtual ~BaseQueue() = 0;

  virtual bool PrepareBuffers() = 0;
  bool DeallocateBuffers();
  bool StartStreaming();
  bool StopStreaming();

 protected:
  bool AllocateBuffers(uint32_t num_planes);
  virtual std::string Description() = 0;

  scoped_refptr<StatelessDevice> device_;
  const BufferType buffer_type_;
  const MemoryType memory_type_;
  uint32_t num_planes_;
  std::vector<Buffer> buffers_;

  // Ordered set of free buffers. Because it is ordered the same index
  // will be used more often than if it was a ring buffer. Using a set
  // enforces the elements be unique.
  std::set<uint32_t> free_buffer_indices_;

 private:
  virtual uint32_t BufferMinimumCount() = 0;
};

class MEDIA_GPU_EXPORT InputQueue : public BaseQueue {
 public:
  static std::unique_ptr<InputQueue> Create(
      scoped_refptr<StatelessDevice> device,
      const VideoCodec codec,
      const gfx::Size resolution);

  InputQueue(scoped_refptr<StatelessDevice> device, VideoCodec codec);
  bool PrepareBuffers() override;

 private:
  bool SetupFormat(const gfx::Size resolution);
  std::string Description() override;
  uint32_t BufferMinimumCount() override;

  VideoCodec codec_;
};

}  // namespace media
#endif  // MEDIA_GPU_V4L2_STATELESS_QUEUE_H_
