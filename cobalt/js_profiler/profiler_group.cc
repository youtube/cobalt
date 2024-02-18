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

#include "cobalt/js_profiler/profiler_group.h"

#include "cobalt/web/context.h"
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

ProfilerGroup* ProfilerGroup::From(
    script::EnvironmentSettings* environment_settings) {
  web::Context* context = web::get_context(environment_settings);
  if (!context->profiler_group()) {
    script::GlobalEnvironment* global_env =
        web::get_global_environment(environment_settings);
    context->set_profiler_group(new ProfilerGroup(global_env->isolate()));
  }
  return context->profiler_group();
}

v8::CpuProfilingStatus ProfilerGroup::ProfilerStart(
    const scoped_refptr<Profiler>& profiler,
    script::EnvironmentSettings* settings, v8::CpuProfilingOptions options) {
  if (!cpu_profiler_) {
    cpu_profiler_ = v8::CpuProfiler::New(isolate_);
    cpu_profiler_->SetSamplingInterval(
        cobalt::js_profiler::Profiler::kBaseSampleIntervalMs *
        base::Time::kMicrosecondsPerMillisecond);
  }
  profilers_.push_back(profiler);
  num_active_profilers_++;
  return cpu_profiler_->StartProfiling(
      toV8String(isolate_, profiler->ProfilerId()), options,
      std::make_unique<ProfilerMaxSamplesDelegate>(this,
                                                   profiler->ProfilerId()));
}

v8::CpuProfile* ProfilerGroup::ProfilerStop(Profiler* profiler) {
  auto profile = cpu_profiler_->StopProfiling(
      toV8String(isolate_, profiler->ProfilerId()));
  this->PopProfiler(profiler->ProfilerId());
  if (cpu_profiler_ && num_active_profilers_ == 0) {
    cpu_profiler_->Dispose();
    cpu_profiler_ = nullptr;
  }
  return profile;
}

std::string ProfilerGroup::NextProfilerId() {
  auto id = "cobalt::profiler[" + std::to_string(next_profiler_id_) + "]";
  next_profiler_id_++;
  return id;
}

void ProfilerGroup::DispatchSampleBufferFullEvent(std::string profiler_id) {
  auto profiler = GetProfiler(profiler_id);

  if (profiler) {
    SB_LOG(INFO) << "[PROFILER] DISPATCHING FULL " + profiler->ProfilerId();
    profiler->DispatchSampleBufferFullEvent();
    SB_LOG(INFO) << "[PROFILER] DISPATCHED FULL " + profiler->ProfilerId();
  }
}

Profiler* ProfilerGroup::GetProfiler(std::string profiler_id) {
  auto profiler =
      std::find_if(profilers_.begin(), profilers_.end(),
                   [&profiler_id](const scoped_refptr<Profiler>& profiler) {
                     return profiler->ProfilerId() == profiler_id;
                   });
  if (profiler == profilers_.end()) {
    return nullptr;
  }
  return profiler->get();
}

void ProfilerGroup::PopProfiler(std::string profiler_id) {
  auto profiler = std::find_if(profilers_.begin(), profilers_.end(),
                               [&profiler_id](const Profiler* profiler) {
                                 return profiler->ProfilerId() == profiler_id;
                               });
  if (profiler != profilers_.end()) {
    profilers_.erase(profiler);
  }
  num_active_profilers_--;
}

void ProfilerGroup::WillDestroyCurrentMessageLoop() {
  while (!profilers_.empty()) {
    Profiler* profiler = profilers_[0];
    DCHECK(profiler);
    profiler->Cancel();
    DCHECK(profiler->stopped());
  }

  DCHECK_EQ(num_active_profilers_, 0);
  if (cpu_profiler_) {
    cpu_profiler_->Dispose();
    cpu_profiler_ = nullptr;
  }
}

void ProfilerMaxSamplesDelegate::Notify() {
  if (profiler_group_.Get()) {
    profiler_group_->DispatchSampleBufferFullEvent(profiler_id_);
  }
}

}  // namespace js_profiler
}  // namespace cobalt
