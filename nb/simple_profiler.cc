/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "nb/rewindable_vector.h"
#include "nb/simple_profiler.h"
#include "nb/thread_local_object.h"
#include "starboard/atomic.h"
#include "starboard/common/string.h"
#include "starboard/once.h"
#include "starboard/time.h"

namespace nb {
namespace {

class SimpleProfilerManager {
 public:
  struct Entry {
    Entry() : name(nullptr), time_delta(0), indent_value(0) {}
    Entry(const char* n, SbTimeMonotonic dt, int ind)
        : name(n), time_delta(dt), indent_value(ind) {}
    Entry(const Entry& e) = default;
    const char* name;
    SbTimeMonotonic time_delta;
    int indent_value;
  };

  using MessageHandlerFunction = SimpleProfiler::MessageHandlerFunction;
  using ClockFunction = SimpleProfiler::ClockFunction;
  SimpleProfilerManager()
      : default_enabled_(true), message_handler_(nullptr),
        clock_function_(nullptr) {}

  int BeginInstance(const char* name) {
    Data& d = ThreadLocal();
    if (!d.enable_flag) {
      return -1;
    }
    SbTimeMonotonic now = NowTime();
    // SbAtomicMemoryBarrier() to Keep order of operations so clock doesn't
    // get sampled out of order.
    SbAtomicMemoryBarrier();
    d.profiles.emplace_back(name, now, d.instance_count);
    ++d.instance_count;
    return d.profiles.size() - 1;
  }

  void Output(const Entry& entry, std::stringstream* sstream) {
    for (auto i = 0; i < entry.indent_value; ++i) {
      (*sstream) << ' ';
    }
    (*sstream) << entry.name << ": " << entry.time_delta << "us\n";
  }

  void FinishInstance(int index) {
    Data& d = ThreadLocal();
    if (!d.enable_flag || index < 0) {
      return;
    }

    --d.instance_count;
    Entry& entry = d.profiles[static_cast<size_t>(index)];
    entry.time_delta = NowTime() - entry.time_delta;
    // SbAtomicMemoryBarrier() to Keep order of operations so clock doesn't
    // get sampled out of order.
    SbAtomicMemoryBarrier();

    if (d.instance_count == 0) {
      std::stringstream ss;
      for (auto it = d.profiles.begin(); it != d.profiles.end(); ++it) {
        Output(*it, &ss);
      }
      d.profiles.clear();
      HandleMessage(ss.str());
    }
  }
  bool ThreadLocalEnabled() { return ThreadLocal().enable_flag; }
  void SetThreadLocalEnabled(bool value) {
    Data& d = ThreadLocal();
    d.enable_flag = value;
    d.enable_flag_set = true;
  }

  bool IsEnabled() const {
    const Data& d = ThreadLocal();
    if (d.enable_flag_set) {
      return d.enable_flag;
    }
    return default_enabled_;
  }
  void SetDefaultEnabled(bool value) { default_enabled_ = value; }

  void SetMessageHandler(MessageHandlerFunction handler) {
    message_handler_ = handler;
  }

  void SetClockFunction(ClockFunction fcn) {
    clock_function_ = fcn;
  }

  SbTimeMonotonic NowTime() {
    if (!clock_function_) {
      return SbTimeGetMonotonicNow();
    } else {
      return clock_function_();
    }
  }

 private:
  struct Data {
    bool enable_flag = true;
    bool enable_flag_set = false;
    int instance_count = 0;
    std::vector<Entry> profiles;
  };

  void HandleMessage(const std::string& str) {
    if (message_handler_) {
      message_handler_(str.c_str());
    } else {
      SbLogRaw(str.c_str());
    }
  }

  Data& ThreadLocal() { return *thread_local_.GetOrCreate(); }
  const Data& ThreadLocal() const { return *thread_local_.GetOrCreate(); }
  mutable nb::ThreadLocalObject<Data> thread_local_;
  bool default_enabled_;
  MessageHandlerFunction message_handler_;
  ClockFunction clock_function_;
};

SB_ONCE_INITIALIZE_FUNCTION(SimpleProfilerManager, GetSimpleProfilerManager);

}  // namespace

SimpleProfiler::EnableScope::EnableScope(bool enabled) {
  SimpleProfilerManager* mgr = GetSimpleProfilerManager();
  prev_enabled_ = mgr->ThreadLocalEnabled();
  mgr->SetThreadLocalEnabled(enabled);
}

SimpleProfiler::EnableScope::~EnableScope() {
  GetSimpleProfilerManager()->SetThreadLocalEnabled(prev_enabled_);
}

SimpleProfiler::SimpleProfiler(const char* name) {
  momento_index_ = GetSimpleProfilerManager()->BeginInstance(name);
}

SimpleProfiler::~SimpleProfiler() {
  GetSimpleProfilerManager()->FinishInstance(momento_index_);
}

void SimpleProfiler::SetEnabledByDefault(bool value) {
  GetSimpleProfilerManager()->SetDefaultEnabled(value);
}

bool SimpleProfiler::IsEnabled() {
  return GetSimpleProfilerManager()->IsEnabled();
}

void SimpleProfiler::SetLoggingFunction(MessageHandlerFunction fcn) {
  GetSimpleProfilerManager()->SetMessageHandler(fcn);
}

void SimpleProfiler::SetClockFunction(ClockFunction fcn) {
  GetSimpleProfilerManager()->SetClockFunction(fcn);
}

}  // namespace nb
