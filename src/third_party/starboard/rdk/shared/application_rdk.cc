//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/starboard/rdk/shared/application_rdk.h"

#include "starboard/common/log.h"
#include "starboard/event.h"
#include "starboard/speech_synthesis.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/media/key_system_supportability_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"
#include "third_party/starboard/rdk/shared/window/window_internal.h"
#include "third_party/starboard/rdk/shared/log_override.h"
#include "third_party/starboard/rdk/shared/time_constants.h"

#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <malloc.h>
#include <chrono>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace libcobalt_api {
void Initialize();
void Teardown();
}

namespace player {
void ForceStop();
}  // namespace player

EssTerminateListener Application::terminateListener = {
  //terminated
  [](void* data) { reinterpret_cast<Application*>(data)->OnTerminated(); }
};

EssKeyListener Application::keyListener = {
  // keyPressed
  [](void* data, unsigned int key) { reinterpret_cast<Application*>(data)->OnKeyPressed(key); },
  // keyReleased
  [](void* data, unsigned int key) { reinterpret_cast<Application*>(data)->OnKeyReleased(key); },
  // keyRepeat
  [](void* data, unsigned int key) { reinterpret_cast<Application*>(data)->OnKeyPressed(key); }
};

EssSettingsListener Application::settingsListener = {
  // displaySize
  [](void *data, int width, int height ) { reinterpret_cast<Application*>(data)->OnDisplaySize(width, height); },
  // displaySafeArea
  nullptr
};

constexpr auto kEssRunLoopPeriod = 16666us;

static void setTimerInterval(int fd, microseconds time) {
  struct itimerspec timeout;
  timeout.it_value.tv_sec = time.count() / kSbTimeSecond;
  timeout.it_value.tv_nsec = ( time.count() % kSbTimeSecond ) * kSbTimeNanosecondsPerMicrosecond;
  timeout.it_interval.tv_sec = timeout.it_value.tv_sec;
  timeout.it_interval.tv_nsec = timeout.it_value.tv_nsec;
  int rc = timerfd_settime(fd, 0, &timeout, NULL);
  if (rc == -1) {
    SB_LOG(ERROR) << "Failed to set timer interval, error: " << errno << " ("<< strerror(errno) << ')';
  }
}

using ::starboard::SbAudioSinkImpl;

Application::Application(SbEventHandleCallback sb_event_handle_callback)
  : QueueApplication(sb_event_handle_callback)
  , input_handler_(new EssInput)
  , hang_monitor_(new HangMonitor("Application")) {
  essos_context_recycle_ = !!getenv("COBALT_ESSOS_CONTEXT_DESTROY");
  BuildEssosContext();
}

Application::~Application() {
  EssContextDestroy(ctx_);
}

void Application::Initialize() {
  wakeup_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if ( wakeup_fd_ == -1 ) {
    SB_LOG(ERROR) << "Failed to create eventfd, error: " << errno << " (" << strerror(errno) << ')';
  }

  ess_timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if ( ess_timer_fd_ == -1 ) {
    SB_LOG(ERROR) << "Failed to create timerfd, error: " << errno << " (" << strerror(errno) << ')';
  } else {
    setTimerInterval(ess_timer_fd_, kEssRunLoopPeriod);
  }

#if defined(COBALT_BUILD_TYPE_DEVEL)
  hang_monitor_.reset();
  SB_LOG(INFO) << "Disable application watch dog for devel build.";
#else
  monitor_timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
  if ( monitor_timer_fd_ == -1 ) {
    SB_LOG(ERROR) << "Failed to create timerfd, error: " << errno << " (" << strerror(errno) << ')';
    hang_monitor_.reset();
  } else {
    setTimerInterval(monitor_timer_fd_, hang_monitor_->GetResetInterval());
  }
#endif

  SbAudioSinkImpl::Initialize();
  libcobalt_api::Initialize();
  using ::starboard::KeySystemSupportabilityCache;
  using ::starboard::MimeSupportabilityCache;
  MimeSupportabilityCache::GetInstance()->SetCacheEnabled(true);
  KeySystemSupportabilityCache::GetInstance()->SetCacheEnabled(true);

  ScheduleMemoryUsageCheck(kSbTimeSecond);
}

void Application::Teardown() {
  SbAudioSinkImpl::TearDown();
  libcobalt_api::Teardown();
  TeardownJSONRPCLink();

  close(ess_timer_fd_);
  close(wakeup_fd_);
  if ( !(monitor_timer_fd_ < 0) )
      close(monitor_timer_fd_);
  ess_timer_fd_ = wakeup_fd_ = monitor_timer_fd_ = -1;
}

bool Application::MayHaveSystemEvents() {
  return true;
}

::starboard::Application::Event*
Application::PollNextSystemEvent() {
  auto now = steady_clock::now();
  if ((now - ess_loop_last_ts_) > kEssRunLoopPeriod) {
    ess_loop_last_ts_ = now;
    EssContextRunEventLoopOnce( ctx_ );
  }
  return NULL;
}

::starboard::Application::Event*
Application::WaitForSystemEventWithTimeout(int64_t time) {
  struct timespec timeout;
  struct pollfd fds[3];
  int fds_sz = 0;
  int rc = 0;

  if ( !(ess_timer_fd_ < 0) ) {
    fds[fds_sz].fd = ess_timer_fd_;
    fds[fds_sz].events = POLLIN;
    fds[fds_sz].revents = 0;
    ++fds_sz;
  }

  if ( !(wakeup_fd_ < 0) ) {
    fds[fds_sz].fd = wakeup_fd_;
    fds[fds_sz].events = POLLIN;
    fds[fds_sz].revents = 0;
    ++fds_sz;
  }

  if ( !(monitor_timer_fd_ < 0) ) {
    fds[fds_sz].fd = monitor_timer_fd_;
    fds[fds_sz].events = POLLIN;
    fds[fds_sz].revents = 0;
    ++fds_sz;
  }

  if ( fds_sz != 0 ) {
    timeout.tv_sec = time / kSbTimeSecond;
    timeout.tv_nsec = (time % kSbTimeSecond) * kSbTimeNanosecondsPerMicrosecond;
    rc = ppoll(fds, fds_sz, &timeout, NULL);
  }

  if ( rc > 0 ) {
    for (int i = 0; i < fds_sz; ++i) {
      if ( (fds[i].revents & POLLIN) != POLLIN )
        continue;
      // Ack timer or wakeup event
      uint64_t tmp;
      read(fds[i].fd, &tmp, sizeof(uint64_t));

      if ( fds[i].fd == monitor_timer_fd_ ) {
        hang_monitor_->Reset();
      }
    }
  }

  return NULL;
}

void Application::WakeSystemEventWait() {
  uint64_t u = 1;
  write(wakeup_fd_, &u, sizeof(uint64_t));
}

SbWindow Application::CreateSbWindow(const SbWindowOptions* options) {
  SB_DCHECK(window_ == nullptr);
  if (window_ != nullptr)
    return kSbWindowInvalid;
  MaterializeNativeWindow();
  window_  = new SbWindowPrivate(options);
  return window_;
}

bool Application::DestroySbWindow(SbWindow window) {
  if (!SbWindowIsValid(window))
    return false;
  window_ = nullptr;
  delete window;
  DestroyNativeWindow();
  return true;
}

void Application::InjectInputEvent(SbInputData* data) {
  if (native_window_ == 0) {
    Application::DeleteDestructor<SbInputData>(data);
    return;
  }

  data->window = window_;
  Inject(new Event(kSbEventTypeInput, data,
                   &Application::DeleteDestructor<SbInputData>));
}

void Application::Inject(Event* e) {
  if (e && e->event && e->event->type == kSbEventTypeFreeze) {
    player::ForceStop();
  }

  QueueApplication::Inject(e);
}

void Application::OnSuspend() {
  SbSpeechSynthesisCancel();
  DestroyNativeWindow();
  TeardownJSONRPCLink();
  setTimerInterval(ess_timer_fd_, 1s);
}

void Application::OnResume() {
  if ( essos_context_recycle_ )
    BuildEssosContext();

  setTimerInterval(ess_timer_fd_, kEssRunLoopPeriod);
  MaterializeNativeWindow();
}

void Application::OnTerminated() {
  Stop(0);
}

void Application::OnKeyPressed(unsigned int key) {
  input_handler_->OnKeyPressed(key);
}

void Application::OnKeyReleased(unsigned int key) {
  input_handler_->OnKeyReleased(key);
}

void Application::OnDisplaySize(int width, int height) {
  if (window_width_ == width && window_height_ == height) {
    resize_pending_ = false;
    return;
  }

  SB_DCHECK(native_window_ == 0);
  resize_pending_ = true;
}

void Application::MaterializeNativeWindow() {
  if (native_window_ != 0)
    return;

  bool error = false;

  if ( !EssContextGetDisplaySize(ctx_, &window_width_, &window_height_) ) {
    error = true;
  }

  if ( resize_pending_ ) {
    EssContextResizeWindow(ctx_, window_width_, window_height_);
    resize_pending_ = false;
  }

  if ( !EssContextCreateNativeWindow(ctx_, window_width_, window_height_, &native_window_) ) {
    error = true;
  }
  else if ( !EssContextStart(ctx_) ) {
    error = true;
  }

  if ( error ) {
    const char *detail = EssContextGetLastErrorDetail(ctx_);
    SB_LOG(ERROR) << "Essos error: '" <<  detail << '\'';
    FatalError();
  }
}

void Application::DestroyNativeWindow() {
  if (native_window_ == 0)
    return;

  if ( !EssContextDestroyNativeWindow(ctx_, native_window_) ) {
    const char *detail = EssContextGetLastErrorDetail(ctx_);
    SB_LOG(ERROR) << "Essos error: '" <<  detail << '\'';
  }

  native_window_ = 0;

  if ( essos_context_recycle_ ) {
    EssContextDestroy(ctx_);
    ctx_ = NULL;
  }
  else
    EssContextStop(ctx_);
}

void Application::DisplayInfoChanged() {
  if (state() != kStateStarted)
    return;

  SbWindowSize window_size;
  SbWindowGetSize(window_, &window_size);
  auto *data = new SbEventWindowSizeChangedData();
  data->size = window_size;
  data->window = window_;
  WindowSizeChanged(data, &Application::DeleteDestructor<SbEventWindowSizeChangedData>);
}

void Application::BuildEssosContext() {
  bool error = false;
  ctx_ = EssContextCreate();

  if ( !EssContextInit(ctx_) ) {
    error = true;
  }
  else if ( !EssContextSetTerminateListener(ctx_, this, &terminateListener) ) {
    error = true;
  }
  else if ( !EssContextSetKeyListener(ctx_, this, &keyListener) ) {
    error = true;
  }
  else if ( !EssContextSetSettingsListener(ctx_, this, &settingsListener) ) {
    error = true;
  }

  if ( error ) {
    const char *detail = EssContextGetLastErrorDetail(ctx_);
    SB_LOG(ERROR) << "Essos error: '" <<  detail << '\'';
    FatalError();
  }
}

void Application::FatalError() {
  SB_LOG(ERROR) << "Exiting due to fatal error.";
  ess_loop_last_ts_ = time_point<steady_clock>::max(); // Stop Essos run loop
  SbEventSchedule([](void* data) {
    Application::Get()->Stop(0);
  }, nullptr, 0);
}

void Application::ReleaseMemory() {
  Inject(new Event(kSbEventTypeLowMemory, NULL, [](void*) {
    malloc_trim(0);
  }));
}

void Application::ScheduleMemoryUsageCheck(int64_t delay) {
  SbEventSchedule([](void* data) {
    int64_t back_off_timeout = Application::Get()->CheckMemoryUsage();
    if (back_off_timeout && back_off_timeout != std::numeric_limits<int64_t>::max())
      Application::Get()->ScheduleMemoryUsageCheck(back_off_timeout);
  }, nullptr, delay);
}

int64_t Application::CheckMemoryUsage() {
  static const int64_t kCPUMemoryPressureLimit = ([]() -> int64_t {
    const char* env = std::getenv("COBALT_CPU_MEM_PRESSURE_IN_MB");
    // For C25, default memory pressure threshold was 400 MB, but has been
    // increased for C26 to provide more headroom.
    int64_t limit_in_mb = SB_INT64_C(500);
    if( env ) {
      int64_t t = strtol(env, nullptr, 0);
      if ( t >= 0 )
        limit_in_mb = t;
    }
    int64_t total_in_bytes = SbSystemGetTotalCPUMemory();
    return std::min(total_in_bytes, limit_in_mb * 1024 * 1024);
  })();

  if (!kCPUMemoryPressureLimit)
    return std::numeric_limits<int64_t>::max();

  int64_t usage_in_bytes = SbSystemGetUsedCPUMemory();
  if (kCPUMemoryPressureLimit < usage_in_bytes) {
    SB_LOG(INFO) << "Triggering memory pressure event. Current CPU mem. usage: "
                 << uint64_t(usage_in_bytes / 1024) << " kb, pressure limit: "
                 << uint64_t(kCPUMemoryPressureLimit / 1024) << " kb.";
    ReleaseMemory();
    return 5 * kSbTimeSecond;
  }

  return kSbTimeSecond;
}

void Application::InjectAccessibilityTextToSpeechSettingsChanged(bool enabled) {
  bool* enabled_data = new bool(enabled);
  Inject(new Event(kSbEventTypeAccessibilityTextToSpeechSettingsChanged,
                   enabled_data,
                   &Application::DeleteDestructor<bool>));
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
