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

#ifndef COBALT_JS_PROFILER_PROFILER_H_
#define COBALT_JS_PROFILER_PROFILER_H_

#include <memory>
#include <string>

#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/js_profiler/profiler_init_options.h"
#include "cobalt/js_profiler/profiler_trace.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "third_party/v8/include/cppgc/member.h"
#include "third_party/v8/include/v8-profiler.h"

namespace cobalt {
namespace js_profiler {

class Profiler : public cobalt::web::EventTarget {
 public:
  using ProfilerTracePromise = script::HandlePromiseWrappable;

  Profiler(script::EnvironmentSettings* settings, ProfilerInitOptions options,
           script::ExceptionState* exception_state);
  ~Profiler();

  ProfilerTracePromise Stop(script::EnvironmentSettings* environment_settings);

  bool stopped() const { return stopped_; }

  dom::DOMHighResTimeStamp sample_interval() const { return sample_interval_; }

  DEFINE_WRAPPABLE_TYPE(Profiler);

  virtual v8::CpuProfilingStatus ImplProfilingStart(
      std::string profiler_id, v8::CpuProfilingOptions options,
      script::EnvironmentSettings* settings);

 private:
  void PerformStop(script::EnvironmentSettings* environment_settings,
                   std::unique_ptr<script::ValuePromiseWrappable::Reference>
                       promise_reference,
                   base::TimeTicks time_origin, std::string profiler_id);

  std::string nextProfileId();

  bool stopped_;
  dom::DOMHighResTimeStamp sample_interval_;
  v8::CpuProfiler* cpu_profiler_ = nullptr;
  base::TimeTicks time_origin_;
  std::string profiler_id_;
};

class ProfilerMaxSamplesDelegate : public v8::DiscardedSamplesDelegate {
 public:
  explicit ProfilerMaxSamplesDelegate(Profiler* profiler)
      : profiler_(profiler) {}
  void Notify() override {
    if (profiler_.Get()) {
      profiler_->DispatchEvent(new web::Event("samplebufferfull"));
    }
  }

 private:
  cppgc::WeakMember<Profiler> profiler_;
};

}  // namespace js_profiler
}  // namespace cobalt
#endif  // COBALT_JS_PROFILER_PROFILER_H_
