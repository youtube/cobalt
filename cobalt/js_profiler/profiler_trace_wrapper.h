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

#ifndef COBALT_JS_PROFILER_PROFILER_TRACE_WRAPPER_H_
#define COBALT_JS_PROFILER_PROFILER_TRACE_WRAPPER_H_

#include <string>

#include "cobalt/js_profiler/profiler_trace.h"

namespace cobalt {
namespace js_profiler {
class ProfilerTraceWrapper : public script::Wrappable {
 public:
  DEFINE_WRAPPABLE_TYPE(ProfilerTraceWrapper);
  explicit ProfilerTraceWrapper(ProfilerTrace trace) {
    resources_ = trace.resources();
    frames_ = trace.frames();
    stacks_ = trace.stacks();
    samples_ = trace.samples();
  }
  ProfilerTraceWrapper() {}
  script::Sequence<std::string> resources() const { return resources_; }
  script::Sequence<ProfilerFrame> frames() const { return frames_; }
  script::Sequence<ProfilerStack> stacks() const { return stacks_; }
  script::Sequence<ProfilerSample> samples() const { return samples_; }

 private:
  script::Sequence<std::string> resources_;
  script::Sequence<ProfilerFrame> frames_;
  script::Sequence<ProfilerStack> stacks_;
  script::Sequence<ProfilerSample> samples_;
};
}  // namespace js_profiler
}  // namespace cobalt

#endif  // COBALT_JS_PROFILER_PROFILER_TRACE_WRAPPER_H_
