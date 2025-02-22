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

#include "starboard/shared/uikit/application_darwin.h"

#include "starboard/common/atomic.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"

#import <UIKit/UIKit.h>

#import "starboard/shared/uikit/playback_capabilities.h"

namespace starboard {
namespace shared {
namespace uikit {
namespace {

SbAtomic32 s_idle_timer_lock_count = 0;

}  // namespace

void ApplicationDarwin::Initialize() {
  SbAudioSinkPrivate::Initialize();
  PlaybackCapabilities::InitializeInBackground();
}

void ApplicationDarwin::Teardown() {
  SbAudioSinkPrivate::TearDown();
}

// static
void ApplicationDarwin::IncrementIdleTimerLockCount() {
  if (SbAtomicBarrier_Increment(&s_idle_timer_lock_count, 1) == 1) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIApplication.sharedApplication.idleTimerDisabled =
          SbAtomicAcquire_Load(&s_idle_timer_lock_count) > 0;
    });
  }
}

// static
void ApplicationDarwin::DecrementIdleTimerLockCount() {
  if (SbAtomicBarrier_Increment(&s_idle_timer_lock_count, -1) == 0) {
    dispatch_async(dispatch_get_main_queue(), ^{
      UIApplication.sharedApplication.idleTimerDisabled =
          SbAtomicAcquire_Load(&s_idle_timer_lock_count) > 0;
    });
  }
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
