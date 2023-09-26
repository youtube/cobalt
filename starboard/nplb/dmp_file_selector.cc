// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/dmp_file_selector.h"

#include "starboard/shared/starboard/player/video_dmp_reader.h"

namespace starboard {
namespace nplb {

using shared::starboard::player::video_dmp::VideoDmpReader;

DmpFileSelector::DmpFileSelector() {
  if (SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"av01.0.16M.08\"; width=7680; height=4320", "") ==
      kSbMediaSupportTypeProbably) {
    supports_av1_ = true;
    type_ = kDeviceType8k;
    max_frame_width_ = 7680;
  } else if (SbMediaCanPlayMimeAndKeySystem(
                 "video/webm; codecs=\"vp9\"; width=3840; height=2160", "") ==
             kSbMediaSupportTypeProbably) {
    supports_vp9_ = true;
    type_ = kDeviceType4k;
    max_frame_width_ = 3840;
  } else {
    type_ = kDeviceTypeFHD;
    max_frame_width_ = 1920;
  }

  if (!supports_av1_) {
    supports_av1_ =
        (SbMediaCanPlayMimeAndKeySystem(
             "video/mp4; codecs=\"av01.0.08M.08\"; width=1920; height=1080",
             "") == kSbMediaSupportTypeProbably);
  }

  if (!supports_vp9_) {
    supports_vp9_ = (SbMediaCanPlayMimeAndKeySystem(
                         "video/webm; codecs=\"vp9\"; width=1920; height=1080",
                         "") == kSbMediaSupportTypeProbably);
  }
}

std::vector<const char*> DmpFileSelector::GetSupportedVideoDmpFiles(
    std::vector<const char*> video_files,
    const char* key_system) {
  // Select the highest quality supported file from each codec.
  if (supported_video_files_.empty()) {
    for (auto video_filename : video_files) {
      VideoDmpReader dmp_reader(video_filename,
                                VideoDmpReader::kEnableReadOnDemand);
      SB_DCHECK(dmp_reader.number_of_video_buffers() > 0);
      SbMediaVideoCodec codec = dmp_reader.video_codec();
      if (IsCodecSupported(codec) && !CheckCodecSatisfied(codec) &&
          dmp_reader.video_width() <= max_frame_width_) {
        if (SbMediaCanPlayMimeAndKeySystem(dmp_reader.video_mime_type().c_str(),
                                           key_system)) {
          supported_video_files_.push_back(video_filename);
          SetCodecSatisfied(codec);
        }
      }
    }
  }

  return supported_video_files_;
}

bool DmpFileSelector::IsCodecSupported(SbMediaVideoCodec codec) const {
  switch (codec) {
    case kSbMediaVideoCodecAv1:
      return supports_av1_;
    case kSbMediaVideoCodecVp9:
      return supports_vp9_;
    case kSbMediaVideoCodecH264:
      return true;
    default:
      SB_NOTREACHED();
      return false;
  }
}

bool DmpFileSelector::CheckCodecSatisfied(SbMediaVideoCodec codec) const {
  switch (codec) {
    case kSbMediaVideoCodecAv1:
      return av1_satisfied_;
    case kSbMediaVideoCodecVp9:
      return vp9_satisfied_;
    case kSbMediaVideoCodecH264:
      return avc_satisfied_;
    default:
      SB_NOTREACHED();
      return false;
  }
}

void DmpFileSelector::SetCodecSatisfied(SbMediaVideoCodec codec) {
  switch (codec) {
    case kSbMediaVideoCodecAv1:
      av1_satisfied_ = true;
      break;
    case kSbMediaVideoCodecVp9:
      vp9_satisfied_ = true;
      break;
    case kSbMediaVideoCodecH264:
      avc_satisfied_ = true;
      break;
    default:
      SB_NOTREACHED();
  }
  return;
}

}  // namespace nplb
}  // namespace starboard
