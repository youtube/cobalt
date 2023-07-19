// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/video_capabilities.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/extended_resources_manager.h"
#include "starboard/shared/uwp/xb1_get_type.h"
#include "starboard/shared/win32/video_decoder.h"
#include "starboard/window.h"

using ::starboard::shared::starboard::media::MimeType;

namespace {
using ::starboard::shared::uwp::ApplicationUwp;
using ::starboard::shared::uwp::ExtendedResourcesManager;

class XboxVideoCapabilities {
 public:
  XboxVideoCapabilities() {
    bool limit_to_2k = false;
    SbWindowSize window_size = ApplicationUwp::Get()->GetVisibleAreaSize();
    if (window_size.width <= 1920 || window_size.height <= 1080) {
      limit_to_2k = true;
    }

#ifdef ENABLE_H264_4K_SUPPORT
    // Documentation claims the following resolution constraints for H264
    // decoder:
    // (https://docs.microsoft.com/en-us/windows/win32/medfound/h-264-video-decoder)
    // - minimum Resolution 48*48 pixels
    // - maximum Resolution 4096*2304 pixels
    // For Windows 8 and higher the maximum guaranteed resolution for DXVA
    // acceleration is 1920*1088 pixels.
    // At higher resolutions, decoding is done with DXVA, if it is supported by
    // the underlying hardware, otherwise, decoding is done with software.
    // Therefore platforms must explicitly opt-in to support 4k H264.
    hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecH264, 4096, 2304, 60);
#else   // ENABLE_H264_4K_SUPPORT
    hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecH264, 1920, 1088, 60);
#endif  // ENABLE_H264_4K_SUPPORT

    switch (starboard::shared::uwp::GetXboxType()) {
      case starboard::shared::uwp::kXboxOneBase:
        // Horizontal video resolutions
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 2560, 1440,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 1920, 1080,
                                             60);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2560, 1440,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1920, 1080,
                                             60);
        // Vertical video resolutions
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 1440, 2560,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 1080, 1920,
                                             60);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1440, 2560,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1080, 1920,
                                             60);
        break;
      case starboard::shared::uwp::kXboxOneS:
        if (!limit_to_2k) {
          // Horizontal video resolution
          gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 3840,
                                               2160, 30);
          // Vertical video resolution
          gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 2160,
                                               3840, 30);
        }
        // Horizontal video resolutions
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 2560, 1440,
                                             60);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 2560, 1440,
                                             60);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2560, 1440,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1920, 1080,
                                             60);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 2560, 1440,
                                             30);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1920, 1080,
                                             60);
        // Vertical video resolutions
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 1440, 2560,
                                             60);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 1440, 2560,
                                             60);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1440, 2560,
                                             30);
        gpu_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1080, 1920,
                                             60);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1440, 2560,
                                             30);
        gpu_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1080, 1920,
                                             60);
        break;
      case starboard::shared::uwp::kXboxOneX:
        // Horizontal video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        // Vertical video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 2160, 3840,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 2160, 3840,
                                            60);
        break;
      case starboard::shared::uwp::kXboxSeriesS:
        // Horizontal video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        // Vertical video resolutions
        // Microsoft Vp9 MFT component is limited by 3840x2160 resolution
        // so vertical video resolutions is the same as horizontal

        // Horizontal video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 3840, 2160,
                                            30);
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 3840, 2160,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 3840, 2160,
                                            30);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1920, 1080,
                                            60);
        // Vertical video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2160, 3840,
                                            30);
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2160, 3840,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 2160, 3840,
                                            30);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1080, 1920,
                                            60);
        break;
      case starboard::shared::uwp::kXboxSeriesX:
        // Horizontal video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecVp9, 3840, 2160,
                                            60);
        // Vertical video resolutions vor vp9 is the same as horizontal

        // Horizontal video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 3840, 2160,
                                            30);
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2560, 1440,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 2560, 1440,
                                            30);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1920, 1080,
                                            60);
        // Vertical video resolutions
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 2160, 3840,
                                            30);
        hw_decoder_capabilities_.AddSdrRule(kSbMediaVideoCodecAv1, 1440, 2560,
                                            60);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1440, 2560,
                                            30);
        hw_decoder_capabilities_.AddHdrRule(kSbMediaVideoCodecAv1, 1080, 1920,
                                            60);
        break;
      default:
        // If you appeared here you likely added new XBox model name but not
        // specified corresponded case here to add rule for SDR and HDR
        // supported resolution/framerate.
        SB_NOTREACHED();
    }
  }

  bool IsSupported(SbMediaVideoCodec codec,
                   int bit_depth,
                   SbMediaPrimaryId primary_id,
                   SbMediaTransferId transfer_id,
                   SbMediaMatrixId matrix_id,
                   int width,
                   int height,
                   int fps) const {
    bool is_supported = hw_decoder_capabilities_.IsSupported(
        codec, transfer_id, width, height, fps);

    if (ExtendedResourcesManager::GetInstance()->IsGpuDecoderReady() &&
        !is_supported) {
      is_supported = gpu_decoder_capabilities_.IsSupported(codec, transfer_id,
                                                           width, height, fps);
    }

    if (starboard::shared::starboard::media::IsSDRVideo(
            bit_depth, primary_id, transfer_id, matrix_id)) {
      return is_supported;
    }

#if SB_API_VERSION < 14
    // AV1 decoder only supports YUVI420 compact texture format. The new format
    // is only supported after
    // 14.
    if (codec == kSbMediaVideoCodecAv1) {
      return false;
    }
#endif  // SB_API_VERSION >= 14

    is_supported &= ApplicationUwp::Get()->IsHdrSupported();
    is_supported &= bit_depth == 10;
    is_supported &= primary_id == kSbMediaPrimaryIdBt2020;
    is_supported &= transfer_id == kSbMediaTransferIdSmpteSt2084;
    // According to https://support.google.com/youtube/answer/7126552
    // upload requirements the color matrix should be Rec.2020
    // non-constant luminance.
    is_supported &= matrix_id == kSbMediaMatrixIdBt2020NonconstantLuminance;
    return is_supported;
  }

 private:
  // We use gpu accelerated software decoder on kXboxOneBase and kXboxOneS. The
  // software decoder initialization takes some time at app launch, and will be
  // only available after initialization. So, we use |gpu_decoder_capabilities_|
  // and ExtendedResourcesManager::IsGpuDecoderReady() to determine the device
  // capabilities with software decoder. |hw_decoder_capabilities_| is
  // determined by the device type and would not change at all.
  starboard::shared::starboard::media::VideoCapabilities
      hw_decoder_capabilities_;
  starboard::shared::starboard::media::VideoCapabilities
      gpu_decoder_capabilities_;
};

}  // namespace

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
  if (bitrate > kSbMediaMaxVideoBitrateInBitsPerSecond) {
    return false;
  }

  // To avoid massive dropping frames, only support video's fps
  // that is at most 20% higher than the refresh rate of display
  // in HDR mode.
  if (1.2 * ApplicationUwp::Get()->GetRefreshRate() < fps) {
    return false;
  }

  static const XboxVideoCapabilities xbox_video_capabilities;
  return xbox_video_capabilities.IsSupported(video_codec, bit_depth, primary_id,
                                             transfer_id, matrix_id,
                                             frame_width, frame_height, fps);
}
