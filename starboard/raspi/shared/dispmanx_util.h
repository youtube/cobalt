// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_RASPI_SHARED_DISPMANX_UTIL_H_
#define STARBOARD_RASPI_SHARED_DISPMANX_UTIL_H_

#include <bcm_host.h>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/video_frame_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace raspi {
namespace shared {

class DispmanxRect : public VC_RECT_T {
 public:
  DispmanxRect() { vc_dispmanx_rect_set(this, 0, 0, 0, 0); }
  DispmanxRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    vc_dispmanx_rect_set(this, x, y, width, height);
  }
};

class DispmanxDisplay {
 public:
  DispmanxDisplay() {
    bcm_host_init();
    handle_ = vc_dispmanx_display_open(0);
    SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
  }
  ~DispmanxDisplay() {
    int result = vc_dispmanx_display_close(handle_);
    SB_DCHECK(result == 0);
    bcm_host_deinit();
  }

  DISPMANX_DISPLAY_HANDLE_T handle() const { return handle_; }

 private:
  DISPMANX_DISPLAY_HANDLE_T handle_;

  SB_DISALLOW_COPY_AND_ASSIGN(DispmanxDisplay);
};

class DispmanxResource {
 public:
  DispmanxResource() : handle_(DISPMANX_NO_HANDLE) {}

  virtual ~DispmanxResource() {
    if (handle_ != DISPMANX_NO_HANDLE) {
      int32_t result = vc_dispmanx_resource_delete(handle_);
      SB_DCHECK(result == 0) << " result=" << result;
    }
  }

  DISPMANX_RESOURCE_HANDLE_T handle() const { return handle_; }
  uint32_t width() const { return width_; }
  uint32_t height() const { return height_; }

 protected:
  DispmanxResource(VC_IMAGE_TYPE_T image_type, uint32_t width, uint32_t height)
      : width_(width), height_(height) {
    uint32_t vc_image_ptr;

    handle_ =
        vc_dispmanx_resource_create(image_type, width, height, &vc_image_ptr);
    SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
  }

  DISPMANX_RESOURCE_HANDLE_T handle_;
  uint32_t width_;
  uint32_t height_;

  SB_DISALLOW_COPY_AND_ASSIGN(DispmanxResource);
};

class DispmanxYUV420Resource : public DispmanxResource {
 public:
  DispmanxYUV420Resource(uint32_t width, uint32_t height)
      : DispmanxResource(VC_IMAGE_YUV420, width, height) {}

  void WriteData(const void* data) {
    SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
    DispmanxRect dst_rect(0, 0, width(), height() * 3 / 2);
    int32_t result = vc_dispmanx_resource_write_data(
        handle_, VC_IMAGE_YUV420, width(), const_cast<void*>(data), &dst_rect);
    SB_DCHECK(result == 0);
  }
  void ClearWithBlack();
};

class DispmanxElement {
 public:
  DispmanxElement(const DispmanxDisplay& display,
                  int32_t layer,
                  const DispmanxRect& dest_rect,
                  const DispmanxResource& src,
                  const DispmanxRect& src_rect);
  ~DispmanxElement();

  DISPMANX_ELEMENT_HANDLE_T handle() const { return handle_; }

 private:
  DISPMANX_ELEMENT_HANDLE_T handle_;

  SB_DISALLOW_COPY_AND_ASSIGN(DispmanxElement);
};

class DispmanxVideoRenderer {
 public:
  typedef starboard::shared::starboard::player::VideoFrame VideoFrame;
  DispmanxVideoRenderer(const DispmanxDisplay& display, int32_t layer)
      : display_(display), layer_(layer) {}

  void Update(const VideoFrame& video_frame);

 private:
  const DispmanxDisplay& display_;
  int32_t layer_;
  scoped_ptr<DispmanxYUV420Resource> resource_;
  scoped_ptr<DispmanxElement> element_;

  SB_DISALLOW_COPY_AND_ASSIGN(DispmanxVideoRenderer);
};

}  // namespace shared
}  // namespace raspi
}  // namespace starboard

#endif  // STARBOARD_RASPI_SHARED_DISPMANX_UTIL_H_
