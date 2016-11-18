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

#include "starboard/raspi/shared/dispmanx_util.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/memory.h"

namespace starboard {
namespace raspi {
namespace shared {

namespace {

class DispmanxAutoUpdate {
 public:
  DispmanxAutoUpdate() {
    handle_ = vc_dispmanx_update_start(0 /*screen*/);
    SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
  }
  ~DispmanxAutoUpdate() {
    if (handle_ != DISPMANX_NO_HANDLE) {
      Update();
    }
  }

  DISPMANX_UPDATE_HANDLE_T handle() const { return handle_; }

  void Update() {
    SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
    int32_t result = vc_dispmanx_update_submit_sync(handle_);
    SB_DCHECK(result == 0) << " result=" << result;
    handle_ = DISPMANX_NO_HANDLE;
  }

 private:
  DISPMANX_UPDATE_HANDLE_T handle_;

  SB_DISALLOW_COPY_AND_ASSIGN(DispmanxAutoUpdate);
};

}  // namespace

void DispmanxYUV420Resource::ClearWithBlack() {
  scoped_array<uint8_t> data(new uint8_t[width_ * height_ * 3 / 2]);
  SbMemorySet(data.get(), width_ * height_, 0);
  SbMemorySet(data.get() + width_ * height_, width_ * height_ / 2, 0x80);
  WriteData(data.get());
}

DispmanxElement::DispmanxElement(const DispmanxDisplay& display,
                                 int32_t layer,
                                 const DispmanxRect& dest_rect,
                                 const DispmanxResource& src,
                                 const DispmanxRect& src_rect) {
  DispmanxAutoUpdate update;
  handle_ = vc_dispmanx_element_add(update.handle(), display.handle(), layer,
                                    &dest_rect, src.handle(), &src_rect,
                                    DISPMANX_PROTECTION_NONE, NULL /*alpha*/,
                                    NULL /*clamp*/, DISPMANX_NO_ROTATE);
  SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
}

DispmanxElement::~DispmanxElement() {
  DispmanxAutoUpdate update;
  int32_t result = vc_dispmanx_element_remove(update.handle(), handle_);
  SB_DCHECK(result == 0) << " result=" << result;
}

void DispmanxVideoRenderer::Update(const VideoFrame& video_frame) {
  if (video_frame.IsEndOfStream()) {
    element_.reset();
    resource_.reset();
    return;
  }
  if (!resource_ || resource_->width() != video_frame.width() ||
      resource_->height() != video_frame.height()) {
    element_.reset();
    resource_.reset();

    DispmanxRect src_rect(0, 0, video_frame.width() << 16,
                          video_frame.height() << 16);
    resource_.reset(
        new DispmanxYUV420Resource(video_frame.width(), video_frame.height()));
    element_.reset(new DispmanxElement(display_, layer_, DispmanxRect(),
                                       *resource_, src_rect));
  }

  resource_->WriteData(video_frame.GetPlane(0).data);
}

}  // namespace shared
}  // namespace raspi
}  // namespace starboard
