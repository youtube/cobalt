// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_RENDERER_H_
#define STARBOARD_SHARED_WIN32_VIDEO_RENDERER_H_

#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/filter/video_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/video_renderer_impl_internal.h"
#include "starboard/shared/win32/decode_target_internal.h"
#include "starboard/shared/win32/video_decoder.h"

namespace starboard {
namespace shared {
namespace win32 {

class VideoRendererImpl
    : public ::starboard::shared::starboard::player::filter::VideoRendererImpl {
 public:
  using Base =
      ::starboard::shared::starboard::player::filter::VideoRendererImpl;
  using HostedVideoDecoder =
      ::starboard::shared::starboard::player::filter::HostedVideoDecoder;

  using VideoDecoder = ::starboard::shared::win32::VideoDecoder;

  explicit VideoRendererImpl(scoped_ptr<VideoDecoder> decoder)
      : Base(decoder.PassAs<HostedVideoDecoder>()) {}

  SbDecodeTarget GetCurrentDecodeTarget(SbMediaTime media_time,
                                        bool audio_eos_reached) SB_OVERRIDE;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_RENDERER_H_
