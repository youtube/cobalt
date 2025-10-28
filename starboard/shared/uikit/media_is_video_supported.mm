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

#include "starboard/shared/starboard/media/media_support_internal.h"

#import <AVFoundation/AVFoundation.h>
#import <UIKit/UIKit.h>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/media/mime_type.h"

#import "starboard/shared/uikit/playback_capabilities.h"
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
#import "starboard/shared/uikit/vpx_video_decoder.h"  // nogncheck
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64

using ::starboard::shared::starboard::media::MimeType;

bool SbMediaIsVideoSupported(SbMediaVideoCodec video_codec,
                             const MimeType* mime_type,
                             int profile,
                             int level,
                             int bit_depth,
                             SbMediaPrimaryId primary_id,
                             SbMediaTransferId transfer_id,
                             SbMediaMatrixId matrix_id,
                             int frame_width,
                             int frame_height,
                             int64_t bitrate,
                             int fps,
                             bool decode_to_texture_required) {
  bool experimental_allowed = false;

  if (mime_type) {
    if (!mime_type->is_valid()) {
      return false;
    }
    // The "experimental" attribute can have three conditions:
    // 1. Not present: only returns true when hardware decoder is present.
    // 2. "allowed":   returns true if it is supported, either via a hardware
    //                 or a software decoder.
    // 3. "invalid":   always returns false.  Note that false is also returned
    //                 for other unknown values that should never be present.
    if (!mime_type->ValidateStringParameter("experimental", "allowed")) {
      return false;
    }
    const std::string& experimental_value =
        mime_type->GetParamStringValue("experimental", "");
    experimental_allowed = experimental_value == "allowed";
  }

  @autoreleasepool {
    using starboard::shared::starboard::media::IsSDRVideo;
    using starboard::shared::uikit::PlaybackCapabilities;

    if (bitrate > kSbMediaMaxVideoBitrateInBitsPerSecond) {
      return false;
    }

    if (fps > 60) {
      return false;
    }

    // Reject HFR video on SFR displays.
    if (fps > 45) {
      NSInteger refresh_rate = UIScreen.mainScreen.maximumFramesPerSecond;
      if (refresh_rate < 1) {
        refresh_rate = 60;
      }
      if (refresh_rate <= 30) {
        return false;
      }
    }

    bool is_hdr = !IsSDRVideo(bit_depth, primary_id, transfer_id, matrix_id);
    if (video_codec == kSbMediaVideoCodecH264) {
      return !is_hdr && frame_height <= 1080 && frame_width <= 1920;
    }
    if (video_codec == kSbMediaVideoCodecVp9) {
      const bool kEnableHdrWithSoftwareVp9 = false;

      if (is_hdr) {
        if (transfer_id == kSbMediaTransferIdSmpteSt2084 ||
            transfer_id == kSbMediaTransferIdAribStdB67) {
          NSInteger available_hdr_modes = AVPlayer.availableHDRModes;
          if (!(available_hdr_modes & AVPlayerHDRModeHDR10)) {
            return false;
          }
        } else {
          return false;
        }
      }

      if (PlaybackCapabilities::IsHwVp9Supported()) {
        return frame_height <= 2160 && frame_width <= 3840;
      }
#if SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
      if (experimental_allowed) {
        if (is_hdr && !kEnableHdrWithSoftwareVp9) {
          return false;
        }
        if (PlaybackCapabilities::IsAppleTV4K()) {
          // The sw vp9 decoder capability for Apple TV 4K.
          if (frame_height <= 1080 && frame_width <= 1920) {
            return true;
          }
          if (frame_height <= 1440 && frame_width <= 2560) {
            return !is_hdr || fps <= 30;
          }
          if (frame_height <= 2160 && frame_width <= 3840 && fps <= 30) {
            return true;
          }
        } else {
          // The sw vp9 decoder capability for Apple TV HD.
          if (frame_height <= 1080 && frame_width <= 1920 && fps <= 30) {
            return true;
          } else if (frame_height <= 720 && frame_width <= 1280) {
            return true;
          }
        }
        return false;
      }
#endif  // SB_IS_ARCH_ARM || SB_IS_ARCH_ARM64
    }
  }  // @autoreleasepool

  return false;
}
