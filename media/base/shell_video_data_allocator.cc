/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/base/shell_video_data_allocator.h"

#include "base/logging.h"

namespace media {

ShellVideoDataAllocator::YV12Param::YV12Param(int decoded_width,
                                              int decoded_height,
                                              const gfx::Rect& visible_rect)
    : decoded_width_(decoded_width),
      decoded_height_(decoded_height),
      visible_rect_(visible_rect),
      y_pitch_(0),
      uv_pitch_(0),
      y_data_(NULL),
      u_data_(NULL),
      v_data_(NULL) {}

ShellVideoDataAllocator::YV12Param::YV12Param(int width,
                                              int height,
                                              int y_pitch,
                                              int uv_pitch,
                                              uint8* y_data,
                                              uint8* u_data,
                                              uint8* v_data)
    : decoded_width_(width),
      decoded_height_(height),
      visible_rect_(0, 0, width, height),
      y_pitch_(y_pitch),
      uv_pitch_(uv_pitch),
      y_data_(y_data),
      u_data_(u_data),
      v_data_(v_data) {
  DCHECK_NE(y_pitch_, 0);
  DCHECK_NE(uv_pitch_, 0);
  DCHECK(y_data_);
  DCHECK(u_data);
  DCHECK(v_data);
}

ShellVideoDataAllocator::NV12Param::NV12Param(int width,
                                              int height,
                                              int y_pitch,
                                              const gfx::Rect& visible_rect)
    : decoded_width_(width),
      decoded_height_(height),
      y_pitch_(y_pitch),
      visible_rect_(visible_rect) {}

}  // namespace media
