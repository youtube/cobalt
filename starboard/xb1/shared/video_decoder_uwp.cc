// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/xb1/shared/video_decoder_uwp.h"

#include <windows.h>

#include "starboard/shared/uwp/async_utils.h"
#include "starboard/shared/uwp/decoder_utils.h"
#include "third_party/angle/include/angle_hdr.h"

using ::starboard::shared::uwp::ApplicationUwp;
using ::starboard::shared::uwp::UpdateHdrColorMetadataToCurrentDisplay;
using ::starboard::shared::uwp::WaitForComplete;
using Windows::Graphics::Display::Core::HdmiDisplayInformation;

namespace starboard {
namespace xb1 {
namespace shared {

const int kXboxeOneXMaxOutputSamples = 5;

VideoDecoderUwp::~VideoDecoderUwp() {
  if (IsHdrSupported() && IsHdrAngleModeEnabled()) {
    SetHdrAngleModeEnabled(false);
    auto hdmi_display_info = HdmiDisplayInformation::GetForCurrentView();
    WaitForComplete(hdmi_display_info->SetDefaultDisplayModeAsync());
  }
}

// static
bool VideoDecoderUwp::IsHardwareVp9DecoderSupported() {
  return ::starboard::shared::win32::VideoDecoder::
      IsHardwareVp9DecoderSupported(ApplicationUwp::Get()->IsHdrSupported());
}

// static
bool VideoDecoderUwp::IsHardwareAv1DecoderSupported() {
  // XboxOneS/Base has av1 mft decoder, but sw av1 decoder
  // is more preferred for Xbox OneS/Base because of performance.
  // So to avoid using av1 mft in these devices we test if
  // the current XBOX has vp9 hw decoder. If it doesn't it means
  // that there is XboxOneS/Base and we don't use mft av1 decoder.
  return VideoDecoderUwp::IsHardwareVp9DecoderSupported() &&
         ::starboard::shared::win32::IsHardwareAv1DecoderSupported();
}

bool VideoDecoderUwp::TryUpdateOutputForHdrVideo(
    const VideoStreamInfo& stream_info) {
  bool is_hdr_video =
      stream_info.color_metadata.primaries == kSbMediaPrimaryIdBt2020;
  if (is_hdr_video && !ApplicationUwp::Get()->IsHdrSupported()) {
    return false;
  }
  if (is_first_input_) {
    is_first_input_ = false;
    const SbMediaColorMetadata& color_metadata = stream_info.color_metadata;
    if (is_hdr_video && IsHdrSupported()) {
      if (!IsHdrAngleModeEnabled()) {
        SetHdrAngleModeEnabled(true);
      }
      if (memcmp(&color_metadata, &current_color_metadata_,
                 sizeof(color_metadata)) != 0) {
        current_color_metadata_ = color_metadata;
        UpdateHdrColorMetadataToCurrentDisplay(color_metadata);
      }
    }
  }
  return true;
}

size_t VideoDecoderUwp::GetPrerollFrameCount() const {
  return GetMaxNumberOfCachedFrames();
}

size_t VideoDecoderUwp::GetMaxNumberOfCachedFrames() const {
  return (::starboard::shared::uwp::kXboxOneX ==
          ::starboard::shared::uwp::GetXboxType())
             ? kXboxeOneXMaxOutputSamples
             : ::starboard::shared::win32::VideoDecoder::GetPrerollFrameCount();
}

}  // namespace shared
}  // namespace xb1
}  // namespace starboard
