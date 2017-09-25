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

#include "starboard/shared/win32/video_renderer.h"

#include "starboard/log.h"
#include "starboard/shared/win32/decode_target_internal.h"
#include "starboard/shared/win32/error_utils.h"
#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "third_party/angle/include/GLES2/gl2.h"

using starboard::scoped_refptr;
using Microsoft::WRL::ComPtr;
using starboard::shared::win32::CheckResult;

namespace starboard {
namespace shared {
namespace win32 {

SbDecodeTarget VideoRendererImpl::GetCurrentDecodeTarget(
    SbMediaTime media_time,
    bool audio_eos_reached) {
  VideoFramePtr current_frame =
      Base::GetCurrentFrame(media_time, audio_eos_reached);
  if (!current_frame || current_frame->IsEndOfStream()) {
    return kSbDecodeTargetInvalid;
  }
  return new SbDecodeTargetPrivate(current_frame);
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
