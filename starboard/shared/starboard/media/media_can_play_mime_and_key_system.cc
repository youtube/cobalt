// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/media.h"

#include "starboard/character.h"
#include "starboard/log.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/string.h"

#if SB_API_VERSION < 4

using starboard::shared::starboard::media::MimeType;

namespace {
bool HasMultiChannelOutput() {
  int count = SbMediaGetAudioOutputCount();
  for (int output_index = 0; output_index < count; ++output_index) {
    SbMediaAudioConfiguration configuration;
    if (!SbMediaGetAudioConfiguration(output_index, &configuration)) {
      continue;
    }

    if (configuration.number_of_channels >= 6) {
      return true;
    }
  }

  return false;
}
}  // namespace

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  if (mime == NULL) {
    SB_DLOG(WARNING) << "mime cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }
  if (key_system == NULL) {
    SB_DLOG(WARNING) << "key_system cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }
  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                            key_system)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }
  MimeType mime_type(mime);
  if (!mime_type.is_valid()) {
    SB_DLOG(WARNING) << mime << " is not a valid mime type";
    return kSbMediaSupportTypeNotSupported;
  }

  if (mime_type.type() == "audio" && mime_type.subtype() == "mp4") {
    // TODO: Base this on the "channels" param, not the codecs param.
    if (mime_type.GetParamStringValue("codecs", "") == "aac51") {
      if (!HasMultiChannelOutput()) {
        return kSbMediaSupportTypeNotSupported;
      }
    }
    return kSbMediaSupportTypeProbably;
  }
  if (mime_type.type() == "video" && mime_type.subtype() == "mp4") {
    return kSbMediaSupportTypeProbably;
  }
#if SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  if (mime_type.type() == "video" && mime_type.subtype() == "webm") {
    if (mime_type.GetCodecs().empty()) {
      return kSbMediaSupportTypeMaybe;
    }
    if (mime_type.GetParamStringValue(0) == "vp9") {
      return kSbMediaSupportTypeProbably;
    }
    return kSbMediaSupportTypeNotSupported;
  }
#endif  // SB_HAS(MEDIA_WEBM_VP9_SUPPORT)
  return kSbMediaSupportTypeNotSupported;
}

#else  // SB_API_VERSION < 4

#include "starboard/shared/starboard/media/codec_util.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"

using starboard::shared::starboard::media::GetAudioCodecFromString;
using starboard::shared::starboard::media::GetTransferIdFromString;
using starboard::shared::starboard::media::GetVideoCodecFromString;
using starboard::shared::starboard::media::IsAudioOutputSupported;
using starboard::shared::starboard::media::MimeType;
using starboard::shared::starboard::media::ParseH264Info;

namespace {

const int64_t kDefaultBitRate = 0;
const int64_t kDefaultAudioChannels = 2;

// A progressive video contains both audio and video data.  The JS app uses
// canPlayType() like "canPlayType(video/mp4; codecs="avc1.42001E, mp4a.40.2",)"
// to query for support on both the audio and video codecs, which is being
// forwarded to here.
//
// Note that canPlayType() doesn't support extra parameters like width, height
// and channels.
SbMediaSupportType CanPlayProgressiveVideo(const MimeType& mime_type) {
  const std::vector<std::string>& codecs = mime_type.GetCodecs();

  SB_DCHECK(codecs.size() == 2) << codecs.size();

  bool has_audio_codec = false;
  bool has_video_codec = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    SbMediaAudioCodec audio_codec = GetAudioCodecFromString(codecs[i].c_str());
    if (audio_codec != kSbMediaAudioCodecNone) {
      if (!SbMediaIsAudioSupported(audio_codec, kDefaultBitRate)) {
        return kSbMediaSupportTypeNotSupported;
      }
      has_audio_codec = true;
      continue;
    }
    SbMediaVideoCodec video_codec = GetVideoCodecFromString(codecs[i].c_str());
    if (video_codec == kSbMediaVideoCodecNone) {
      return kSbMediaSupportTypeNotSupported;
    }

    has_video_codec = true;

    int width = 0;
    int height = 0;
    int fps = 0;

    if (video_codec == kSbMediaVideoCodecH264) {
      if (!ParseH264Info(codecs[i].c_str(), &width, &height, &fps)) {
        return kSbMediaSupportTypeNotSupported;
      }
    }
    if (!SbMediaIsVideoSupported(video_codec, width, height, kDefaultBitRate,
                                 fps)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  if (has_audio_codec && has_video_codec) {
    return kSbMediaSupportTypeProbably;
  }

  return kSbMediaSupportTypeNotSupported;
}

// Calls to isTypeSupported() are redirected to this function.  Following are
// some example inputs:
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; width=640)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; width=99999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; height=360)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; height=99999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; framerate=30)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; framerate=9999)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; bitrate=300000)
//   isTypeSupported(video/mp4; codecs="avc1.4d401e"; bitrate=2000000000)
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2")
//   isTypeSupported(audio/webm; codecs="vorbis")
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(video/webm; codecs="vp9")
//   isTypeSupported(audio/webm; codecs="opus")
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2"; channels=2)
//   isTypeSupported(audio/mp4; codecs="mp4a.40.2"; channels=99)
SbMediaSupportType CanPlayMimeAndKeySystem(const MimeType& mime_type,
                                           const char* key_system) {
  SB_DCHECK(mime_type.is_valid());

  const std::vector<std::string>& codecs = mime_type.GetCodecs();

  // Pre-filter for |key_system|.
  if (SbStringGetLength(key_system) != 0) {
    if (!SbMediaIsSupported(kSbMediaVideoCodecNone, kSbMediaAudioCodecNone,
                            key_system)) {
      return kSbMediaSupportTypeNotSupported;
    }
  }

  if (codecs.size() == 0) {
    // When there is no codecs listed, returns |kSbMediaSupportTypeMaybe| and
    // reject unsupported formats when query again with valid "codecs".
    return kSbMediaSupportTypeMaybe;
  }

  if (codecs.size() > 2) {
    return kSbMediaSupportTypeNotSupported;
  }

  if (codecs.size() == 2) {
    SB_DCHECK(SbStringGetLength(key_system) == 0);
    return CanPlayProgressiveVideo(mime_type);
  }

  SB_DCHECK(codecs.size() == 1);

  if (mime_type.type() == "audio") {
    SbMediaAudioCodec audio_codec = GetAudioCodecFromString(codecs[0].c_str());
    if (audio_codec == kSbMediaAudioCodecNone) {
      return kSbMediaSupportTypeNotSupported;
    }

    if (SbStringGetLength(key_system) != 0) {
      if (!SbMediaIsSupported(kSbMediaVideoCodecNone, audio_codec,
                              key_system)) {
        return kSbMediaSupportTypeNotSupported;
      }
    }

    int channels =
        mime_type.GetParamIntValue("channels", kDefaultAudioChannels);
    if (!IsAudioOutputSupported(kSbMediaAudioCodingTypePcm, channels)) {
      return kSbMediaSupportTypeNotSupported;
    }

    int bitrate = mime_type.GetParamIntValue("bitrate", kDefaultBitRate);

    if (SbMediaIsAudioSupported(audio_codec, bitrate)) {
      return kSbMediaSupportTypeProbably;
    }

    return kSbMediaSupportTypeNotSupported;
  }

  if (mime_type.type() == "video") {
    SbMediaVideoCodec video_codec = GetVideoCodecFromString(codecs[0].c_str());
    if (video_codec == kSbMediaVideoCodecNone) {
      return kSbMediaSupportTypeNotSupported;
    }

    if (SbStringGetLength(key_system) != 0) {
      if (!SbMediaIsSupported(video_codec, kSbMediaAudioCodecNone,
                              key_system)) {
        return kSbMediaSupportTypeNotSupported;
      }
    }

    std::string eotf = mime_type.GetParamStringValue("eotf", "");
    if (!eotf.empty()) {
      SbMediaTransferId transfer_id = GetTransferIdFromString(eotf);
      if (!SbMediaIsTransferCharacteristicsSupported(transfer_id)) {
        return kSbMediaSupportTypeNotSupported;
      }
    }

    int width = 0;
    int height = 0;
    int fps = 0;

    if (video_codec == kSbMediaVideoCodecH264) {
      if (!ParseH264Info(codecs[0].c_str(), &width, &height, &fps)) {
        return kSbMediaSupportTypeNotSupported;
      }
    }

    width = mime_type.GetParamIntValue("width", width);
    height = mime_type.GetParamIntValue("height", height);
    fps = mime_type.GetParamIntValue("framerate", fps);

    int bitrate = mime_type.GetParamIntValue("bitrate", kDefaultBitRate);

    if (SbMediaIsVideoSupported(video_codec, width, height, bitrate, fps)) {
      return kSbMediaSupportTypeProbably;
    }

    return kSbMediaSupportTypeNotSupported;
  }

  return kSbMediaSupportTypeNotSupported;
}

}  // namespace

SbMediaSupportType SbMediaCanPlayMimeAndKeySystem(const char* mime,
                                                  const char* key_system) {
  if (mime == NULL) {
    SB_DLOG(WARNING) << "mime cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }

  if (key_system == NULL) {
    SB_DLOG(WARNING) << "key_system cannot be NULL";
    return kSbMediaSupportTypeNotSupported;
  }

  MimeType mime_type(mime);
  if (!mime_type.is_valid()) {
    SB_DLOG(WARNING) << mime << " is not a valid mime type";
    return kSbMediaSupportTypeNotSupported;
  }

  return CanPlayMimeAndKeySystem(mime_type, key_system);
}

#endif  // SB_API_VERSION < 4
