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

ProfilerGroupFactory* ProfilerGroupFactory::GetInstance() {
  return base::Singleton<
      ProfilerGroupFactory,
      base::StaticMemorySingletonTraits<ProfilerGroupFactory> >::get();
}

ProfilerGroup* ProfilerGroupFactory::getOrCreateProfilerGroup(
    v8::Isolate* isolate) {
  auto it = profiler_group_per_isolate_map_.find(isolate);
  if (it != profiler_group_per_isolate_map_.end()) {
    return it->second;
  }
  auto profiler_group = new ProfilerGroup(isolate);
  profiler_group_per_isolate_map_[isolate] = profiler_group;
  return profiler_group;
}

ProfilerGroup* ProfilerGroup::From(v8::Isolate* isolate) {
  auto factory = ProfilerGroupFactory::GetInstance();
  return factory->getOrCreateProfilerGroup(isolate);
}

v8::CpuProfilingStatus ProfilerGroup::ProfilerStart(
    Profiler* profiler, script::GlobalEnvironment* global_env,
    v8::CpuProfilingOptions options) {
  if (!cpu_profiler_) {
    cpu_profiler_ = v8::CpuProfiler::New(isolate_);
    cpu_profiler_->SetSamplingInterval(
        cobalt::js_profiler::Profiler::kBaseSampleIntervalMs *
        base::Time::kMicrosecondsPerMillisecond);
  }
  profilers_.push_back(profiler);
  num_active_profilers_++;
  profiler->PreventGarbageCollection(global_env);
  return cpu_profiler_->StartProfiling(
      toV8String(isolate_, profiler->ProfilerId()), options,
      std::make_unique<ProfilerMaxSamplesDelegate>(this,
                                                   profiler->ProfilerId()));
}

v8::CpuProfile* ProfilerGroup::ProfilerStop(Profiler* profiler) {
  auto profile = cpu_profiler_->StopProfiling(
      toV8String(isolate_, profiler->ProfilerId()));
  profiler->AllowGarbageCollection();
  this->PopProfiler(profiler->ProfilerId());
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
    profiler->DispatchEvent(new web::Event("samplebufferfull"));
  }
}

Profiler* ProfilerGroup::GetProfiler(std::string profiler_id) {
  auto profiler = std::find_if(profilers_.begin(), profilers_.end(),
                               [&profiler_id](const Profiler* profiler) {
                                 return profiler->ProfilerId() == profiler_id;
                               });
  if (profiler == profilers_.end()) {
    return nullptr;
  }
  return *profiler;
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

void ProfilerMaxSamplesDelegate::Notify() {
  if (profiler_group_.Get()) {
    profiler_group_->DispatchSampleBufferFullEvent(profiler_id_);
  }
}

}  // namespace js_profiler
}  // namespace cobalt
