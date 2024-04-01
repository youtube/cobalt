// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
#define MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace gl {
class GLContext;
class GLImage;
}

namespace gpu {
struct GpuPreferences;

namespace gles2 {
class ContextGroup;
}
}

namespace media {

class MediaLog;

class MEDIA_GPU_EXPORT GpuVideoDecodeAcceleratorFactory {
 public:
  ~GpuVideoDecodeAcceleratorFactory();

  // Return current GLContext.
  using GetGLContextCallback = base::RepeatingCallback<gl::GLContext*(void)>;

  // Make the applicable GL context current. To be called by VDAs before
  // executing any GL calls. Return true on success, false otherwise.
  using MakeGLContextCurrentCallback = base::RepeatingCallback<bool(void)>;

  // Bind |image| to |client_texture_id| given |texture_target|. If
  // |can_bind_to_sampler| is true, then the image may be used as a sampler
  // directly, otherwise a copy to a staging buffer is required.
  // Return true on success, false otherwise.
  using BindGLImageCallback =
      base::RepeatingCallback<bool(uint32_t client_texture_id,
                                   uint32_t texture_target,
                                   const scoped_refptr<gl::GLImage>& image,
                                   bool can_bind_to_sampler)>;

  // Return a ContextGroup*, if one is available.
  using GetContextGroupCallback =
      base::RepeatingCallback<gpu::gles2::ContextGroup*(void)>;

  static std::unique_ptr<GpuVideoDecodeAcceleratorFactory> Create(
      const GpuVideoDecodeGLClient& gl_client);

  static gpu::VideoDecodeAcceleratorCapabilities GetDecoderCapabilities(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds);

  std::unique_ptr<VideoDecodeAccelerator> CreateVDA(
      VideoDecodeAccelerator::Client* client,
      const VideoDecodeAccelerator::Config& config,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log = nullptr);

 private:
  GpuVideoDecodeAcceleratorFactory(const GpuVideoDecodeGLClient& gl_client);

#if defined(OS_WIN)
  std::unique_ptr<VideoDecodeAccelerator> CreateD3D11VDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
  std::unique_ptr<VideoDecodeAccelerator> CreateDXVAVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if BUILDFLAG(USE_VAAPI)
  std::unique_ptr<VideoDecodeAccelerator> CreateVaapiVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#elif BUILDFLAG(USE_V4L2_CODEC)
  std::unique_ptr<VideoDecodeAccelerator> CreateV4L2VDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
  std::unique_ptr<VideoDecodeAccelerator> CreateV4L2SliceVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if defined(OS_MAC)
  std::unique_ptr<VideoDecodeAccelerator> CreateVTVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif
#if defined(OS_ANDROID)
  std::unique_ptr<VideoDecodeAccelerator> CreateAndroidVDA(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log) const;
#endif

  const GpuVideoDecodeGLClient gl_client_;
  const AndroidOverlayMojoFactoryCB overlay_factory_cb_;
  base::ThreadChecker thread_checker_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAcceleratorFactory);
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
