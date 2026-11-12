// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "starboard/common/log.h"
#include "starboard/common/size.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/feature_list.h"
#include "starboard/shared/starboard/features.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_type.h"
#include "starboard/shared/starboard/media/resolutions.h"
#import "starboard/tvos/shared/media/playback_capabilities.h"

namespace starboard {
namespace {

#if defined(COBALT_INTERNAL_BUILD)
// NOTE: Despite the generic name, this is NOT for general experiments.
// It specifically gates software VP9 decoder fallback on Apple TV (tvOS).
// See b/180431906.
constexpr MimeParam kMimeParamExperimental{"experimental"};
#endif  // defined(COBALT_INTERNAL_BUILD)

}  // namespace

bool MediaIsVideoSupported(const MimeType* mime_type,
                           const VideoSupportQueryParams& params) {
  const ParsedVideoCodec& codec_info = params.codec_info;
  const Size& frame_size = params.frame_size;

  @autoreleasepool {
    if (params.bitrate > kSbMediaMaxVideoBitrateInBitsPerSecond) {
      return false;
    }

    const int max_fps =
        features::FeatureList::IsEnabled(features::kSupport120FpsVideo) ? 120
                                                                        : 60;
    if (params.fps > max_fps) {
      return false;
    }

    // Reject HFR video on SFR displays.
    if (params.fps > 45) {
      NSInteger refresh_rate = UIScreen.mainScreen.maximumFramesPerSecond;
      if (refresh_rate < 1) {
        refresh_rate = 60;
      }
      if (refresh_rate <= 30) {
        return false;
      }
    }

    bool is_hdr = !Is8BitSDRVideo(codec_info.bit_depth, codec_info.primary_id,
                                  codec_info.transfer_id, codec_info.matrix_id);
    if (codec_info.codec == kSbMediaVideoCodecH264) {
      return !is_hdr && frame_size.FitsWithin(Resolution::k1080p);
    }

    if (is_hdr) {
      if (codec_info.transfer_id == kSbMediaTransferIdSmpteSt2084 ||
          codec_info.transfer_id == kSbMediaTransferIdAribStdB67) {
        NSInteger available_hdr_modes = AVPlayer.availableHDRModes;
        if (!(available_hdr_modes & AVPlayerHDRModeHDR10)) {
          return false;
        }
      } else {
        return false;
      }
    }

    if (codec_info.codec == kSbMediaVideoCodecVp9) {
#if defined(COBALT_INTERNAL_BUILD)
      if (PlaybackCapabilities::IsHwVp9Supported()) {
        return frame_size.FitsWithin(Resolution::k4k);
      }

      bool experimental_allowed = false;
      if (mime_type) {
        // This block checks if the "experimental" attribute is explicitly set
        // to "allowed". If the attribute is not present or has an invalid
        // value, `ValidateStringParameter` will cause an early return of
        // `false` from the function. If present and "allowed",
        // `experimental_allowed` is set to true.
        if (!mime_type->ValidateStringParameter(kMimeParamExperimental,
                                                "allowed")) {
          return false;
        }
        const std::string& experimental_value =
            mime_type->GetParamStringValue(kMimeParamExperimental, "");
        experimental_allowed = experimental_value == "allowed";
      }

      if (experimental_allowed) {
        if (is_hdr) {
          return false;
        }
        if (PlaybackCapabilities::IsAppleTV4K()) {
          // The sw vp9 decoder capability for Apple TV 4K.
          if (frame_size.FitsWithin(Resolution::k1080p)) {
            return true;
          }
          if (frame_size.FitsWithin(Resolution::k1440p)) {
            return !is_hdr || params.fps <= 30;
          }
          if (frame_size.FitsWithin(Resolution::k4k) && params.fps <= 30) {
            return true;
          }
        } else {
          // The sw vp9 decoder capability for Apple TV HD.
          if (frame_size.FitsWithin(Resolution::k1080p) && params.fps <= 30) {
            return true;
          } else if (frame_size.FitsWithin(Resolution::k720p)) {
            return true;
          }
        }
        return false;
      }
#else
      SB_LOG(INFO) << "Non-internal build, accepting all VP9";
      return true;
#endif  // defined(COBALT_INTERNAL_BUILD)
    }

    if (codec_info.codec == kSbMediaVideoCodecAv1) {
#if defined(COBALT_INTERNAL_BUILD)
      if (PlaybackCapabilities::IsHwAv1Supported()) {
        // Cap resolution to 4k for AV1 implementation verification.
        return frame_size.FitsWithin(Resolution::k4k);
      }
#endif  // defined(COBALT_INTERNAL_BUILD)
    }
  }  // @autoreleasepool

  return false;
}

}  // namespace starboard
