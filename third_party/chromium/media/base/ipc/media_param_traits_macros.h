// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/audio_parameters.h"
#include "third_party/chromium/media/base/buffering_state.h"
#include "third_party/chromium/media/base/cdm_config.h"
#include "third_party/chromium/media/base/cdm_key_information.h"
#include "third_party/chromium/media/base/cdm_promise.h"
#include "third_party/chromium/media/base/channel_layout.h"
#include "third_party/chromium/media/base/container_names.h"
#include "third_party/chromium/media/base/content_decryption_module.h"
#include "third_party/chromium/media/base/decode_status.h"
#include "third_party/chromium/media/base/decoder.h"
#include "third_party/chromium/media/base/decrypt_config.h"
#include "third_party/chromium/media/base/decryptor.h"
#include "third_party/chromium/media/base/demuxer_stream.h"
#include "third_party/chromium/media/base/eme_constants.h"
#include "third_party/chromium/media/base/encryption_scheme.h"
#include "third_party/chromium/media/base/media_content_type.h"
#include "third_party/chromium/media/base/media_log_record.h"
#include "third_party/chromium/media/base/media_status.h"
#include "third_party/chromium/media/base/output_device_info.h"
#include "third_party/chromium/media/base/overlay_info.h"
#include "third_party/chromium/media/base/pipeline_status.h"
#include "third_party/chromium/media/base/sample_format.h"
#include "third_party/chromium/media/base/status_codes.h"
#include "third_party/chromium/media/base/subsample_entry.h"
#include "third_party/chromium/media/base/supported_video_decoder_config.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/base/video_color_space.h"
#include "third_party/chromium/media/base/video_transformation.h"
#include "third_party/chromium/media/base/video_types.h"
#include "third_party/chromium/media/base/waiting.h"
#include "third_party/chromium/media/base/watch_time_keys.h"
#include "third_party/chromium/media/media_buildflags.h"
#include "third_party/blink/public/platform/web_fullscreen_video_status.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/ipc/color/gfx_param_traits_macros.h"

#if BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)
#include "third_party/chromium/media/base/media_drm_key_type.h"
#endif  // BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)

// Enum traits.

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebFullscreenVideoStatus,
                          blink::WebFullscreenVideoStatus::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::AudioCodec, media_m96::AudioCodec::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(media_m96::AudioCodecProfile,
                          media_m96::AudioCodecProfile::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::AudioLatency::LatencyType,
                          media_m96::AudioLatency::LATENCY_COUNT)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::AudioParameters::Format,
                          media_m96::AudioParameters::AUDIO_FORMAT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::BufferingState,
                          media_m96::BufferingState::BUFFERING_STATE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(
    media_m96::BufferingStateChangeReason,
    media_m96::BufferingStateChangeReason::BUFFERING_STATE_CHANGE_REASON_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::CdmMessageType,
                          media_m96::CdmMessageType::MESSAGE_TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::CdmPromise::Exception,
                          media_m96::CdmPromise::Exception::EXCEPTION_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::CdmSessionType,
                          media_m96::CdmSessionType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::ChannelLayout, media_m96::CHANNEL_LAYOUT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::Decryptor::Status,
                          media_m96::Decryptor::Status::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::Decryptor::StreamType,
                          media_m96::Decryptor::StreamType::kStreamTypeMax)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::DemuxerStream::Status,
                          media_m96::DemuxerStream::kStatusMax)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::DemuxerStream::Type,
                          media_m96::DemuxerStream::TYPE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::EmeInitDataType, media_m96::EmeInitDataType::MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::EncryptionScheme,
                          media_m96::EncryptionScheme::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::HdcpVersion,
                          media_m96::HdcpVersion::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::MediaContentType, media_m96::MediaContentType::Max)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::MediaLogRecord::Type,
                          media_m96::MediaLogRecord::Type::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::MediaStatus::State,
                          media_m96::MediaStatus::State::STATE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::OutputDeviceStatus,
                          media_m96::OUTPUT_DEVICE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::PipelineStatus,
                          media_m96::PipelineStatus::PIPELINE_STATUS_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::SampleFormat, media_m96::kSampleFormatMax)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::VideoCodec, media_m96::VideoCodec::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::WaitingReason, media_m96::WaitingReason::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::WatchTimeKey,
                          media_m96::WatchTimeKey::kWatchTimeKeyMax)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(media_m96::VideoCodecProfile,
                              media_m96::VIDEO_CODEC_PROFILE_MIN,
                              media_m96::VIDEO_CODEC_PROFILE_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::VideoDecoderType,
                          media_m96::VideoDecoderType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::AudioDecoderType,
                          media_m96::AudioDecoderType::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::VideoPixelFormat, media_m96::PIXEL_FORMAT_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::VideoRotation, media_m96::VIDEO_ROTATION_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::container_names::MediaContainerName,
                          media_m96::container_names::CONTAINER_MAX)

IPC_ENUM_TRAITS_MAX_VALUE(media_m96::StatusCode, media_m96::StatusCode::kMaxValue)

#if BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(media_m96::MediaDrmKeyType,
                              media_m96::MediaDrmKeyType::MIN,
                              media_m96::MediaDrmKeyType::MAX)
#endif  // BUILDFLAG(ENABLE_MEDIA_DRM_STORAGE)

IPC_ENUM_TRAITS_VALIDATE(
    media_m96::VideoColorSpace::PrimaryID,
    static_cast<int>(value) ==
        static_cast<int>(
            media_m96::VideoColorSpace::GetPrimaryID(static_cast<int>(value))))

IPC_ENUM_TRAITS_VALIDATE(
    media_m96::VideoColorSpace::TransferID,
    static_cast<int>(value) ==
        static_cast<int>(
            media_m96::VideoColorSpace::GetTransferID(static_cast<int>(value))))

IPC_ENUM_TRAITS_VALIDATE(
    media_m96::VideoColorSpace::MatrixID,
    static_cast<int>(value) ==
        static_cast<int>(
            media_m96::VideoColorSpace::GetMatrixID(static_cast<int>(value))))

// Struct traits.

IPC_STRUCT_TRAITS_BEGIN(media_m96::CdmConfig)
  IPC_STRUCT_TRAITS_MEMBER(allow_distinctive_identifier)
  IPC_STRUCT_TRAITS_MEMBER(allow_persistent_state)
  IPC_STRUCT_TRAITS_MEMBER(use_hw_secure_codecs)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media_m96::MediaLogRecord)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(params)
  IPC_STRUCT_TRAITS_MEMBER(time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media_m96::SubsampleEntry)
  IPC_STRUCT_TRAITS_MEMBER(clear_bytes)
  IPC_STRUCT_TRAITS_MEMBER(cypher_bytes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media_m96::VideoColorSpace)
  IPC_STRUCT_TRAITS_MEMBER(primaries)
  IPC_STRUCT_TRAITS_MEMBER(transfer)
  IPC_STRUCT_TRAITS_MEMBER(matrix)
  IPC_STRUCT_TRAITS_MEMBER(range)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::ColorVolumeMetadata)
  IPC_STRUCT_TRAITS_MEMBER(primary_r)
  IPC_STRUCT_TRAITS_MEMBER(primary_g)
  IPC_STRUCT_TRAITS_MEMBER(primary_b)
  IPC_STRUCT_TRAITS_MEMBER(white_point)
  IPC_STRUCT_TRAITS_MEMBER(luminance_max)
  IPC_STRUCT_TRAITS_MEMBER(luminance_min)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::HDRMetadata)
  IPC_STRUCT_TRAITS_MEMBER(color_volume_metadata)
  IPC_STRUCT_TRAITS_MEMBER(max_content_light_level)
  IPC_STRUCT_TRAITS_MEMBER(max_frame_average_light_level)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media_m96::OverlayInfo)
  IPC_STRUCT_TRAITS_MEMBER(routing_token)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen)
  IPC_STRUCT_TRAITS_MEMBER(is_persistent_video)
IPC_STRUCT_TRAITS_END()

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_IPC_MEDIA_PARAM_TRAITS_MACROS_H_
