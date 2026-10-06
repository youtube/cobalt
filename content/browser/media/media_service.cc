// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/media_service.h"

#include "base/no_destructor.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "content/browser/gpu/gpu_process_host.h"
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
#include "media/mojo/services/media_service_factory.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#endif

#if BUILDFLAG(IS_COBALT)
#include "mojo/public/cpp/bindings/callback_helpers.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace content {

#if BUILDFLAG(IS_COBALT)
namespace {

struct MediaServiceSlot {
  mojo::Remote<media::mojom::MediaService> remote;
  raw_ptr<media::mojom::MediaService> test_service = nullptr;
  bool is_concealed = false;
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
  std::unique_ptr<media::MediaService> service;
#endif
};

MediaServiceSlot& GetMediaServiceSlot() {
  static base::SequenceLocalStorageSlot<MediaServiceSlot> slot;
  return slot.GetOrCreateValue();
}

media::mojom::MediaService* GetConnectedMediaService(MediaServiceSlot& slot) {
  if (slot.test_service) {
    return slot.test_service;
  }
  if (slot.remote && slot.remote.is_connected()) {
    return slot.remote.get();
  }
  return nullptr;
}

}  // namespace

media::mojom::MediaService& GetMediaService() {
  // NOTE: We use sequence-local storage to limit the lifetime of this Remote to
  // that of the UI-thread sequence. This ensures that the Remote is destroyed
  // when the task environment is torn down and reinitialized, e.g. between unit
  // tests.
  auto& slot = GetMediaServiceSlot();
  if (slot.test_service) {
    return *slot.test_service;
  }
  auto& remote = slot.remote;
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    remote.reset_on_disconnect();

#if BUILDFLAG(IS_COBALT) && BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
    auto* process_host = GpuProcessHost::Get();
    if (process_host) {
      process_host->RunService(std::move(receiver));
    } else {
      DLOG(ERROR) << "GPU process host not available";
    }
#elif BUILDFLAG(IS_COBALT) && BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
    static_assert(media::mojom::MediaService::kServiceSandbox ==
                      sandbox::mojom::Sandbox::kNoSandbox,
                  "MediaService requested in-browser but not with kNoSandbox");
    slot.service = media::CreateMediaService(std::move(receiver));
#endif
    // Propagate concealed state if MediaService is lazily bound while
    // concealed.
#if BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
    if (slot.is_concealed) {
      remote->FlushAndSuspendActiveRenderers(base::DoNothing());
    }
#endif  // BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
  }

  return *remote.get();
}

void FlushAndSuspendMediaServiceOnUI(base::OnceClosure done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& slot = GetMediaServiceSlot();
  slot.is_concealed = true;
#if BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
  auto* service = GetConnectedMediaService(slot);
  if (!service) {
    std::move(done_cb).Run();
    return;
  }
  service->FlushAndSuspendActiveRenderers(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done_cb)));
#else
  std::move(done_cb).Run();
#endif  // BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
}

void ResumeMediaServiceOnUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& slot = GetMediaServiceSlot();
  slot.is_concealed = false;
#if BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
  if (auto* service = GetConnectedMediaService(slot)) {
    service->ResumeActiveRenderers();
  }
#endif  // BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
}

void OverrideMediaServiceForTesting(media::mojom::MediaService* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto& slot = GetMediaServiceSlot();
  slot.test_service = service;
#if BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
  if (service && slot.is_concealed) {
    service->FlushAndSuspendActiveRenderers(base::DoNothing());
  }
#endif  // BUILDFLAG(IS_COBALT) && BUILDFLAG(USE_STARBOARD_MEDIA)
}
#else
media::mojom::MediaService& GetMediaService() {
  // NOTE: We use sequence-local storage to limit the lifetime of this Remote to
  // that of the UI-thread sequence. This ensures that the Remote is destroyed
  // when the task environment is torn down and reinitialized, e.g. between unit
  // tests.
  static base::SequenceLocalStorageSlot<
      mojo::Remote<media::mojom::MediaService>>
      remote_slot;
  auto& remote = remote_slot.GetOrCreateValue();
  if (!remote) {
    auto receiver = remote.BindNewPipeAndPassReceiver();
    remote.reset_on_disconnect();

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
    auto* process_host = GpuProcessHost::Get();
    if (process_host) {
      process_host->RunService(std::move(receiver));
    } else {
      DLOG(ERROR) << "GPU process host not available";
    }
#elif BUILDFLAG(ENABLE_MOJO_MEDIA_IN_BROWSER_PROCESS)
    static_assert(media::mojom::MediaService::kServiceSandbox ==
                      sandbox::mojom::Sandbox::kNoSandbox,
                  "MediaService requested in-browser but not with kNoSandbox");
    static base::NoDestructor<std::unique_ptr<media::MediaService>> service;
    *service = media::CreateMediaService(std::move(receiver));
#endif
  }

  return *remote.get();
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace content
