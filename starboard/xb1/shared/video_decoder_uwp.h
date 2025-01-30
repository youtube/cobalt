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

#ifndef STARBOARD_XB1_SHARED_VIDEO_DECODER_UWP_H_
#define STARBOARD_XB1_SHARED_VIDEO_DECODER_UWP_H_

#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/uwp/application_uwp.h"
#include "starboard/shared/uwp/xb1_get_type.h"
#include "starboard/shared/win32/video_decoder.h"

namespace starboard {
namespace xb1 {
namespace shared {

class VideoDecoderUwp : public ::starboard::shared::win32::VideoDecoder {
 public:
  typedef ::starboard::shared::starboard::media::VideoStreamInfo
      VideoStreamInfo;

  VideoDecoderUwp(
      SbMediaVideoCodec video_codec,
      SbPlayerOutputMode output_mode,
      SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
      SbDrmSystem drm_system);

  ~VideoDecoderUwp() override;

  static bool IsHardwareVp9DecoderSupported();
  static bool IsHardwareAv1DecoderSupported();

  bool TryUpdateOutputForHdrVideo(const VideoStreamInfo& stream_info) override;

  size_t GetPrerollFrameCount() const override;
  size_t GetMaxNumberOfCachedFrames() const override;

 private:
  void OnDraw(bool fullscreen);

  SbMediaColorMetadata current_color_metadata_ = {};
  Mutex metadata_mutex_;
  bool metadata_available_ = false;
  bool metadata_is_set_ = false;
  bool is_first_input_ = true;
};

}  // namespace shared
}  // namespace xb1
}  // namespace starboard

#endif  // STARBOARD_XB1_SHARED_VIDEO_DECODER_UWP_H_
