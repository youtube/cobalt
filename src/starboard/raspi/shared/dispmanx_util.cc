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

const int kElementChangeAttributesFlagSrcRect = 1 << 3;

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

DispmanxResource::DispmanxResource(VC_IMAGE_TYPE_T image_type,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t visible_width,
                                   uint32_t visible_height)
    : width_(width),
      height_(height),
      visible_width_(visible_width),
      visible_height_(visible_height) {
  static const uint32_t kMaxDimension = 1 << 16;

  SB_DCHECK(width_ > 0 && width_ < kMaxDimension);
  SB_DCHECK(height_ > 0 && height_ < kMaxDimension);
  SB_DCHECK(visible_width_ > 0 && visible_width_ < kMaxDimension);
  SB_DCHECK(visible_height > 0 && visible_height < kMaxDimension);
  SB_DCHECK(width_ >= visible_width_);
  SB_DCHECK(height_ >= visible_height);

  uint32_t vc_image_ptr;

  handle_ = vc_dispmanx_resource_create(
      image_type, visible_width_ | (width_ << 16),
      visible_height | (height_ << 16), &vc_image_ptr);
  SB_DCHECK(handle_ != DISPMANX_NO_HANDLE);
}

void DispmanxYUV420Resource::WriteData(const void* data) {
  SB_DCHECK(handle() != DISPMANX_NO_HANDLE);

  DispmanxRect dst_rect(0, 0, width(), height() * 3 / 2);
  int32_t result = vc_dispmanx_resource_write_data(
      handle(), VC_IMAGE_YUV420, width(), const_cast<void*>(data), &dst_rect);
  SB_DCHECK(result == 0);
}

void DispmanxYUV420Resource::ClearWithBlack() {
  scoped_array<uint8_t> data(new uint8_t[width() * height() * 3 / 2]);
  SbMemorySet(data.get(), width() * height(), 0);
  SbMemorySet(data.get() + width() * height(), width() * height() / 2, 0x80);
  WriteData(data.get());
}

void DispmanxRGB565Resource::WriteData(const void* data) {
  SB_DCHECK(handle() != DISPMANX_NO_HANDLE);

  DispmanxRect dst_rect(0, 0, width(), height());
  int32_t result =
      vc_dispmanx_resource_write_data(handle(), VC_IMAGE_RGB565, width() * 2,
                                      const_cast<void*>(data), &dst_rect);
  SB_DCHECK(result == 0);
}

void DispmanxRGB565Resource::ClearWithBlack() {
  scoped_array<uint8_t> data(new uint8_t[width() * height() * 2]);
  SbMemorySet(data.get(), width() * height() * 2, 0);
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

void DispmanxElement::ChangeSource(const DispmanxResource& new_src) {
  DispmanxAutoUpdate update;
  vc_dispmanx_element_change_source(update.handle(), handle_, new_src.handle());
}

DispmanxVideoRenderer::DispmanxVideoRenderer(const DispmanxDisplay& display,
                                             int32_t layer)
    : black_frame_(256, 256, 256, 256) {
  black_frame_.ClearWithBlack();
  element_.reset(new DispmanxElement(display, layer, DispmanxRect(),
                                     black_frame_, DispmanxRect()));
}

void DispmanxVideoRenderer::Update(
    const scoped_refptr<VideoFrame>& video_frame) {
  SB_DCHECK(video_frame);

  if (frame_ == video_frame) {
    return;
  }

  if (video_frame->is_end_of_stream()) {
    element_->ChangeSource(black_frame_);
    frame_ = video_frame;
    return;
  }

  DispmanxYUV420Resource* resource =
      static_cast<DispmanxVideoFrame*>(video_frame.get())->resource();
  element_->ChangeSource(*resource);
  frame_ = video_frame;
}

}  // namespace shared
}  // namespace raspi
}  // namespace starboard
