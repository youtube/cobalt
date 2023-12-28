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

#ifndef COBALT_JS_PROFILER_PROFILER_TRACE_BUILDER_H_
#define COBALT_JS_PROFILER_PROFILER_TRACE_BUILDER_H_

#include <map>
#include <string>

#include "base/time/time.h"
#include "cobalt/script/sequence.h"
#include "third_party/chromium/media/cobalt/third_party/abseil-cpp/absl/types/optional.h"
#include "v8/include/v8-profiler.h"

namespace cobalt {
namespace js_profiler {

class ProfilerFrame;
class ProfilerSample;
class ProfilerStack;
class ProfilerTrace;

class ProfilerTraceBuilder {
 public:
  static ProfilerTrace FromProfile(const v8::CpuProfile* profile,
                                   base::TimeTicks time_origin);

  explicit ProfilerTraceBuilder(base::TimeTicks time_origin);

  ProfilerTraceBuilder(const ProfilerTraceBuilder&) = delete;
  ProfilerTraceBuilder& operator=(const ProfilerTraceBuilder&) = delete;

 private:
  // Adds a stack sample from V8 to the trace, performing necessary filtering
  // and coalescing.
  void AddSample(const v8::CpuProfileNode* node, base::TimeTicks timestamp);

  // Obtains the stack ID of the substack with the given node as its leaf,
  // performing origin-based filtering.
  absl::optional<uint64_t> GetOrInsertStackId(const v8::CpuProfileNode* node);

  // Obtains the frame ID of the stack frame represented by the given node.
  uint64_t GetOrInsertFrameId(const v8::CpuProfileNode* node);

  // Obtains the resource ID for the given resource name.
  uint64_t GetOrInsertResourceId(const char* resource_name);

  ProfilerTrace GetTrace() const;

  // Discards metadata frames and performs an origin check on the given stack
  // frame, returning true if it either has the same origin as the profiler, or
  // if it should be shared cross origin.
  bool ShouldIncludeStackFrame(const v8::CpuProfileNode* node);

  base::TimeTicks time_origin_;

  script::Sequence<std::string> resources_;
  script::Sequence<ProfilerFrame> frames_;
  script::Sequence<ProfilerStack> stacks_;
  script::Sequence<ProfilerSample> samples_;

  // Maps V8-managed resource strings to their indices in the resources table.
  std::map<const char*, uint64_t> resource_indices_;
  std::map<const v8::CpuProfileNode*, uint64_t> node_to_stack_map_;
  std::map<const v8::CpuProfileNode*, uint64_t> node_to_frame_map_;

  std::map<int, bool> script_same_origin_cache_;
};
}  // namespace js_profiler
}  // namespace cobalt
#endif  // COBALT_JS_PROFILER_PROFILER_TRACE_BUILDER_H_
