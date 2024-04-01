// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_POOL_H_
#define MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_POOL_H_

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

// Simple VideoFrame pool used to avoid unnecessarily allocating and destroying
// VideoFrame objects. The pool manages the memory for the VideoFrame
// returned by GetFrame(). When one of these VideoFrames is destroyed,
// the memory is returned to the pool for use by a subsequent GetFrame()
// call. The memory in the pool is retained for the life of the
// PlatformVideoFramePool object. Before calling GetFrame(), the client should
// call NegotiateFrameFormat(). If the parameters passed to
// NegotiateFrameFormat() are changed, then the memory used by frames with the
// old parameter values will be purged from the pool.
class MEDIA_GPU_EXPORT PlatformVideoFramePool : public DmabufVideoFramePool {
 public:
  explicit PlatformVideoFramePool(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory);

  PlatformVideoFramePool(const PlatformVideoFramePool&) = delete;
  PlatformVideoFramePool& operator=(const PlatformVideoFramePool&) = delete;

  ~PlatformVideoFramePool() override;

  // Returns the ID of the GpuMemoryBuffer wrapped by |frame|.
  static gfx::GpuMemoryBufferId GetGpuMemoryBufferId(const VideoFrame& frame);

  // DmabufVideoFramePool implementation.
  StatusOr<GpuBufferLayout> Initialize(const Fourcc& fourcc,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size,
                                       size_t max_num_frames,
                                       bool use_protected) override;
  scoped_refptr<VideoFrame> GetFrame() override;
  bool IsExhausted() override;
  void NotifyWhenFrameAvailable(base::OnceClosure cb) override;
  void ReleaseAllFrames() override;

  // Returns the original frame of a wrapped frame. We need this method to
  // determine whether the frame returned by GetFrame() is the same one after
  // recycling, and bind destruction callback at original frames.
  VideoFrame* UnwrapFrame(const VideoFrame& wrapped_frame);

  // Returns the number of frames in the pool for testing purposes.
  size_t GetPoolSizeForTesting();

 private:
  friend class PlatformVideoFramePoolTest;

  // Thunk to post OnFrameReleased() to |task_runner|.
  // Because this thunk may be called in any thread, We don't want to
  // dereference WeakPtr. Therefore we wrap the WeakPtr by absl::optional to
  // avoid the task runner defererencing the WeakPtr.
  static void OnFrameReleasedThunk(
      absl::optional<base::WeakPtr<PlatformVideoFramePool>> pool,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      scoped_refptr<VideoFrame> origin_frame);
  // Called when a wrapped frame gets destroyed.
  // When returning a frame to the pool, the pool might have already been
  // destroyed. In this case, the WeakPtr of the pool will have been invalidated
  // at |parent_task_runner_|, and OnFrameReleased() will not get executed.
  void OnFrameReleased(scoped_refptr<VideoFrame> origin_frame);

  void InsertFreeFrame_Locked(scoped_refptr<VideoFrame> frame)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  size_t GetTotalNumFrames_Locked() const EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsSameFormat_Locked(VideoPixelFormat format,
                           const gfx::Size& coded_size,
                           const gfx::Rect& visible_rect,
                           bool use_protected) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  bool IsExhausted_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // The function used to allocate new frames.
  using CreateFrameCB = base::RepeatingCallback<scoped_refptr<VideoFrame>(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      bool use_protected,
      base::TimeDelta timestamp)>;
  CreateFrameCB create_frame_cb_;

  // Lock to protect all data members.
  // Every public method and OnFrameReleased() acquire this lock.
  base::Lock lock_;

  // Used to allocate the video frame GpuMemoryBuffers, passed directly to
  // the callback that creates video frames. Indirectly owned by GpuChildThread;
  // therefore alive as long as the GPU process is.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_ = nullptr;

  // The arguments of current frame. We allocate new frames only if a pixel
  // format or size in |frame_layout_| is changed. When GetFrame() is
  // called, we update |visible_rect_| and |natural_size_| of wrapped frames.
  absl::optional<GpuBufferLayout> frame_layout_ GUARDED_BY(lock_);
  gfx::Rect visible_rect_ GUARDED_BY(lock_);
  gfx::Size natural_size_ GUARDED_BY(lock_);

  // The pool of free frames. The layout of all the frames in |free_frames_|
  // should be the same as |format_| and |coded_size_|.
  base::circular_deque<scoped_refptr<VideoFrame>> free_frames_
      GUARDED_BY(lock_);
  // Mapping from the frame's GpuMemoryBuffer's ID to the original frame.
  std::map<gfx::GpuMemoryBufferId, VideoFrame*> frames_in_use_
      GUARDED_BY(lock_);

  // The maximum number of frames created by the pool.
  size_t max_num_frames_ GUARDED_BY(lock_) = 0;

  // If we are using HW protected buffers.
  bool use_protected_ GUARDED_BY(lock_) = false;

  // Callback which is called when the pool is not exhausted.
  base::OnceClosure frame_available_cb_ GUARDED_BY(lock_);

  // The weak pointer of this, bound at |parent_task_runner_|.
  // Used at the VideoFrame destruction callback.
  base::WeakPtr<PlatformVideoFramePool> weak_this_;
  base::WeakPtrFactory<PlatformVideoFramePool> weak_this_factory_{this};
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_POOL_H_
