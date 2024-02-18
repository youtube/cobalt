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


#include "cobalt/js_profiler/profiler.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "cobalt/js_profiler/profiler_trace_builder.h"
#include "cobalt/js_profiler/profiler_trace_wrapper.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings_helper.h"
#include "starboard/common/log.h"

namespace {
void OnGetArgument() {}
}  // namespace

namespace cobalt {
namespace js_profiler {

Profiler::Profiler(script::EnvironmentSettings* settings,
                   ProfilerInitOptions options,
                   script::ExceptionState* exception_state)
    : ALLOW_THIS_IN_INITIALIZER_LIST(listeners_(this)),
      stopped_(false),
      time_origin_{base::TimeTicks::Now()} {
  profiler_group_ = ProfilerGroup::From(settings);
  profiler_id_ = profiler_group_->NextProfilerId();

  const base::TimeDelta sample_interval =
      base::Milliseconds(options.sample_interval());

  int64_t sample_interval_us = sample_interval.InMicroseconds();

  if (sample_interval_us < 0 ||
      sample_interval_us > std::numeric_limits<int>::max()) {
    sample_interval_us = 0;
  }

  int effective_sample_interval_ms =
      static_cast<int>(sample_interval.InMilliseconds());
  if (effective_sample_interval_ms % Profiler::kBaseSampleIntervalMs != 0 ||
      effective_sample_interval_ms == 0) {
    effective_sample_interval_ms +=
        (Profiler::kBaseSampleIntervalMs -
         effective_sample_interval_ms % Profiler::kBaseSampleIntervalMs);
  }
  sample_interval_ = effective_sample_interval_ms;

  SB_LOG(INFO) << "[PROFILER] START " + profiler_id_;
  auto status = profiler_group_->ProfilerStart(
      this, settings,
      v8::CpuProfilingOptions(v8::kLeafNodeLineNumbers,
                              options.max_buffer_size(), sample_interval_us));

  if (status == v8::CpuProfilingStatus::kAlreadyStarted) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             "Profiler Already started", exception_state);
  } else if (status == v8::CpuProfilingStatus::kErrorTooManyProfilers) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             "Too Many Profilers", exception_state);
  }
}

void Profiler::AddEventListener(
    const std::string& name,
    const Profiler::SampleBufferFullCallbackHolder& callback_holder) {
  if (name != base::Tokens::samplebufferfull()) {
    return;
  }
  listeners_.AddListener(callback_holder);
}

void Profiler::DispatchSampleBufferFullEvent() {
  listeners_.DispatchEvent(base::Bind(OnGetArgument));
}

Profiler::ProfilerTracePromise Profiler::Stop(
    script::EnvironmentSettings* environment_settings) {
  SB_LOG(INFO) << "[PROFILER] STOPPING " + profiler_id_;
  script::HandlePromiseWrappable promise =
      web::get_script_value_factory(environment_settings)
          ->CreateInterfacePromise<scoped_refptr<ProfilerTraceWrapper>>();
  if (!stopped()) {
    stopped_ = true;
    auto* global_wrappable = web::get_global_wrappable(environment_settings);
    auto* context = web::get_context(environment_settings);
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference(
        new script::ValuePromiseWrappable::Reference(global_wrappable,
                                                     promise));

    context->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&Profiler::PerformStop, base::Unretained(this),
                       profiler_group_, std::move(promise_reference),
                       std::move(time_origin_), std::move(profiler_id_)));
  } else {
    promise->Reject(new web::DOMException(web::DOMException::kInvalidStateErr,
                                          "Profiler already stopped."));
  }
  return promise;
}

void Profiler::PerformStop(
    ProfilerGroup* profiler_group,
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    base::TimeTicks time_origin, std::string profiler_id) {
  SB_LOG(INFO) << "[PROFILER] STOPPED " + profiler_id_;
  auto profile = profiler_group->ProfilerStop(this);
  auto trace = ProfilerTraceBuilder::FromProfile(profile, time_origin_);
  scoped_refptr<ProfilerTraceWrapper> result(new ProfilerTraceWrapper(trace));
  promise_reference->value().Resolve(result);
  if (profile) {
    profile->Delete();
  }
}

void Profiler::Cancel() {
  if (!stopped_) {
    auto profile = profiler_group_->ProfilerStop(this);
    profile->Delete();
  }
  profiler_group_ = nullptr;
}

}  // namespace js_profiler
}  // namespace cobalt
