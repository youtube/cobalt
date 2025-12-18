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

#include "starboard/common/command_line.h"
#include "starboard/common/log.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/queue_application.h"
#import "starboard/tvos/shared/media/drm_manager.h"
#import "starboard/tvos/shared/media/playback_capabilities.h"
#import "starboard/tvos/shared/media/player_manager.h"
#import "starboard/tvos/shared/starboard_application.h"

@interface ObjCApplication : NSObject <SBDStarboardApplication>
@end

@implementation ObjCApplication
@synthesize drmManager = _drmManager;
@synthesize playerManager = _playerManager;

- (instancetype)init {
  self = [super init];
  if (self) {
    _drmManager = [[SBDDrmManager alloc] init];
    _playerManager = [[SBDPlayerManager alloc] init];
  }
  return self;
}

@end

namespace starboard {
namespace {

std::atomic_int32_t s_idle_timer_lock_count{0};

__weak id<SBDStarboardApplication> g_starboard_application_ = nil;

void EmptyHandleCallback(const SbEvent* event) {
  SB_LOG(ERROR) << "Starboard event DISCARDED:" << event->type;
}

}  // namespace

struct ApplicationDarwin::ObjCStorage {
  ObjCApplication* __strong objc_application;
};

class ApplicationDarwin::ApplicationDarwinInternal final
    : public shared::starboard::QueueApplication {
 public:
  explicit ApplicationDarwinInternal(
      std::unique_ptr<::starboard::CommandLine> command_line)
      : QueueApplication(EmptyHandleCallback) {
    SetCommandLine(std::move(command_line));
  }

  ~ApplicationDarwinInternal() override = default;

 protected:
  // QueueApplication overrides.
  bool IsStartImmediate() override { return false; }
  bool MayHaveSystemEvents() override { return false; }
  shared::starboard::Application::Event* WaitForSystemEventWithTimeout(
      int64_t time) override {
    return nullptr;
  }
  void WakeSystemEventWait() override {}
};

ApplicationDarwin::ApplicationDarwin(
    std::unique_ptr<::starboard::CommandLine> command_line)
    : application_darwin_internal_(
          std::make_unique<ApplicationDarwinInternal>(std::move(command_line))),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->objc_application = [[ObjCApplication alloc] init];
  SB_CHECK(objc_storage_->objc_application);
  g_starboard_application_ = objc_storage_->objc_application;

  SbAudioSinkImpl::Initialize();
  PlaybackCapabilities::InitializeInBackground();
}

ApplicationDarwin::~ApplicationDarwin() {
  SbAudioSinkImpl::TearDown();
  g_starboard_application_ = nullptr;
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

id<SBDStarboardApplication> SBDGetApplication() {
  return starboard::g_starboard_application_;
}
