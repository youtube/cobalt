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
const int kVideoLayer = -1;
}  // namespace

SbWindow ApplicationDispmanx::CreateWindow(const SbWindowOptions* options) {
  if (SbWindowIsValid(window_)) {
    return kSbWindowInvalid;
  }

  InitializeDispmanx();

  SB_DCHECK(IsDispmanxInitialized());
  window_ = new SbWindowPrivate(*display_, options);
  input_ = DevInput::Create(window_);

  video_renderer_.reset(new DispmanxVideoRenderer(*display_, kVideoLayer));

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
                                      const scoped_refptr<VideoFrame>& frame,
                                      int z_index,
                                      int x,
                                      int y,
                                      int width,
                                      int height) {
  video_renderer_->Update(frame);
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
  return event;
}

void ApplicationDispmanx::WakeSystemEventWait() {
  SB_DCHECK(input_);
  input_->WakeSystemEventWait();
}

void ApplicationDispmanx::InitializeDispmanx() {
  if (IsDispmanxInitialized()) {
    return;
  }

  display_.reset(new DispmanxDisplay);
  SB_DCHECK(IsDispmanxInitialized());
}

void ApplicationDispmanx::ShutdownDispmanx() {
  if (!IsDispmanxInitialized()) {
    return;
  }

  SB_DCHECK(!SbWindowIsValid(window_));

  display_.reset();
}

}  // namespace shared
}  // namespace raspi
}  // namespace starboard
