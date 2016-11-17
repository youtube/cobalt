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

#include "starboard/raspi/shared/application_dispmanx.h"

#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>

#include "starboard/input.h"
#include "starboard/key.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/raspi/shared/window_internal.h"
#include "starboard/shared/linux/dev_input/dev_input.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace raspi {
namespace shared {

using ::starboard::shared::dev_input::DevInput;

namespace {

// TODO: Support arbitrary frame size.
const int kMaxVideoFrameWidth = 1920;
const int kMaxVideoFrameHeight = 1080;
const int kMaxVideoFrameHeightAligned = 1088;

// Fill the resource into black pixels.
void ClearYUV420Resource(DISPMANX_RESOURCE_HANDLE_T resource,
                         int width,
                         int height) {
  SB_DCHECK(width > 0 && height > 0);

  std::vector<uint8_t> pixels;
  pixels.reserve(width * height * 3 / 2);
  pixels.assign(width * height, 0);
  pixels.insert(pixels.end(), width * height / 2, 0x80);

  VC_RECT_T dst_rect;
  vc_dispmanx_rect_set(&dst_rect, 0, 0, width, height * 3 / 2);
  int32_t result = vc_dispmanx_resource_write_data(
      resource, VC_IMAGE_YUV420, width, &pixels[0], &dst_rect);
  SB_DCHECK(result == 0);
}

}  // namespace

SbWindow ApplicationDispmanx::CreateWindow(const SbWindowOptions* options) {
  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  InitializeDispmanx();

  SB_DCHECK(IsDispmanxInitialized());
  window_ = new SbWindowPrivate(display_, options);
  input_ = DevInput::Create(window_);

  // Create the dispmanx element to display video frames.
  int result = 0;
  uint32_t vc_image_ptr;

  video_frame_resource_ =
      vc_dispmanx_resource_create(VC_IMAGE_YUV420, kMaxVideoFrameWidth,
                                  kMaxVideoFrameHeightAligned, &vc_image_ptr);
  SB_DCHECK(video_frame_resource_ != DISPMANX_NO_HANDLE);

  VC_DISPMANX_ALPHA_T alpha = {DISPMANX_FLAGS_ALPHA_FROM_SOURCE, 255, 0};

  VC_RECT_T src_rect;
  VC_RECT_T dst_rect;
  vc_dispmanx_rect_set(&src_rect, 0, 0, kMaxVideoFrameWidth << 16,
                       kMaxVideoFrameHeight << 16);
  vc_dispmanx_rect_set(&dst_rect, 0, 0, 0, 0);

  ClearYUV420Resource(video_frame_resource_, kMaxVideoFrameWidth,
                      kMaxVideoFrameHeightAligned);

  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0 /*screen*/);
  video_frame_element_ = vc_dispmanx_element_add(
      update, display_, -1, &dst_rect, video_frame_resource_, &src_rect,
      DISPMANX_PROTECTION_NONE, &alpha, NULL, DISPMANX_NO_ROTATE);
  SB_DCHECK(video_frame_element_ != DISPMANX_NO_HANDLE);
  result = vc_dispmanx_update_submit_sync(update);
  SB_DCHECK(result == 0) << " result=" << result;

  return window_;
}

bool ApplicationDispmanx::DestroyWindow(SbWindow window) {
  if (!SbWindowIsValid(window)) {
    return false;
  }

  SB_DCHECK(IsDispmanxInitialized());

  SB_DCHECK(input_);
  delete input_;
  input_ = NULL;

  DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0 /*screen*/);
  int32_t result = vc_dispmanx_element_remove(update, video_frame_element_);
  SB_DCHECK(result == 0) << " result=" << result;
  vc_dispmanx_update_submit_sync(update);
  video_frame_element_ = DISPMANX_NO_HANDLE;

  result = vc_dispmanx_resource_delete(video_frame_resource_);
  SB_DCHECK(result == 0) << " result=" << result;
  video_frame_resource_ = DISPMANX_NO_HANDLE;

  SB_DCHECK(window_ == window);
  delete window;
  window_ = kSbWindowInvalid;
  ShutdownDispmanx();
  return true;
}

void ApplicationDispmanx::Initialize() {
  SbAudioSinkPrivate::Initialize();
}

void ApplicationDispmanx::Teardown() {
  ShutdownDispmanx();
  SbAudioSinkPrivate::TearDown();
}

void ApplicationDispmanx::AcceptFrame(SbPlayer player,
                                      const VideoFrame& frame,
                                      int x,
                                      int y,
                                      int width,
                                      int height) {
  VC_RECT_T dst_rect;
  vc_dispmanx_rect_set(&dst_rect, 0, 0, frame.width(), frame.height() * 3 / 2);
  int32_t result = 0;
  if (frame.IsEndOfStream()) {
    ClearYUV420Resource(video_frame_resource_, kMaxVideoFrameWidth,
                        kMaxVideoFrameHeightAligned);
  } else {
    result = vc_dispmanx_resource_write_data(
        video_frame_resource_, VC_IMAGE_YUV420,
        frame.GetPlane(0).pitch_in_bytes,
        const_cast<uint8_t*>(frame.GetPlane(0).data), &dst_rect);
  }
  SB_DCHECK(result == 0);
}

bool ApplicationDispmanx::MayHaveSystemEvents() {
  return input_ != NULL;
}

::starboard::shared::starboard::Application::Event*
ApplicationDispmanx::PollNextSystemEvent() {
  SB_DCHECK(input_);
  return input_->PollNextSystemEvent();
}

::starboard::shared::starboard::Application::Event*
ApplicationDispmanx::WaitForSystemEventWithTimeout(SbTime duration) {
  SB_DCHECK(input_);
  Event* event = input_->WaitForSystemEventWithTimeout(duration);
}

void ApplicationDispmanx::WakeSystemEventWait() {
  SB_DCHECK(input_);
  input_->WakeSystemEventWait();
}

void ApplicationDispmanx::InitializeDispmanx() {
  if (IsDispmanxInitialized()) {
    return;
  }

  bcm_host_init();
  display_ = vc_dispmanx_display_open(0);
  SB_DCHECK(IsDispmanxInitialized());
}

void ApplicationDispmanx::ShutdownDispmanx() {
  if (!IsDispmanxInitialized()) {
    return;
  }

  SB_DCHECK(!SbWindowIsValid(window_));
  int result = vc_dispmanx_display_close(display_);
  SB_DCHECK(result == 0);
  display_ = DISPMANX_NO_HANDLE;
  bcm_host_deinit();
}

}  // namespace shared
}  // namespace raspi
}  // namespace starboard
