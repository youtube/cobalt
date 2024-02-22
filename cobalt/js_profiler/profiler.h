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

#include "base/threading/thread_checker.h"
#include "cobalt/dom/performance_high_resolution_time.h"
#include "cobalt/js_profiler/profiler_group.h"
#include "cobalt/js_profiler/profiler_init_options.h"
#include "cobalt/js_profiler/profiler_trace.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/event_target.h"
#include "third_party/v8/include/v8-profiler.h"

namespace cobalt {
namespace js_profiler {

// Forward declaration of ProfilerGroup
class ProfilerGroup;

// TODO(b/326337485): Profiler should be a subclass of EventTarget.
class Profiler : public script::Wrappable {
 public:
  static const int kBaseSampleIntervalMs = 10;
  typedef script::HandlePromiseWrappable ProfilerTracePromise;
  typedef script::CallbackFunction<void()> SampleBufferFullCallback;
  typedef script::ScriptValue<SampleBufferFullCallback>
      SampleBufferFullCallbackHolder;
  typedef SampleBufferFullCallbackHolder::Reference
      SampleBufferFullCallbackReference;

  Profiler(script::EnvironmentSettings* settings, ProfilerInitOptions options,
           script::ExceptionState* exception_state);
  ~Profiler() override = default;

  void AddEventListener(script::EnvironmentSettings* environment_settings,
                        const std::string& name,
                        const SampleBufferFullCallbackHolder& listener);

  void DispatchSampleBufferFullEvent();

  ProfilerTracePromise Stop(script::EnvironmentSettings* environment_settings);
  void Cancel();

  bool stopped() const { return stopped_; }

  dom::DOMHighResTimeStamp sample_interval() const { return sample_interval_; }
  std::string ProfilerId() const { return profiler_id_; }
  base::TimeTicks time_origin() const { return time_origin_; }

  DEFINE_WRAPPABLE_TYPE(Profiler);

 private:
  void PerformStop(ProfilerGroup* profiler_group,
                   std::unique_ptr<script::ValuePromiseWrappable::Reference>
                       promise_reference,
                   base::TimeTicks time_origin, std::string profiler_id);

  bool stopped_;
  dom::DOMHighResTimeStamp sample_interval_;
  base::TimeTicks time_origin_;
  std::string profiler_id_;
  ProfilerGroup* profiler_group_;
  // All samplebufferfull listeners. Prevents GC on callbacks owned by this
  // object, by binding to global-wrappable.
  std::unique_ptr<SampleBufferFullCallbackReference> listener_;

  // Thread checker for the thread that creates this instance.
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(Profiler);
};

}  // namespace js_profiler
}  // namespace cobalt
#endif  // COBALT_JS_PROFILER_PROFILER_H_
