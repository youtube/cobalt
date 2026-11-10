// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/gpu/cobalt_content_gpu_client.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cobalt/browser/memory_ablation.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "components/viz/service/display/starboard/video_geometry_setter.h"
#include "content/public/child/child_thread.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"                          // nogncheck
#include "gpu/ipc/service/gpu_channel_manager.h"        // nogncheck
#include "media/base/media_switches.h"                  // nogncheck
#include "starboard/extension/protected_video_plane.h"  // nogncheck
#include "starboard/system.h"                           // nogncheck
#include "ui/gfx/extension_set.h"                       // nogncheck
#include "ui/gl/gl_bindings.h"                          // nogncheck
#include "ui/gl/gl_display.h"                           // nogncheck
#include "ui/gl/gl_utils.h"                             // nogncheck
#endif                                                  // BUILDFLAG(IS_ANDROID)

namespace cobalt {

#if BUILDFLAG(IS_ANDROID)
namespace {

// Whether this GPU can create the protected contexts and window surfaces of
// protected WebGL (see GLES2CommandBufferStub::Initialize()). Known without
// creating one, which would take the video window.
bool CanCreateProtectedContexts(gpu::GpuChannelManager* channel_manager) {
  if (!channel_manager->gpu_preferences().use_passthrough_cmd_decoder) {
    return false;
  }
  gl::GLDisplayEGL* display = gl::GetDefaultDisplayEGL();
  if (!display || !display->IsInitialized() ||
      !display->IsEGLSurfacelessContextSupported()) {
    return false;
  }
  const char* extensions =
      eglQueryString(display->GetDisplay(), EGL_EXTENSIONS);
  return extensions && gfx::HasExtension(gfx::MakeExtensionSet(extensions),
                                         "EGL_EXT_protected_content");
}

// Tells Starboard whether secure decoders may decode to texture. This also
// answers `decode-to-texture=protected` in MediaSource.isTypeSupported(), so
// a page can tell before playback whether protected WebGL will work.
void UpdateProtectedDecodeToTexture(gpu::GpuChannelManager* channel_manager) {
  const auto* api =
      static_cast<const StarboardExtensionProtectedVideoPlaneApi*>(
          SbSystemGetExtension(kStarboardExtensionProtectedVideoPlaneName));
  if (!api) {
    return;
  }
  const bool enabled =
      base::FeatureList::IsEnabled(::media::kCobaltProtectedWebGL) &&
      CanCreateProtectedContexts(channel_manager);
  api->SetProtectedDecodeToTextureEnabled(enabled);
}

}  // namespace
#endif  // BUILDFLAG(IS_ANDROID)

CobaltContentGpuClient::CobaltContentGpuClient()
    : video_geometry_setter_service_(
          std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                          base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr))) {}

CobaltContentGpuClient::~CobaltContentGpuClient() = default;

void CobaltContentGpuClient::GpuServiceInitialized(
    gpu::GpuChannelManager* channel_manager) {
  MaybeApplyGpuMemoryAblation(channel_manager);
#if BUILDFLAG(IS_ANDROID)
  UpdateProtectedDecodeToTexture(channel_manager);
#endif  // BUILDFLAG(IS_ANDROID)
}

void CobaltContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  // Initialize PendingRemote for VideoGeometrySetter and post it
  // to compositor thread (viz service). This is called on gpu thread
  // right after the compositor thread is created.
  mojo::PendingRemote<cobalt::media::mojom::VideoGeometrySetter>
      video_geometry_setter;
  mojo::PendingReceiver<cobalt::media::mojom::VideoGeometrySetter>
      pending_receiver = video_geometry_setter.InitWithNewPipeAndPassReceiver();
  video_geometry_setter_service_->GetVideoGeometrySetter(
      std::move(pending_receiver));

  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&viz::ConnectVideoGeometrySetter,
                                       std::move(video_geometry_setter)));
}

media::VideoGeometrySetterService*
CobaltContentGpuClient::GetVideoGeometrySetterService() {
  if (!video_geometry_setter_service_) {
    CreateVideoGeometrySetterService();
  }
  return video_geometry_setter_service_.get();
}

void CobaltContentGpuClient::CreateVideoGeometrySetterService() {
  DCHECK(!video_geometry_setter_service_);
  video_geometry_setter_service_ =
      std::unique_ptr<cobalt::media::VideoGeometrySetterService,
                      base::OnTaskRunnerDeleter>(
          new media::VideoGeometrySetterService,
          base::OnTaskRunnerDeleter(
              base::SingleThreadTaskRunner::GetCurrentDefault()));
}

}  // namespace cobalt
