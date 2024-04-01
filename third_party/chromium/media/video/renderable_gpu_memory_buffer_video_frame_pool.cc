// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

class InternalRefCountedPool;

// The VideoFrame-backing resources that are reused by the pool, namely, a
// GpuMemoryBuffer and a per-plane SharedImage. This retains a reference to
// the InternalRefCountedPool that created it.
class FrameResources {
 public:
  FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                 const gfx::Size& coded_size);
  ~FrameResources();
  FrameResources(const FrameResources& other) = delete;
  FrameResources& operator=(const FrameResources& other) = delete;

  // Allocate GpuMemoryBuffer and create SharedImages. Returns false on failure.
  bool Initialize();

  // Return true if these resources can be reused for a frame with the specified
  // parameters.
  bool IsCompatibleWith(const gfx::Size& coded_size) const;

  // Create a VideoFrame using these resources. This will transfer ownership of
  // the GpuMemoryBuffer to the VideoFrame.
  scoped_refptr<VideoFrame> CreateVideoFrameAndTakeGpuMemoryBuffer();

  // Return ownership of the GpuMemory to `this`, and update the sync tokens
  // of the MailboxHolders to `sync_token`.
  void ReturnGpuMemoryBufferFromFrame(
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
      const gpu::SyncToken& sync_token);

 private:
  // This reference ensures that the creating InternalRefCountedPool (and,
  // critically, its interface through which `this` can destroy its
  // SharedImages) will not be destroyed until after `this` is destroyed.
  const scoped_refptr<InternalRefCountedPool> pool_;

  const gfx::Size coded_size_;
  const gfx::ColorSpace color_space_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  gpu::MailboxHolder mailbox_holders_[VideoFrame::kMaxPlanes];
};

// The owner of the RenderableGpuMemoryBufferVideoFramePool::Client needs to be
// reference counted to ensure that not be destroyed while there still exist any
// FrameResources.
class InternalRefCountedPool : public base::RefCounted<InternalRefCountedPool> {
 public:
  explicit InternalRefCountedPool(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
          context);

  // Create a VideoFrame with the specified parameters, reusing the resources
  // of a previous frame, if possible.
  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(const gfx::Size& coded_size);

  // Indicate that the owner of `this` is being destroyed. This will eventually
  // cause `this` to be destroyed (once all of the FrameResources it created are
  // destroyed, which will happen only once all of the VideoFrames it created
  // are destroyed).
  void Shutdown();

  // Return the Context for accessing the GpuMemoryBufferManager and
  // SharedImageInterface. Never returns nullptr.
  RenderableGpuMemoryBufferVideoFramePool::Context* GetContext() const;

 private:
  friend class base::RefCounted<InternalRefCountedPool>;
  ~InternalRefCountedPool();

  // Callback made whe a created VideoFrame is destroyed. Returns
  // `gpu_memory_buffer` to `frame_resources`, and then either returns
  // `frame_resources` to `available_frame_resources_` or destroys it.
  void OnVideoFrameDestroyed(
      std::unique_ptr<FrameResources> frame_resources,
      const gpu::SyncToken& sync_token,
      std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer);

  const std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
      context_;
  std::list<std::unique_ptr<FrameResources>> available_frame_resources_;
  bool shutting_down_ = false;
};

// Implementation of the RenderableGpuMemoryBufferVideoFramePool abstract
// class.
class RenderableGpuMemoryBufferVideoFramePoolImpl
    : public RenderableGpuMemoryBufferVideoFramePool {
 public:
  explicit RenderableGpuMemoryBufferVideoFramePoolImpl(
      std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
          context);

  scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& coded_size) override;

  ~RenderableGpuMemoryBufferVideoFramePoolImpl() override;

  const scoped_refptr<InternalRefCountedPool> pool_internal_;
};

////////////////////////////////////////////////////////////////////////////////
// FrameResources

FrameResources::FrameResources(scoped_refptr<InternalRefCountedPool> pool,
                               const gfx::Size& coded_size)
    : pool_(std::move(pool)),
      coded_size_(coded_size),
      color_space_(gfx::ColorSpace::CreateREC709()) {}

FrameResources::~FrameResources() {
  auto* context = pool_->GetContext();
  for (auto& holder : mailbox_holders_) {
    if (holder.mailbox.IsZero())
      continue;
    context->DestroySharedImage(holder.sync_token, holder.mailbox);
  }
}

bool FrameResources::Initialize() {
  auto* context = pool_->GetContext();

  constexpr gfx::BufferUsage kBufferUsage =
#if defined(OS_MAC)
      gfx::BufferUsage::SCANOUT_VEA_CPU_READ
#else
      gfx::BufferUsage::SCANOUT_CPU_READ_WRITE
#endif
      ;

  // Create the GpuMemoryBuffer.
  gpu_memory_buffer_ = context->CreateGpuMemoryBuffer(
      coded_size_, gfx::BufferFormat::YUV_420_BIPLANAR, kBufferUsage);
  if (!gpu_memory_buffer_) {
    DLOG(ERROR) << "Failed to allocate GpuMemoryBuffer for frame.";
    return false;
  }

  // Bind SharedImages to each plane.
  constexpr size_t kNumPlanes = 2;
  constexpr gfx::BufferPlane kPlanes[kNumPlanes] = {gfx::BufferPlane::Y,
                                                    gfx::BufferPlane::UV};
  constexpr uint32_t kSharedImageUsage =
#if defined(OS_MAC)
      gpu::SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX |
#endif
      gpu::SHARED_IMAGE_USAGE_GLES2 | gpu::SHARED_IMAGE_USAGE_RASTER |
      gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;

  for (size_t plane = 0; plane < kNumPlanes; ++plane) {
    context->CreateSharedImage(
        gpu_memory_buffer_.get(), kPlanes[plane], color_space_,
        kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, kSharedImageUsage,
        mailbox_holders_[plane].mailbox, mailbox_holders_[plane].sync_token);
    // TODO(https://crbug.com/1191956): This should be parameterized.
#if defined(OS_MAC)
    mailbox_holders_[plane].texture_target = GL_TEXTURE_RECTANGLE_ARB;
#else
    mailbox_holders_[plane].texture_target = GL_TEXTURE_2D;
#endif
  }
  return true;
}

bool FrameResources::IsCompatibleWith(const gfx::Size& coded_size) const {
  return coded_size_ == coded_size;
}

scoped_refptr<VideoFrame>
FrameResources::CreateVideoFrameAndTakeGpuMemoryBuffer() {
  const gfx::Rect visible_rect(coded_size_);
  const gfx::Size natural_size = coded_size_;
  auto video_frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      visible_rect, natural_size, std::move(gpu_memory_buffer_),
      mailbox_holders_, VideoFrame::ReleaseMailboxAndGpuMemoryBufferCB(),
      base::TimeDelta());
  if (!video_frame)
    return nullptr;

  video_frame->set_color_space(color_space_);

  // TODO(https://crbug.com/1191956): This should depend on the platform and
  // format.
  video_frame->metadata().allow_overlay = true;

  // Only native (non shared memory) GMBs require waiting on GPU fences.
  const bool has_native_gmb =
      video_frame->HasGpuMemoryBuffer() &&
      video_frame->GetGpuMemoryBuffer()->GetType() != gfx::SHARED_MEMORY_BUFFER;
  video_frame->metadata().read_lock_fences_enabled = has_native_gmb;

  return video_frame;
}

void FrameResources::ReturnGpuMemoryBufferFromFrame(
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    const gpu::SyncToken& sync_token) {
  DCHECK(!gpu_memory_buffer_);
  gpu_memory_buffer_ = std::move(gpu_memory_buffer);
  for (auto& holder : mailbox_holders_)
    holder.sync_token = sync_token;
}

////////////////////////////////////////////////////////////////////////////////
// InternalRefCountedPool

InternalRefCountedPool::InternalRefCountedPool(
    std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context> context)
    : context_(std::move(context)) {}

scoped_refptr<VideoFrame> InternalRefCountedPool::MaybeCreateVideoFrame(
    const gfx::Size& coded_size) {
  // Find or create a suitable FrameResources.
  std::unique_ptr<FrameResources> frame_resources;
  while (!available_frame_resources_.empty()) {
    frame_resources = std::move(available_frame_resources_.front());
    available_frame_resources_.pop_front();
    if (!frame_resources->IsCompatibleWith(coded_size)) {
      frame_resources = nullptr;
      continue;
    }
  }
  if (!frame_resources) {
    frame_resources = std::make_unique<FrameResources>(this, coded_size);
    if (!frame_resources->Initialize()) {
      DLOG(ERROR) << "Failed to initialize frame resources.";
      return nullptr;
    }
  }
  DCHECK(frame_resources);

  // Create a VideoFrame from the FrameResources.
  auto video_frame = frame_resources->CreateVideoFrameAndTakeGpuMemoryBuffer();
  if (!video_frame) {
    DLOG(ERROR) << "Failed to create VideoFrame from FrameResources.";
    return nullptr;
  }

  // Set the ReleaseMailboxAndGpuMemoryBufferCB to return the GpuMemoryBuffer to
  // the FrameResources, and return the FrameResources to the available pool. Do
  // this on the calling thread.
  auto callback = base::BindOnce(&InternalRefCountedPool::OnVideoFrameDestroyed,
                                 this, std::move(frame_resources));
  video_frame->SetReleaseMailboxAndGpuMemoryBufferCB(base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(), std::move(callback), FROM_HERE));
  return video_frame;
}

void InternalRefCountedPool::OnVideoFrameDestroyed(
    std::unique_ptr<FrameResources> frame_resources,
    const gpu::SyncToken& sync_token,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer) {
  frame_resources->ReturnGpuMemoryBufferFromFrame(std::move(gpu_memory_buffer),
                                                  sync_token);

  if (shutting_down_)
    return;

  // TODO(https://crbug.com/1191956): Determine if we can get away with just
  // having 1 available frame, or if that will cause flakey underruns.
  constexpr size_t kMaxAvailableFrames = 2;
  available_frame_resources_.push_back(std::move(frame_resources));
  while (available_frame_resources_.size() > kMaxAvailableFrames)
    available_frame_resources_.pop_front();
}

void InternalRefCountedPool::Shutdown() {
  shutting_down_ = true;
  available_frame_resources_.clear();
}

RenderableGpuMemoryBufferVideoFramePool::Context*
InternalRefCountedPool::GetContext() const {
  return context_.get();
}

InternalRefCountedPool::~InternalRefCountedPool() {
  DCHECK(shutting_down_);
  DCHECK(available_frame_resources_.empty());
}

////////////////////////////////////////////////////////////////////////////////
// RenderableGpuMemoryBufferVideoFramePoolImpl

RenderableGpuMemoryBufferVideoFramePoolImpl::
    RenderableGpuMemoryBufferVideoFramePoolImpl(
        std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool::Context>
            context)
    : pool_internal_(
          base::MakeRefCounted<InternalRefCountedPool>(std::move(context))) {}

scoped_refptr<VideoFrame>
RenderableGpuMemoryBufferVideoFramePoolImpl::MaybeCreateVideoFrame(
    const gfx::Size& coded_size) {
  return pool_internal_->MaybeCreateVideoFrame(coded_size);
}

RenderableGpuMemoryBufferVideoFramePoolImpl::
    ~RenderableGpuMemoryBufferVideoFramePoolImpl() {
  pool_internal_->Shutdown();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// media::RenderableGpuMemoryBufferVideoFramePool

// static
std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool>
RenderableGpuMemoryBufferVideoFramePool::Create(
    std::unique_ptr<Context> context) {
  return std::make_unique<RenderableGpuMemoryBufferVideoFramePoolImpl>(
      std::move(context));
}

}  // namespace media
