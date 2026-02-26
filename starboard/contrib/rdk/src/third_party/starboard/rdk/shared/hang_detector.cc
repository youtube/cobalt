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
#include "third_party/starboard/rdk/shared/hang_detector.h"

#include <algorithm>
#include <vector>

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>

#include "starboard/common/once.h"
#include "starboard/thread.h"

#include "third_party/starboard/rdk/shared/log_override.h"

using namespace std::chrono_literals;
using namespace std::chrono;

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

#if defined(COBALT_BUILD_TYPE_GOLD)
const int32_t kMaxExpirationCount = 6;
#else
const int32_t kMaxExpirationCount = 18;
#endif

pid_t get_tid() {
#ifdef SYS_gettid
  return syscall(SYS_gettid);
#else
  return 0;
#endif
}

void print_action(pid_t pid, pid_t tid, const std::string& name) {
  fprintf(stderr, "\n*** Cobalt hang monitor expired!!! pid=%ld, tid=%ld, monitor='%s'. Continue.\n", (long)pid, (long)tid, name.c_str());
}

void kill_action(pid_t pid, pid_t tid, const std::string& name) {
  fprintf(stderr, "\n*** Hang detected in Cobalt!!! pid=%ld, tid=%ld, monitor='%s'. sending SIGFPE \n", (long)pid, (long)tid, name.c_str());

  long rc = -1;

#ifdef __NR_tgkill
  if (tid > 0) {
    rc = syscall(__NR_tgkill, pid, tid, SIGFPE);
  }
#endif

  if (rc < 0)
    rc = kill(pid, SIGKILL);

  std::this_thread::sleep_for ( 60s );
  fprintf(stderr, "\n*** Hang detected in Cobalt!!! pid=%ld, monitor='%s'. still hanging... sending SIGKILL \n", (long)pid, name.c_str());

  kill(pid, SIGKILL);

  std::this_thread::sleep_for ( 60s );
  fprintf(stderr, "\n*** Hang detected in Cobalt!!! pid=%ld, monitor='%s'. still hanging... give up \n", (long)pid, name.c_str());
}

seconds get_check_interval() {
  const char* env = std::getenv("COBALT_HANG_DETECTOR_INTERVAL_IN_SECONDS");
  if( env ) {
    int64_t t = strtol(env, nullptr, 0);
    if ( 0 >= t )
      return seconds::max();
    else
      return seconds { t };
  }
  return seconds { 30 };
}

}

struct HangDetector
{
  static void* ThreadEntryPoint(void* context) {
    SbThreadSetPriority(kSbThreadNoPriority);
    SB_DCHECK(context);
    static_cast<HangDetector*>(context)->DoWork();
    return nullptr;
  }

  HangDetector() {
    if ( check_interval_ == decltype(check_interval_)::max() )
      return;

    thread_ = std::thread(&HangDetector::ThreadEntryPoint, this);

    SB_DCHECK(thread_.joinable());
  }

  ~HangDetector() {
    if (thread_.joinable()) {
      std::unique_lock lock(mutex_);
      running_ = false;
      monitors_.clear();
      condition_.notify_all();
      lock.unlock();
      thread_.join();
    }
  }

  void DoWork() {
    time_point<steady_clock> next_check;
    pid_t pid = getpid();

    for (;;) {
      // don't hog the cpu in case timed condition fails
      usleep( 1000 );

      std::unique_lock lock(mutex_);
      if ( running_ == false )
        return;

      if ( condition_.wait_for( lock, std::chrono::microseconds(duration_cast<microseconds>(check_interval_).count()) ) == std::cv_status::no_timeout )
        continue;

      auto now = steady_clock::now();

      if ( now > next_check ) {
        next_check = now + check_interval_;

        for (const auto& m : monitors_) {
          if (m->GetExpirationTime() > now)
            continue;

          pid_t tid = m->GetTID();
          std::string name = m->Name();

          if ( m->IncExpirationCount() < kMaxExpirationCount ) {
            print_action( pid, tid, name );
            continue;
          }

          mutex_.unlock();
          kill_action( pid, tid, name );
          mutex_.lock();

          running_ = false;
          SB_CHECK(false);
          break;
        }
      }
    }
  }

  void AddMonitor(HangMonitor *m) {
    std::lock_guard lock(mutex_);
    monitors_.push_back(m);
  }

  void RemoveMonitor(HangMonitor *m) {
    std::lock_guard lock(mutex_);
    monitors_.erase(std::remove(monitors_.begin(), monitors_.end(), m), monitors_.end());
  }

  seconds GetCheckInterval() const {
    return check_interval_;
  }

  std::mutex& Lock() {
    return mutex_;
  }

private:
  const seconds check_interval_ { get_check_interval() };

  std::thread thread_;
  bool running_ { true };
  std::vector<HangMonitor*> monitors_;

  std::mutex mutex_;
  std::condition_variable condition_;
};

SB_ONCE_INITIALIZE_FUNCTION(HangDetector, GetHangDetector);

HangMonitor::HangMonitor(std::string name)
  : name_(std::move(name))
  , expiration_time_(steady_clock::now() + GetHangDetector()->GetCheckInterval()) {
  GetHangDetector()->AddMonitor(this);
  tid_ = get_tid();
}

HangMonitor::~HangMonitor() {
  GetHangDetector()->RemoveMonitor(this);
}

const std::string& HangMonitor::Name() const {
  return name_;
}

time_point<steady_clock> HangMonitor::GetExpirationTime() const {
  return expiration_time_;
}

milliseconds HangMonitor::GetResetInterval() const {
  return GetHangDetector()->GetCheckInterval() / 2;
}

pid_t HangMonitor::GetTID() const {
  return tid_;
}

int HangMonitor::IncExpirationCount() {
  return ++expiration_count_;
}

void HangMonitor::Reset() {
  auto &hang_detector = *GetHangDetector();
  std::lock_guard lock(hang_detector.Lock());
  expiration_time_ = steady_clock::now() + hang_detector.GetCheckInterval();
  expiration_count_ = 0;
  tid_ = get_tid();
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
