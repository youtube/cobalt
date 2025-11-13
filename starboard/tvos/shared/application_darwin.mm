// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/application_darwin.h"

#import <UIKit/UIKit.h>

#include <atomic>

#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#import "starboard/tvos/shared/media/playback_capabilities.h"

namespace starboard {
namespace {

std::atomic_int32_t s_idle_timer_lock_count{0};

}  // namespace

void ApplicationDarwin::Initialize() {
  SbAudioSinkImpl::Initialize();
  PlaybackCapabilities::InitializeInBackground();
}

void ApplicationDarwin::Teardown() {
  SbAudioSinkImpl::TearDown();
}

// static
void ApplicationDarwin::IncrementIdleTimerLockCount() {
  if (s_idle_timer_lock_count.fetch_add(1, std::memory_order_relaxed) == 1) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIApplication.sharedApplication.idleTimerDisabled =
          s_idle_timer_lock_count.load(std::memory_order_acquire) > 0;
    });
  }
}

// static
void ApplicationDarwin::DecrementIdleTimerLockCount() {
  if (s_idle_timer_lock_count.fetch_sub(1, std::memory_order_relaxed) == 0) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIApplication.sharedApplication.idleTimerDisabled =
          s_idle_timer_lock_count.load(std::memory_order_acquire) > 0;
    });
  }
}

}  // namespace starboard
