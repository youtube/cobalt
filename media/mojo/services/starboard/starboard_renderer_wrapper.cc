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

#include "media/mojo/services/starboard/starboard_renderer_wrapper.h"

#include <utility>

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/base/starboard/starboard_rendering_mode.h"
#include "media/mojo/services/mojo_media_log.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media {

namespace {
void TestFunc() {
  LOG(ERROR) << "Cobalt: " << __func__;
}
}  // namespace

StarboardRendererWrapper::StarboardRendererWrapper(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    const base::UnguessableToken& overlay_plane_id,
    TimeDelta audio_write_duration_local,
    TimeDelta audio_write_duration_remote,
    mojo::PendingReceiver<RendererExtension> renderer_extension_receiver,
    mojo::PendingRemote<ClientExtension> client_extension_remote,
    base::RepeatingCallback<gpu::CommandBufferStub*(base::UnguessableToken,
                                                    int32_t)>
        get_command_buffer_stub_cb)
    : renderer_extension_receiver_(this,
                                   std::move(renderer_extension_receiver)),
      client_extension_remote_(std::move(client_extension_remote), task_runner),
      renderer_(std::make_unique<StarboardRenderer>(
          std::move(task_runner),
          std::make_unique<MojoMediaLog>(std::move(media_log_remote),
                                         task_runner),
          overlay_plane_id,
          audio_write_duration_local,
          audio_write_duration_remote)),
#if BUILDFLAG(IS_ANDROID)
      gpu_factory_(gpu_task_runner, std::move(get_command_buffer_stub_cb)),
#endif  // BUILDFLAG(IS_ANDROID)
      gpu_task_runner_(std::move(gpu_task_runner)) {
  DETACH_FROM_THREAD(thread_checker_);
}

StarboardRendererWrapper::~StarboardRendererWrapper() = default;

void StarboardRendererWrapper::Initialize(MediaResource* media_resource,
                                          RendererClient* client,
                                          PipelineStatusCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetStarboardRendererCallbacks(
      base::BindRepeating(
          &StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard,
          weak_factory_.GetWeakPtr()));
  renderer_->Initialize(media_resource, client, std::move(init_cb));
}

void StarboardRendererWrapper::Flush(base::OnceClosure flush_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->Flush(std::move(flush_cb));
}

void StarboardRendererWrapper::StartPlayingFrom(base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->StartPlayingFrom(time);
}

void StarboardRendererWrapper::SetPlaybackRate(double playback_rate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetPlaybackRate(playback_rate);
}

void StarboardRendererWrapper::SetVolume(float volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetVolume(volume);
}

void StarboardRendererWrapper::SetCdm(CdmContext* cdm_context,
                                      CdmAttachedCB cdm_attached_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetCdm(cdm_context, std::move(cdm_attached_cb));
}

void StarboardRendererWrapper::SetLatencyHint(
    absl::optional<base::TimeDelta> latency_hint) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->SetLatencyHint(latency_hint);
}

base::TimeDelta StarboardRendererWrapper::GetMediaTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return renderer_->GetMediaTime();
}

RendererType StarboardRendererWrapper::GetRendererType() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return RendererType::kStarboard;
}

void StarboardRendererWrapper::OnVideoGeometryChange(
    const gfx::Rect& output_rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  renderer_->OnVideoGeometryChange(output_rect);
}

void StarboardRendererWrapper::OnUpdateStarboardRenderingModeByStarboard(
    const StarboardRenderingMode mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_extension_remote_->UpdateStarboardRenderingMode(mode);
}

void StarboardRendererWrapper::Render(RenderCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  // Post renderer_->Render() to gpu thread.
  LOG(ERROR) << "Cobalt: " << __func__;
  base::RepeatingCallback<void()> get_current_decode_target_cb =
      base::BindRepeating(&StarboardRendererWrapper::GetCurrentDecodeTarget,
                          base::Unretained(this));
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  LOG(ERROR) << "Cobalt: " << __func__;
  gpu_factory_.AsyncCall(&StarboardGpuFactory::RunTaskOnGpuThreadv2)
      .WithArgs(std::move(get_current_decode_target_cb), &done_event);
  LOG(ERROR) << "Cobalt: " << __func__;
  // TODO(borongchen): Blocking is okay here because XYZ.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  done_event.Wait();
  LOG(ERROR) << "Cobalt: " << __func__;
  std::move(callback).Run(nullptr);
  LOG(ERROR) << "Cobalt: " << __func__;
#else   // BUILDFLAG(IS_ANDROID)
  auto video_frame = renderer_->Render();
  std::move(callback).Run(video_frame);
#endif  // BUILDFLAG(IS_ANDROID)
}

void StarboardRendererWrapper::OnGpuChannelTokenReady(
    mojom::CommandBufferIdPtr command_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << "Cobalt: " << __func__
             << " channel_token:" << command_buffer_id->channel_token
             << " route_id:" << command_buffer_id->route_id;

#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<gpu::RefCountedLock> ref_counted_lock;
  if (features::NeedThreadSafeAndroidMedia()) {
    ref_counted_lock = base::MakeRefCounted<gpu::RefCountedLock>();
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  LOG(ERROR) << "Cobalt: " << __func__;
  gpu_factory_.AsyncCall(&StarboardGpuFactory::Initialize)
      .WithArgs(command_buffer_id->channel_token, command_buffer_id->route_id,
                &done_event)
      .Then(base::BindOnce(&StarboardRendererWrapper::OnGpuFactoryInitialized,
                           weak_factory_.GetWeakPtr()));
  LOG(ERROR) << "Cobalt: " << __func__;
  // TODO(borongchen): Blocking is okay here because XYZ.
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  done_event.Wait();
  LOG(ERROR) << "Cobalt: " << __func__;
#endif  // BUILDFLAG(IS_ANDROID)

  LOG(ERROR) << "Cobalt: " << __func__;
  decode_target_graphics_context_provider_.gles_context_runner_context = this;
  decode_target_graphics_context_provider_.gles_context_runner =
      &StarboardRendererWrapper::GraphicsContextRunner;
  renderer_->set_decode_target_graphics_context_provider(base::BindRepeating(
      &StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider,
      base::Unretained(this)));
  LOG(ERROR) << "Cobalt: " << __func__;
}

#if BUILDFLAG(IS_ANDROID)
void StarboardRendererWrapper::OnGpuFactoryInitialized() {
  LOG(ERROR) << "Cobalt: " << __func__;
  is_gpu_factory_initialized_ = true;
  LOG(ERROR) << "Cobalt: " << __func__;
}
#endif  // BUILDFLAG(IS_ANDROID)

void StarboardRendererWrapper::OnPaintVideoHoleFrameByStarboard(
    const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_extension_remote_->PaintVideoHoleFrame(size);
}

SbDecodeTargetGraphicsContextProvider*
StarboardRendererWrapper::GetSbDecodeTargetGraphicsContextProvider() {
  return &decode_target_graphics_context_provider_;
}

// static
void StarboardRendererWrapper::GraphicsContextRunner(
    SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context) {
  LOG(ERROR) << "Cobalt: " << __func__;

  StarboardRendererWrapper* provider =
      reinterpret_cast<StarboardRendererWrapper*>(
          graphics_context_provider->gles_context_runner_context);

#if BUILDFLAG(IS_ANDROID)
  if (!provider->is_gpu_factory_initialized_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    return;
  }
  LOG(ERROR) << "Cobalt: " << __func__;
  if (provider->gpu_factory_) {
    LOG(ERROR) << "Cobalt: " << __func__;
    base::WaitableEvent done_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    LOG(ERROR) << "Cobalt: " << __func__;
    provider->gpu_factory_.AsyncCall(&StarboardGpuFactory::RunTaskOnGpuThread)
        .WithArgs(target_function, target_function_context, &done_event);
    LOG(ERROR) << "Cobalt: " << __func__;
    // TODO(borongchen): Blocking is okay here because XYZ.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    done_event.Wait();
    LOG(ERROR) << "Cobalt: " << __func__;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void StarboardRendererWrapper::GetCurrentDecodeTarget() {
  LOG(ERROR) << "Cobalt: " << __func__;
  renderer_->Render();
  LOG(ERROR) << "Cobalt: " << __func__;
}

}  // namespace media
