// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"

#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory_core.h"

#include <cstdint>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/media/media_devices_permission_checker.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

void GetSaltOriginAndPermissionsOnUIThread(
    int process_id,
    int frame_id,
    base::OnceCallback<void(MediaDeviceSaltAndOrigin salt_and_origin,
                            bool has_access)> cb) {
  auto salt_and_origin = GetMediaDeviceSaltAndOrigin(process_id, frame_id);
  bool access = MediaDevicesPermissionChecker().CheckPermissionOnUIThread(
      MediaDeviceType::MEDIA_AUDIO_OUTPUT, process_id, frame_id);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(cb), std::move(salt_and_origin), access));
}

}  // namespace

RenderFrameAudioInputStreamFactory::RenderFrameAudioInputStreamFactory(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver,
    MediaStreamManager* media_stream_manager,
    RenderFrameHost* render_frame_host)
    : core_(new Core(std::move(receiver),
                     media_stream_manager,
                     render_frame_host)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RenderFrameAudioInputStreamFactory::~RenderFrameAudioInputStreamFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure |core_| is deleted on the right thread. DeleteOnIOThread isn't used
  // as it doesn't post in case it is already executed on the right thread. That
  // causes issues in unit tests where the UI thread and the IO thread are the
  // same.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(core_)));
}

void RenderFrameAudioInputStreamFactory::Init(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  core_->Init(std::move(receiver));
}

// static
RenderFrameAudioInputStreamFactory::Core*
RenderFrameAudioInputStreamFactory::Core::Get(int render_process_id,
                                              int render_frame_id) {
  base::AutoLock lock(GetLock());
  auto it = GetMap().find({render_process_id, render_frame_id});
  if (it != GetMap().end()) {
    return it->second;
  }
  return nullptr;
}

// static
base::Lock& RenderFrameAudioInputStreamFactory::Core::GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// static
RenderFrameAudioInputStreamFactory::Core::CoreMap&
RenderFrameAudioInputStreamFactory::Core::GetMap() {
  static base::NoDestructor<CoreMap> map;
  return *map;
}

RenderFrameAudioInputStreamFactory::Core::Core(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver,
    MediaStreamManager* media_stream_manager,
    RenderFrameHost* render_frame_host)
    : media_stream_manager_(media_stream_manager),
      process_id_(render_frame_host->GetProcess()->GetID()),
      frame_id_(render_frame_host->GetRoutingID()),
      origin_(render_frame_host->GetLastCommittedOrigin()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  {
    base::AutoLock lock(GetLock());
    GetMap()[{process_id_, frame_id_}] = this;
  }

  ForwardingAudioStreamFactory::Core* tmp_factory =
      ForwardingAudioStreamFactory::CoreForFrame(render_frame_host);

  if (!tmp_factory) {
    // The only case when we not have a forwarding factory at this point is when
    // the frame belongs to an interstitial. Interstitials don't need audio, so
    // it's fine to drop the receiver.
    return;
  }

  forwarding_factory_ = tmp_factory->AsWeakPtr();

  // Unretained is safe since the destruction of |this| is posted to the IO
  // thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Init, base::Unretained(this), std::move(receiver)));
}

RenderFrameAudioInputStreamFactory::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::AutoLock lock(GetLock());
  GetMap().erase({process_id_, frame_id_});
}

void RenderFrameAudioInputStreamFactory::Core::Init(
    mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  receiver_.Bind(std::move(receiver));
}

void RenderFrameAudioInputStreamFactory::Core::CreateStream(
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client,
    const base::UnguessableToken& session_id,
    const media::AudioParameters& audio_params,
    bool automatic_gain_control,
    uint32_t shared_memory_count,
    media::mojom::AudioProcessingConfigPtr processing_config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  LOG(INFO) << "RFAISF::Core::CreateStream session_id=" << session_id.ToString();

  if (!forwarding_factory_) {
    LOG(ERROR) << "RFAISF::Core::CreateStream NO FORWARDING FACTORY";
    return;
  }

  const blink::MediaStreamDevice* device =
      media_stream_manager_->audio_input_device_manager()->GetOpenedDeviceById(
          session_id);

  if (!device) {
    LOG(ERROR) << "RFAISF::Core::CreateStream DEVICE NOT FOUND for session_id="
               << session_id.ToString();
    return;
  }

  LOG(INFO) << "RFAISF::Core::CreateStream calling forwarding_factory_->CreateInputStream";
  forwarding_factory_->CreateInputStream(
      process_id_, frame_id_, device->id, audio_params, shared_memory_count,
      automatic_gain_control, std::move(processing_config), std::move(client));
}

void RenderFrameAudioInputStreamFactory::Core::AssociateInputAndOutputForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetSaltOriginAndPermissionsOnUIThread, process_id_,
                     frame_id_,
                     base::BindOnce(&Core::AssociateInputAndOutputForAecAfterCheckingAccess,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    input_stream_id, output_device_id)));
}

void RenderFrameAudioInputStreamFactory::Core::CreateLoopbackStream(
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client,
    const media::AudioParameters& audio_params,
    uint32_t shared_memory_count,
    bool disable_local_echo,
    AudioStreamBroker::LoopbackSource* loopback_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (forwarding_factory_) {
    forwarding_factory_->CreateLoopbackStream(
        process_id_, frame_id_, loopback_source, audio_params,
        shared_memory_count, /*mute_source=*/!disable_local_echo,
        std::move(client));
  }
}

void RenderFrameAudioInputStreamFactory::Core::AssociateInputAndOutputForAecAfterCheckingAccess(
    const base::UnguessableToken& input_stream_id,
    const std::string& output_device_id,
    MediaDeviceSaltAndOrigin salt_and_origin,
    bool access_granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!forwarding_factory_ || !access_granted)
    return;

  // We should ideally translate the device ID here using MediaStreamManager,
  // but for a "fast-path" and since we're in single process, we'll try to
  // use the output_device_id directly or handle it in Forwarding factory.
  forwarding_factory_->AssociateInputAndOutputForAec(input_stream_id,
                                                     output_device_id);
}

void RenderFrameAudioInputStreamFactory::Core::AssociateTranslatedOutputDeviceForAec(
    const base::UnguessableToken& input_stream_id,
    const std::string& raw_output_device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (forwarding_factory_) {
    forwarding_factory_->AssociateInputAndOutputForAec(input_stream_id,
                                                       raw_output_device_id);
  }
}

}  // namespace content
