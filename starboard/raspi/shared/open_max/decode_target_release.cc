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

#include "starboard/decode_target.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "starboard/common/log.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"

void SbDecodeTargetRelease(SbDecodeTarget target) {
  if (SbDecodeTargetIsValid(target)) {
    SB_DCHECK(target->info.format == kSbDecodeTargetFormat1PlaneRGBA);
    // This may need to change if we support more than just
    // kSbDecodeTargetFormat1PlaneRGBA.
    const int kNumPlanes = 1;

    for (int plane = 0; plane < kNumPlanes; ++plane) {
      if (target->images[plane] != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(target->display, target->images[plane]);
        SB_DCHECK(eglGetError() == EGL_SUCCESS);

        glDeleteTextures(1, &target->info.planes[0].texture);
        SB_DCHECK(glGetError() == GL_NO_ERROR);
      }
    }
    delete target;
  } else {
    SB_NOTREACHED() << "Invalid target";
  }
}
