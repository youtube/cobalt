// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "base/memory/singleton.h"
#include "cobalt/js_profiler/profiler.h"
#include "third_party/v8/include/v8-profiler.h"
#include "v8/include/cppgc/member.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace js_profiler {

// Forward Declaration of Profiler Class.
class Profiler;


// A ProfilerGroup represents a set of profilers sharing an underlying
// v8::CpuProfiler attached to a common isolate.
class ProfilerGroup {
 public:
  explicit ProfilerGroup(v8::Isolate* isolate) : isolate_(isolate) {}

  v8::CpuProfilingStatus ProfilerStart(Profiler* profiler,
                                       v8::CpuProfilingOptions options);

  v8::CpuProfile* ProfilerStop(Profiler* profiler);

  void DispatchSampleBufferFullEvent(std::string profiler_id);

  static ProfilerGroup* From(v8::Isolate* isolate);

  // Generates an unused string identifier to use for a new profiling session.
  std::string NextProfilerId();

 private:
  Profiler* GetProfiler(std::string profiler_id);
  void PopProfiler(std::string profiler_id);

  v8::Isolate* isolate_;
  v8::CpuProfiler* cpu_profiler_ = nullptr;
  std::vector<Profiler*> profilers_;
  int num_active_profilers_;
  int next_profiler_id_;
};

// A ProfilerGroupFactory represents a singleton that maps one Isolate to
// one ProfilerGroup.
class ProfilerGroupFactory {
 public:
  friend struct base::StaticMemorySingletonTraits<ProfilerGroupFactory>;

  // Fetches or creates the Singleton.
  static ProfilerGroupFactory* GetInstance();

  // Fetches or creates the ProfilerGroup associated with the given isolate.
  ProfilerGroup* getOrCreateProfilerGroup(v8::Isolate* isolate);

 private:
  // Class can only be instanced via the singleton
  ProfilerGroupFactory() {}
  ~ProfilerGroupFactory() {}

  // V8 Per Isolate data. One Isolate to One ProfilerGroup mapping type.
  typedef std::map<v8::Isolate*, ProfilerGroup*> ProfilerGroupMap;

  // V8 Per Isolate data. One Isolate to One ProfilerGroup mapping.
  ProfilerGroupMap profiler_group_per_isolate_map_;
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
