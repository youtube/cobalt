// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_JS_PROFILER_PROFILER_GROUP_H_
#define COBALT_JS_PROFILER_PROFILER_GROUP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "cobalt/js_profiler/profiler.h"
#include "cobalt/js_profiler/profiler_trace.h"
#include "cobalt/script/global_environment.h"
#include "third_party/v8/include/v8-profiler.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace js_profiler {

// Forward Declaration of Profiler Class.
class Profiler;

// A ProfilerGroup represents a set of window.Profiler JS objects sharing an
// underlying v8::CpuProfiler attached to a common isolate.
class ProfilerGroup : public base::MessageLoop::DestructionObserver {
 public:
  explicit ProfilerGroup(v8::Isolate* isolate) : isolate_(isolate) {}

  ~ProfilerGroup() = default;

  v8::CpuProfilingStatus ProfilerStart(const scoped_refptr<Profiler>& profiler,
                                       script::EnvironmentSettings* settings,
                                       v8::CpuProfilingOptions options);

  ProfilerTrace ProfilerStop(Profiler* profiler);

  void ProfilerCancel(Profiler* profiler);

  void DispatchSampleBufferFullEvent(std::string profiler_id);

  static ProfilerGroup* From(script::EnvironmentSettings* environment_settings);

  // Generates an unused string identifier to use for a new profiling session.
  std::string NextProfilerId();

  // From base::MessageLoop::DestructionObserver.
  // All active profiling threads must be stopped before discarding this object.
  void WillDestroyCurrentMessageLoop() override;

  int num_active_profilers() const { return num_active_profilers_; }

  bool active() const { return !!cpu_profiler_; }

 private:
  Profiler* GetProfiler(std::string profiler_id);
  void PopProfiler(std::string profiler_id);

  v8::Isolate* isolate_;
  v8::CpuProfiler* cpu_profiler_ = nullptr;

  int num_active_profilers_ = 0;
  int next_profiler_id_ = 0;

  // All active profilers, retained from GC.
  std::vector<scoped_refptr<Profiler>> profilers_;
};

// ProfilerMaxSamplesDelegate has a notify function that is called when the
// number of collected samples exceeds the allocated buffer. It is primarily
// responsible for the "samplebufferfull" event listener on the Profiler class.
class ProfilerMaxSamplesDelegate : public v8::DiscardedSamplesDelegate {
 public:
  explicit ProfilerMaxSamplesDelegate(ProfilerGroup* profiler_group,
                                      std::string profiler_id)
      : profiler_group_(profiler_group), profiler_id_(profiler_id) {}

  void Notify() override;

 private:
  cppgc::WeakMember<ProfilerGroup> profiler_group_;
  std::string profiler_id_;
};

}  // namespace js_profiler
}  // namespace cobalt

#endif  // COBALT_JS_PROFILER_PROFILER_GROUP_H_
