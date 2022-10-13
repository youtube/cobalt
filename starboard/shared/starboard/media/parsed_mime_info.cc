// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/parsed_mime_info.h"

#include <cmath>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/media.h"
#include "starboard/shared/starboard/media/codec_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

const int64_t kDefaultAudioChannels = 2;

// Turns |eotf| into value of SbMediaTransferId.  If |eotf| isn't recognized the
// function returns kSbMediaTransferIdUnknown.
// This function supports all eotfs required by YouTube TV HTML5 Technical
// Requirements.
SbMediaTransferId GetTransferIdFromString(const std::string& transfer_id) {
  if (transfer_id == "bt709") {
    return kSbMediaTransferIdBt709;
  } else if (transfer_id == "smpte2084") {
    return kSbMediaTransferIdSmpteSt2084;
  } else if (transfer_id == "arib-std-b67") {
    return kSbMediaTransferIdAribStdB67;
  }
  return kSbMediaTransferIdUnknown;
}

}  // namespace

ParsedMimeInfo::ParsedMimeInfo(const std::string& mime_string)
    : mime_type_(mime_string) {
  ParseMimeInfo();
}

void ParsedMimeInfo::SetBitrate(int bitrate) {
  audio_info_.bitrate = bitrate;
  video_info_.bitrate = bitrate;
}

void ParsedMimeInfo::ParseMimeInfo() {
  if (!mime_type_.is_valid()) {
    is_valid_ = false;
    return;
  }

  // Read "disablecache".
  if (!mime_type_.ValidateBoolParameter("disablecache")) {
    is_valid_ = false;
    return;
  }
  disable_cache_ = mime_type_.GetParamBoolValue("disablecache", false);

  // We only support audio or video type.
  if (mime_type_.type() != "audio" && mime_type_.type() != "video") {
    is_valid_ = false;
    return;
  }

  auto codecs = mime_type_.GetCodecs();
  // We only support up to one audio codec and one video codec.
  if (codecs.size() > 2) {
    is_valid_ = false;
    return;
  }

  for (const auto& codec : codecs) {
    if (!has_audio_info() && ParseAudioInfo(codec)) {
      continue;
    }
    if (!has_video_info() && ParseVideoInfo(codec)) {
      continue;
    }
    // It either has an invalid codec or has two codecs of same type.
    ResetCodecInfos();
    is_valid_ = false;
    return;
  }
}

bool ParsedMimeInfo::ParseAudioInfo(const std::string& codec) {
  SB_DCHECK(mime_type_.is_valid());
  SB_DCHECK(!has_audio_info());

  SbMediaAudioCodec audio_codec = GetAudioCodecFromString(codec.c_str());
  if (audio_codec == kSbMediaAudioCodecNone) {
    return false;
  }
  if (!mime_type_.ValidateIntParameter("channels") ||
      !mime_type_.ValidateIntParameter("bitrate")) {
    return false;
  }
  audio_info_.codec = audio_codec;
  audio_info_.channels =
      mime_type_.GetParamIntValue("channels", kDefaultAudioChannels);
  audio_info_.bitrate = mime_type_.GetParamIntValue("bitrate", 0);

  return audio_info_.channels >= 0 && audio_info_.bitrate >= 0;
}

bool ParsedMimeInfo::ParseVideoInfo(const std::string& codec) {
  SB_DCHECK(mime_type_.is_valid());
  SB_DCHECK(!has_video_info());

  if (!ParseVideoCodec(codec.c_str(), &video_info_.codec, &video_info_.profile,
                       &video_info_.level, &video_info_.bit_depth,
                       &video_info_.primary_id, &video_info_.transfer_id,
                       &video_info_.matrix_id)) {
    return false;
  }

  if (video_info_.codec == kSbMediaVideoCodecNone) {
    return false;
  }

  std::string eotf = mime_type_.GetParamStringValue("eotf", "");
  if (!eotf.empty()) {
    SbMediaTransferId transfer_id_from_eotf = GetTransferIdFromString(eotf);
    if (transfer_id_from_eotf == kSbMediaTransferIdUnknown) {
      // The eotf is an unknown value, mark the codec info as invalid.
      SB_LOG(WARNING) << "Unknown eotf " << eotf << ".";
      return false;
    }
    SB_LOG_IF(WARNING,
              video_info_.transfer_id != kSbMediaTransferIdUnspecified &&
                  video_info_.transfer_id != transfer_id_from_eotf)
        << "transfer_id " << video_info_.transfer_id
        << " set by the codec string \"" << video_info_.codec
        << "\" will be overwritten by the eotf attribute " << eotf;
    video_info_.transfer_id = transfer_id_from_eotf;
  }

  if (!mime_type_.ValidateIntParameter("width") ||
      !mime_type_.ValidateIntParameter("height") ||
      !mime_type_.ValidateFloatParameter("framerate") ||
      !mime_type_.ValidateIntParameter("bitrate") ||
      !mime_type_.ValidateBoolParameter("decode-to-texture")) {
    return false;
  }

  video_info_.frame_width = mime_type_.GetParamIntValue("width", 0);
  video_info_.frame_height = mime_type_.GetParamIntValue("height", 0);
  // TODO: Support float framerate. Our starboard implementation only supports
  // integer framerate, but framerate could be float and we should support it.
  float framerate = mime_type_.GetParamFloatValue("framerate", 0.0f);
  video_info_.fps = std::round(framerate);
  video_info_.bitrate = mime_type_.GetParamIntValue("bitrate", 0);
  video_info_.decode_to_texture_required =
      mime_type_.GetParamBoolValue("decode-to-texture", false);

  return video_info_.frame_width >= 0 && video_info_.frame_height >= 0 &&
         video_info_.fps >= 0 && video_info_.bitrate >= 0;
}

void ParsedMimeInfo::ResetCodecInfos() {
  audio_info_.codec = kSbMediaAudioCodecNone;
  video_info_.codec = kSbMediaVideoCodecNone;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
