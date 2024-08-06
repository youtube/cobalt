// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_CHROME_MEDIA_H_
#define COBALT_MEDIA_BASE_CHROME_MEDIA_H_

#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/media_log.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/audio_decoder_config.h"
#include "third_party/chromium/media/base/decoder_buffer.h"
#include "third_party/chromium/media/base/demuxer.h"
#include "third_party/chromium/media/base/demuxer_stream.h"
#include "third_party/chromium/media/base/eme_constants.h"
#include "third_party/chromium/media/base/media_log.h"
#include "third_party/chromium/media/base/pipeline_status.h"
#include "third_party/chromium/media/base/ranges.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/base/video_decoder_config.h"
#include "third_party/chromium/media/filters/chunk_demuxer.h"

// "//media" is from Chrome M114, make the namespace explicit.
namespace media_m114 = ::media;

namespace cobalt {
namespace media {

// Abstract common type names between Chrome Media M96 and M114 from Cobalt
// Media. Cobalt Media may refer to types in this class instead of the ones in
// specific namespaces.
//
// For example, ChromeMedia::DecoderBuffer refers to the DecoderBuffer
// implementation in the particular Chrome Media version, it will be mapped to
// ::media_96::DecoderBuffer under M96, and ::media_114::DecoderBuffer under
// M114.
//
// Two specializations ChromeMediaM96 and ChromeMediaM114 are defined later in
// this file.
template <typename AudioCodecType, typename AudioDecoderConfigType,
          typename ChunkDemuxerType, typename DecoderBufferType,
          typename DemuxerHostType, typename DemuxerStreamType,
          typename DemuxerType, typename EmeInitDataTypeType,
          typename MediaLogType, typename PipelineStatusCBType,
          typename PipelineStatusCodesType, typename PipelineStatusType,
          typename RangesType, typename VideoCodecType,
          typename VideoDecoderConfigType>
struct ChromeMedia {
  typedef AudioCodecType AudioCodec;
  typedef AudioDecoderConfigType AudioDecoderConfig;
  typedef ChunkDemuxerType ChunkDemuxer;
  typedef DecoderBufferType DecoderBuffer;
  typedef DemuxerHostType DemuxerHost;
  typedef DemuxerStreamType DemuxerStream;
  typedef DemuxerType Demuxer;
  typedef EmeInitDataTypeType EmeInitDataType;
  typedef MediaLogType MediaLog;
  typedef PipelineStatusCBType PipelineStatusCB;
  typedef PipelineStatusCodesType PipelineStatusCodes;
  typedef PipelineStatusType PipelineStatus;
  typedef RangesType Ranges;
  typedef VideoCodecType VideoCodec;
  typedef VideoDecoderConfigType VideoDecoderConfig;
};

#define TYPEDEF_CHROME_MEDIA_RELEASE(media_namespace, typename,              \
                                     PipelineStatusCodesType)                \
  typedef ChromeMedia<                                                       \
      media_namespace::AudioCodec, media_namespace::AudioDecoderConfig,      \
      media_namespace::ChunkDemuxer, media_namespace::DecoderBuffer,         \
      media_namespace::DemuxerHost, media_namespace::DemuxerStream,          \
      media_namespace::Demuxer, media_namespace::EmeInitDataType,            \
      media_namespace::MediaLog, media_namespace::PipelineStatusCB,          \
      media_namespace::PipelineStatusCodesType,                              \
      media_namespace::PipelineStatus,                                       \
      media_namespace::Ranges<base::TimeDelta>, media_namespace::VideoCodec, \
      media_namespace::VideoDecoderConfig> typename

TYPEDEF_CHROME_MEDIA_RELEASE(::media_m96, ChromeMediaM96, PipelineStatus);
TYPEDEF_CHROME_MEDIA_RELEASE(::media_m114, ChromeMediaM114,
                             PipelineStatusCodes);

#undef TYPEDEF_CHROME_MEDIA_RELEASE

// Introduce all types of the specific ChromeMedia specialization into the local
// scope, whether it's a namespace or a class, to make them readily accessible
// without extra specifier.
#define TYPEDEF_CHROME_MEDIA_TYPES(ChromeMedia)                          \
  typedef typename ChromeMedia::AudioCodec AudioCodec;                   \
  typedef typename ChromeMedia::AudioDecoderConfig AudioDecoderConfig;   \
  typedef typename ChromeMedia::ChunkDemuxer ChunkDemuxer;               \
  typedef typename ChromeMedia::DecoderBuffer DecoderBuffer;             \
  typedef typename ChromeMedia::DemuxerHost DemuxerHost;                 \
  typedef typename ChromeMedia::DemuxerStream DemuxerStream;             \
  typedef typename ChromeMedia::Demuxer Demuxer;                         \
  typedef typename ChromeMedia::EmeInitDataType EmeInitDataType;         \
  typedef typename ChromeMedia::MediaLog MediaLog;                       \
  typedef typename ChromeMedia::PipelineStatusCB PipelineStatusCB;       \
  typedef typename ChromeMedia::PipelineStatusCodes PipelineStatusCodes; \
  typedef typename ChromeMedia::PipelineStatus PipelineStatus;           \
  typedef typename ChromeMedia::Ranges Ranges;                           \
  typedef typename ChromeMedia::VideoDecoderConfig VideoCodec;           \
  typedef typename ChromeMedia::VideoDecoderConfig VideoDecoderConfig

// PipelineStatus is an enum in M96, and a class in M114.  The enum for
// individual error codes is named PipelineStatusCodes in M114.
// The following functions allows conversion and comparison between
// PipelineStatus values and enums between both versions.
// The conversion between specific error codes are implemented by casting
// the types directly, as the value of the same error stays the same between
// Chrome Media releases, and the error codes in M114 is a superset of the codes
// in M96.
template <typename DestinationPipelineStatus, typename SourcePipelineStatus>
DestinationPipelineStatus pipeline_status_cast(SourcePipelineStatus);

template <>
inline ::media_m114::PipelineStatus pipeline_status_cast<
    ::media_m114::PipelineStatus, ::media_m114::PipelineStatus>(
    ::media_m114::PipelineStatus pipeline_status) {
  return pipeline_status;
}

template <>
inline ::media_m114::PipelineStatus pipeline_status_cast<
    ::media_m114::PipelineStatus, ::media_m114::PipelineStatusCodes>(
    ::media_m114::PipelineStatusCodes pipeline_status) {
  return pipeline_status;
}

template <>
inline ::media_m114::PipelineStatus
pipeline_status_cast<::media_m114::PipelineStatus, ::media_m96::PipelineStatus>(
    ::media_m96::PipelineStatus pipeline_status) {
  return static_cast<::media_m114::PipelineStatusCodes>(pipeline_status);
}

template <>
inline ::media_m114::PipelineStatusCodes pipeline_status_cast<
    ::media_m114::PipelineStatusCodes, ::media_m114::PipelineStatus>(
    ::media_m114::PipelineStatus pipeline_status) {
  return pipeline_status.code();
}

template <>
inline ::media_m96::PipelineStatus pipeline_status_cast<
    ::media_m96::PipelineStatus, ::media_m114::PipelineStatusCodes>(
    ::media_m114::PipelineStatusCodes pipeline_status) {
  return static_cast<::media_m96::PipelineStatus>(pipeline_status);
}

template <>
inline ::media_m114::PipelineStatusCodes pipeline_status_cast<
    ::media_m114::PipelineStatusCodes, ::media_m96::PipelineStatus>(
    ::media_m96::PipelineStatus pipeline_status) {
  return static_cast<::media_m114::PipelineStatusCodes>(pipeline_status);
}

inline bool operator==(::media_m114::PipelineStatusCodes left,
                       ::media_m96::PipelineStatus right) {
  return static_cast<int>(left) == static_cast<int>(right);
}

inline bool operator==(::media_m96::PipelineStatus right,
                       ::media_m114::PipelineStatusCodes left) {
  return static_cast<int>(left) == static_cast<int>(right);
}

inline bool operator!=(::media_m114::PipelineStatusCodes left,
                       ::media_m96::PipelineStatus right) {
  return static_cast<int>(left) != static_cast<int>(right);
}

inline bool operator!=(::media_m96::PipelineStatus right,
                       ::media_m114::PipelineStatusCodes left) {
  return static_cast<int>(left) != static_cast<int>(right);
}

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_CHROME_MEDIA_H_
