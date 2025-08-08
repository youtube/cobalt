// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/render_media_client.h"

#include "base/command_line.h"
#include "base/time/default_tick_clock.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "media/base/video_color_space.h"
#include "media/mojo/buildflags.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/display/display_switches.h"

namespace {

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
void UpdateDecoderVideoProfilesInternal(
    const media::SupportedVideoDecoderConfigs& supported_configs) {
  base::flat_set<media::VideoCodecProfile> media_profiles;
  for (const auto& config : supported_configs) {
    for (int profile = config.profile_min; profile <= config.profile_max;
         profile++) {
      media_profiles.insert(static_cast<media::VideoCodecProfile>(profile));
    }
  }
  media::UpdateDefaultDecoderSupportedVideoProfiles(media_profiles);
}
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
void UpdateDecoderAudioTypesInternal(
    const media::SupportedAudioDecoderConfigs& supported_configs) {
  base::flat_set<media::AudioType> supported_types;
  for (const auto& config : supported_configs) {
    media::AudioType type{config.codec, config.profile, false};
    supported_types.insert(type);
  }
  media::UpdateDefaultDecoderSupportedAudioTypes(supported_types);
}
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
void UpdateEncoderVideoProfilesInternal(
    const media::VideoEncodeAccelerator::SupportedProfiles& supported_configs) {
  base::flat_set<media::VideoCodecProfile> media_profiles;
  for (const auto& config : supported_configs) {
    media_profiles.insert(
        static_cast<media::VideoCodecProfile>(config.profile));
  }
  media::UpdateDefaultEncoderSupportedVideoProfiles(media_profiles);
}
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)

}  // namespace

namespace content {

void RenderMediaClient::Initialize() {
  static RenderMediaClient* client = new RenderMediaClient();
  media::SetMediaClient(client);
}

RenderMediaClient::RenderMediaClient()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT) || \
    BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  RenderThreadImpl::current()->BindHostReceiver(
      interface_factory_for_supported_profiles_.BindNewPipeAndPassReceiver());
  interface_factory_for_supported_profiles_.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnInterfaceFactoryDisconnected,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)));
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT) ||
        // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  // We'll first try to query the supported video decoder configurations
  // asynchronously. If IsDecoderSupportedVideoType() is called before we get a
  // response, that method will block if its not on the main thread or fall
  // back to querying the video decoder configurations synchronously otherwise.

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  switch (media::GetOutOfProcessVideoDecodingMode()) {
    case media::OOPVDMode::kEnabledWithoutGpuProcessAsProxy: {
      mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>
          stable_video_decoder_remote;
      interface_factory_for_supported_profiles_->CreateStableVideoDecoder(
          stable_video_decoder_remote.BindNewPipeAndPassReceiver());
      stable_video_decoder_remote.set_disconnect_handler(
          base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                         // base::Unretained(this) is safe because the
                         // RenderMediaClient is never destructed.
                         base::Unretained(this),
                         media::SupportedVideoDecoderConfigs(),
                         media::VideoDecoderType::kUnknown),
          main_task_runner_);
      stable_video_decoder_remote->GetSupportedConfigs(
          base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                         // base::Unretained(this) is safe because the
                         // RenderMediaClient is never destructed.
                         base::Unretained(this)));
      video_decoder_for_supported_profiles_.emplace<
          mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
          std::move(stable_video_decoder_remote));
      return;
    }
    case media::OOPVDMode::kEnabledWithGpuProcessAsProxy:
    case media::OOPVDMode::kDisabled:
      break;
  }
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  mojo::SharedRemote<media::mojom::VideoDecoder> video_decoder_remote;
  interface_factory_for_supported_profiles_->CreateVideoDecoder(
      video_decoder_remote.BindNewPipeAndPassReceiver(),
      /*dst_video_decoder=*/{});
  video_decoder_remote.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnVideoDecoderDisconnected,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)),
      main_task_runner_);
  video_decoder_remote->GetSupportedConfigs(
      base::BindOnce(&RenderMediaClient::OnGetSupportedVideoDecoderConfigs,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)));
  video_decoder_for_supported_profiles_
      .emplace<mojo::SharedRemote<media::mojom::VideoDecoder>>(
          std::move(video_decoder_remote));
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)

#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  interface_factory_for_supported_profiles_->CreateAudioDecoder(
      audio_decoder_for_supported_configs_.BindNewPipeAndPassReceiver());
  audio_decoder_for_supported_configs_.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnAudioDecoderDisconnected,
                     base::Unretained(this)),
      main_task_runner_);
  audio_decoder_for_supported_configs_->GetSupportedConfigs(
      base::BindOnce(&RenderMediaClient::OnGetSupportedAudioDecoderConfigs,
                     base::Unretained(this)));
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  RenderThreadImpl::current()->BindHostReceiver(
      gpu_for_supported_profiles_.BindNewPipeAndPassReceiver());
  gpu_for_supported_profiles_.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnGpuDisconnected,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)));

  gpu_for_supported_profiles_->CreateVideoEncodeAcceleratorProvider(
      video_encoder_for_supported_profiles_.BindNewPipeAndPassReceiver());
  gpu_for_supported_profiles_.reset();
  video_encoder_for_supported_profiles_.set_disconnect_handler(
      base::BindOnce(&RenderMediaClient::OnVideoEncoderDisconnected,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)),
      main_task_runner_);
  // In case this causing too much jank on the gpu main thread at startup,
  // query the configs 1s later.
  //
  // NOTE: The side effect of this approach is that for non-main threads,
  // it is likely have to be blocked for up to a second to complete.
  main_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RenderMediaClient::GetSupportedVideoEncoderConfigs,
                     // base::Unretained(this) is safe because the
                     // RenderMediaClient is never destructed.
                     base::Unretained(this)),
      base::Milliseconds(1000));
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
}

RenderMediaClient::~RenderMediaClient() {
  // The RenderMediaClient should never get destroyed and we have usages of
  // base::Unretained(this) here that rely on that behaviour.
  NOTREACHED();
}

bool RenderMediaClient::IsDecoderSupportedAudioType(
    const media::AudioType& type) {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  if (!did_audio_decoder_update_.IsSignaled()) {
    // The asynchronous request didn't complete in time, so we must now block
    // or retrieve the information synchronously.
    if (main_task_runner_->BelongsToCurrentThread()) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
      media::SupportedAudioDecoderConfigs configs;
      if (!audio_decoder_for_supported_configs_->GetSupportedConfigs(
              &configs)) {
        configs.clear();
      }
      OnGetSupportedAudioDecoderConfigs(configs);
      DCHECK(did_audio_decoder_update_.IsSignaled());
    } else {
      // There's already an asynchronous request on the main thread, so wait...
      did_audio_decoder_update_.Wait();
    }
  }
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)

  return GetContentClient()->renderer()->IsDecoderSupportedAudioType(type);
}

bool RenderMediaClient::IsDecoderSupportedVideoType(
    const media::VideoType& type) {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  if (!did_video_decoder_update_.IsSignaled()) {
    // The asynchronous request didn't complete in time, so we must now block
    // or retrieve the information synchronously.
    if (main_task_runner_->BelongsToCurrentThread()) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
      media::SupportedVideoDecoderConfigs configs;
      media::VideoDecoderType video_decoder_type;
      if ((absl::holds_alternative<
               mojo::SharedRemote<media::mojom::VideoDecoder>>(
               video_decoder_for_supported_profiles_) &&
           !absl::get<mojo::SharedRemote<media::mojom::VideoDecoder>>(
                video_decoder_for_supported_profiles_)
                ->GetSupportedConfigs(&configs, &video_decoder_type)) ||
          (absl::holds_alternative<
               mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
               video_decoder_for_supported_profiles_) &&
           !absl::get<
                mojo::SharedRemote<media::stable::mojom::StableVideoDecoder>>(
                video_decoder_for_supported_profiles_)
                ->GetSupportedConfigs(&configs, &video_decoder_type))) {
        configs.clear();
      }
      OnGetSupportedVideoDecoderConfigs(configs, video_decoder_type);
      DCHECK(did_video_decoder_update_.IsSignaled());
    } else {
      // There's already an asynchronous request on the main thread, so wait...
      did_video_decoder_update_.Wait();
    }
  }
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)

  return GetContentClient()->renderer()->IsDecoderSupportedVideoType(type);
}

bool RenderMediaClient::IsEncoderSupportedVideoType(
    const media::VideoType& type) {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  if (!did_video_encoder_update_.IsSignaled()) {
    // The asynchronous request didn't complete in time, so we must now block
    // or retrieve the information synchronously.
    if (main_task_runner_->BelongsToCurrentThread()) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
      media::VideoEncodeAccelerator::SupportedProfiles configs;
      if (!video_encoder_for_supported_profiles_
               ->GetVideoEncodeAcceleratorSupportedProfiles(&configs)) {
        configs.clear();
      }
      OnGetSupportedVideoEncoderConfigs(configs);
      DCHECK(did_video_encoder_update_.IsSignaled());
    } else {
      // There's already an asynchronous request on the main thread, so wait...
      did_video_encoder_update_.Wait();
    }
  }
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)

  return GetContentClient()->renderer()->IsEncoderSupportedVideoType(type);
}

bool RenderMediaClient::IsSupportedBitstreamAudioCodec(
    media::AudioCodec codec) {
  return GetContentClient()->renderer()->IsSupportedBitstreamAudioCodec(codec);
}

std::optional<::media::AudioRendererAlgorithmParameters>
RenderMediaClient::GetAudioRendererAlgorithmParameters(
    media::AudioParameters audio_parameters) {
  return GetContentClient()->renderer()->GetAudioRendererAlgorithmParameters(
      audio_parameters);
}

void RenderMediaClient::GetSupportedVideoEncoderConfigs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  if (!video_encoder_for_supported_profiles_) {
    return;
  }
  video_encoder_for_supported_profiles_
      ->GetVideoEncodeAcceleratorSupportedProfiles(
          base::BindOnce(&RenderMediaClient::OnGetSupportedVideoEncoderConfigs,
                         // base::Unretained(this) is safe because the
                         // RenderMediaClient is never destructed.
                         base::Unretained(this)));
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
}

void RenderMediaClient::OnInterfaceFactoryDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  OnAudioDecoderDisconnected();
  OnVideoDecoderDisconnected();
}

void RenderMediaClient::OnGpuDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  OnVideoEncoderDisconnected();
}

void RenderMediaClient::OnAudioDecoderDisconnected() {
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  OnGetSupportedAudioDecoderConfigs(media::SupportedAudioDecoderConfigs());
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}

void RenderMediaClient::OnVideoDecoderDisconnected() {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  OnGetSupportedVideoDecoderConfigs(media::SupportedVideoDecoderConfigs(),
                                    media::VideoDecoderType::kUnknown);
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
}

void RenderMediaClient::OnVideoEncoderDisconnected() {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  OnGetSupportedVideoEncoderConfigs(
      media::VideoEncodeAccelerator::SupportedProfiles());
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
}

void RenderMediaClient::OnGetSupportedVideoDecoderConfigs(
    const media::SupportedVideoDecoderConfigs& configs,
    media::VideoDecoderType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  if (did_video_decoder_update_.IsSignaled()) {
    return;
  }

  UpdateDecoderVideoProfilesInternal(configs);
  did_video_decoder_update_.Signal();

  video_decoder_for_supported_profiles_
      .emplace<mojo::SharedRemote<media::mojom::VideoDecoder>>();
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  if (did_audio_decoder_update_.IsSignaled()) {
    interface_factory_for_supported_profiles_.reset();
  }
#else
  interface_factory_for_supported_profiles_.reset();
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
}

void RenderMediaClient::OnGetSupportedAudioDecoderConfigs(
    const media::SupportedAudioDecoderConfigs& configs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
  if (did_audio_decoder_update_.IsSignaled()) {
    return;
  }

  UpdateDecoderAudioTypesInternal(configs);
  did_audio_decoder_update_.Signal();

  audio_decoder_for_supported_configs_.reset();
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
  if (did_video_decoder_update_.IsSignaled()) {
    interface_factory_for_supported_profiles_.reset();
  }
#else
  interface_factory_for_supported_profiles_.reset();
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER)
}

void RenderMediaClient::OnGetSupportedVideoEncoderConfigs(
    const media::VideoEncodeAccelerator::SupportedProfiles& configs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  if (did_video_encoder_update_.IsSignaled()) {
    return;
  }

  UpdateEncoderVideoProfilesInternal(configs);
  did_video_encoder_update_.Signal();

  video_encoder_for_supported_profiles_.reset();
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
}

media::ExternalMemoryAllocator* RenderMediaClient::GetMediaAllocator() {
  return GetContentClient()->renderer()->GetMediaAllocator();
}

}  // namespace content
