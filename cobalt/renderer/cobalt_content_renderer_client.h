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

#ifndef COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
#define COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_

#include <atomic>
#include <map>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/hang_watcher.h"
#include "cobalt/browser/mojom/h5vcc_settings.mojom.h"
#include "cobalt/common/cobalt_thread_checker.h"
#include "cobalt/media/audio/cobalt_audio_device_factory.h"
#include "cobalt/media/service/mojom/platform_window_provider.mojom.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/key_systems_support_registration.h"
#include "media/base/starboard/renderer_factory_traits.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class ExternalMemoryAllocator;
class MediaLog;
class RendererFactory;
}  // namespace media

namespace cobalt {
// This class utilizes embedder API for participating in renderer logic.
// It allows Cobalt to customize content Renderer module.
class CobaltContentRendererClient : public content::ContentRendererClient {
 public:
  static CobaltContentRendererClient* Get();

  CobaltContentRendererClient();

  CobaltContentRendererClient(const CobaltContentRendererClient&) = delete;
  CobaltContentRendererClient& operator=(const CobaltContentRendererClient&) =
      delete;

  ~CobaltContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RenderThreadStarted() override;
  virtual std::unique_ptr<::media::KeySystemSupportRegistration>
  GetSupportedKeySystems(content::RenderFrame* render_frame,
                         ::media::GetSupportedKeySystemsCB cb) override;
  bool IsDecoderSupportedAudioType(const ::media::AudioType& type) override;
  bool IsDecoderSupportedVideoType(const ::media::VideoType& type) override;
  ::media::ExternalMemoryAllocator* GetMediaAllocator() override;
  // JS Injection hook
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void GetStarboardRendererFactoryTraits(
      content::RenderFrame* render_frame,
      ::media::RendererFactoryTraits* traits) override;
  void PostSandboxInitialized() override;
  void OnFrameDestroyed(content::RenderFrame* render_frame);

  uint64_t GetSbWindowHandleForFrame(content::RenderFrame* render_frame) const;

 private:
  void EnsureH5vccSettingsRemoteInitialized();
  void OnGetSbWindow(content::RenderFrame* frame, uint64_t handle);

  // Registers a custom content::AudioDeviceFactory
  ::media::CobaltAudioDeviceFactory cobalt_audio_device_factory_;

  mutable base::Lock media_allocator_lock_;
  bool is_external_memory_pool_enabled_ GUARDED_BY(media_allocator_lock_) =
      false;
  std::unique_ptr<::media::ExternalMemoryAllocator> media_memory_allocator_
      GUARDED_BY(media_allocator_lock_);

  std::unique_ptr<mojo::Remote<cobalt::mojom::H5vccSettings>,
                  base::OnTaskRunnerDeleter>
      h5vcc_settings_remote_;

  base::ScopedClosureRunner unregister_thread_closure;

  gfx::Size viewport_size_;

  std::map<content::RenderFrame*, uint64_t> frame_to_window_map_;

  COBALT_THREAD_CHECKER(main_thread_checker_);

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<CobaltContentRendererClient> weak_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
