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

#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/js_profiler/profiler_trace_builder.h"
#include "cobalt/js_profiler/profiler_trace_wrapper.h"
#include "cobalt/web/cache_utils.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/environment_settings_helper.h"

namespace {
v8::Local<v8::String> toV8String(v8::Isolate* isolate,
                                 const std::string& string) {
  if (string.empty()) return v8::String::Empty(isolate);
  return v8::String::NewFromUtf8(isolate, string.c_str(),
                                 v8::NewStringType::kNormal, string.length())
      .ToLocalChecked();
}
}  // namespace

namespace cobalt {
namespace js_profiler {

volatile uint32_t s_lastProfileId = 0;

static constexpr int kBaseSampleIntervalMs = 10;

Profiler::Profiler(script::EnvironmentSettings* settings,
                   ProfilerInitOptions options,
                   script::ExceptionState* exception_state)
    : cobalt::web::EventTarget(settings),
      stopped_(false),
      time_origin_{base::TimeTicks::Now()} {
  profiler_id_ = nextProfileId();

  const base::TimeDelta sample_interval =
      base::Milliseconds(options.sample_interval());

  int64_t sample_interval_us = sample_interval.InMicroseconds();

  if (sample_interval_us < 0 ||
      sample_interval_us > std::numeric_limits<int>::max()) {
    sample_interval_us = 0;
  }

  int effective_sample_interval_ms =
      static_cast<int>(sample_interval.InMilliseconds());
  if (effective_sample_interval_ms % kBaseSampleIntervalMs != 0 ||
      effective_sample_interval_ms == 0) {
    effective_sample_interval_ms +=
        (kBaseSampleIntervalMs -
         effective_sample_interval_ms % kBaseSampleIntervalMs);
  }
  sample_interval_ = effective_sample_interval_ms;

  auto isolate = web::get_isolate(settings);

  auto status = ImplProfilingStart(
      profiler_id_,
      v8::CpuProfilingOptions(v8::kLeafNodeLineNumbers,
                              options.max_buffer_size(), sample_interval_us),
      settings);

  if (status == v8::CpuProfilingStatus::kAlreadyStarted) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             "Profiler Already started", exception_state);
  } else if (status == v8::CpuProfilingStatus::kErrorTooManyProfilers) {
    web::DOMException::Raise(web::DOMException::kInvalidStateErr,
                             "Too Many Profilers", exception_state);
  }
}

Profiler::~Profiler() {
  if (cpu_profiler_) {
    cpu_profiler_->Dispose();
    cpu_profiler_ = nullptr;
  }
}

v8::CpuProfilingStatus Profiler::ImplProfilingStart(
    std::string profiler_id, v8::CpuProfilingOptions options,
    script::EnvironmentSettings* settings) {
  auto isolate = web::get_isolate(settings);
  cpu_profiler_ = v8::CpuProfiler::New(isolate);
  cpu_profiler_->SetSamplingInterval(kBaseSampleIntervalMs *
                                     base::Time::kMicrosecondsPerMillisecond);
  return cpu_profiler_->StartProfiling(
      toV8String(isolate, profiler_id), options,
      std::make_unique<ProfilerMaxSamplesDelegate>(this));
}

std::string Profiler::nextProfileId() {
  s_lastProfileId++;
  return "cobalt::profiler[" + std::to_string(s_lastProfileId) + "]";
}

void Profiler::PerformStop(
    script::EnvironmentSettings* environment_settings,
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    base::TimeTicks time_origin, std::string profiler_id) {
  auto isolate = web::get_isolate(environment_settings);
  auto profile =
      cpu_profiler_->StopProfiling(toV8String(isolate, profiler_id_));
  auto trace = ProfilerTraceBuilder::FromProfile(profile, time_origin_);
  scoped_refptr<ProfilerTraceWrapper> result(new ProfilerTraceWrapper(trace));
  cpu_profiler_->Dispose();
  cpu_profiler_ = nullptr;
  promise_reference->value().Resolve(result);
}

Profiler::ProfilerTracePromise Profiler::Stop(
    script::EnvironmentSettings* environment_settings) {
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
                       environment_settings, std::move(promise_reference),
                       std::move(time_origin_), std::move(profiler_id_)));
  } else {
    promise->Reject(new web::DOMException(web::DOMException::kInvalidStateErr,
                                          "Profiler already stopped."));
  }
  return promise;
}

}  // namespace js_profiler
}  // namespace cobalt
