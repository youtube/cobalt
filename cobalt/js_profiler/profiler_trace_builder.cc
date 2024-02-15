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

#include "cobalt/js_profiler/profiler_trace_builder.h"

#include "base/time/time.h"
#include "cobalt/dom/performance.h"
#include "cobalt/js_profiler/profiler_frame.h"
#include "cobalt/js_profiler/profiler_sample.h"
#include "cobalt/js_profiler/profiler_stack.h"
#include "cobalt/js_profiler/profiler_trace.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace js_profiler {

ProfilerTrace ProfilerTraceBuilder::FromProfile(const v8::CpuProfile* profile,
                                                base::TimeTicks time_origin) {
  ProfilerTraceBuilder builder(time_origin);
  if (profile) {
    for (int i = 0; i < profile->GetSamplesCount(); i++) {
      const auto* node = profile->GetSample(i);
      auto timestamp = base::TimeTicks() +
                       base::Microseconds(profile->GetSampleTimestamp(i));
      builder.AddSample(node, timestamp);
    }
  }
  return builder.GetTrace();
}

ProfilerTraceBuilder::ProfilerTraceBuilder(base::TimeTicks time_origin)
    : time_origin_(time_origin) {}

void ProfilerTraceBuilder::AddSample(const v8::CpuProfileNode* node,
                                     base::TimeTicks timestamp) {
  ProfilerSample sample;

  auto relative_timestamp =
      dom::Performance::MonotonicTimeToDOMHighResTimeStamp(time_origin_,
                                                           timestamp);

  sample.set_timestamp(relative_timestamp);
  absl::optional<uint64_t> stack_id = GetOrInsertStackId(node);
  if (stack_id.has_value()) sample.set_stack_id(stack_id.value());

  samples_.push_back(sample);
}

absl::optional<uint64_t> ProfilerTraceBuilder::GetOrInsertStackId(
    const v8::CpuProfileNode* node) {
  if (!node) return absl::nullopt;

  if (!ShouldIncludeStackFrame(node))
    return GetOrInsertStackId(node->GetParent());

  auto existing_stack_id = node_to_stack_map_.find(node);
  if (existing_stack_id != node_to_stack_map_.end()) {
    // If we found a stack entry for this node ID, the subpath to the root
    // already exists in the trace, and we may coalesce.
    return existing_stack_id->second;
  }

  ProfilerStack stack;
  uint64_t frame_id = GetOrInsertFrameId(node);
  stack.set_frame_id(frame_id);
  absl::optional<int> parent_stack_id = GetOrInsertStackId(node->GetParent());
  if (parent_stack_id.has_value()) stack.set_parent_id(parent_stack_id.value());

  uint64_t stack_id = stacks_.size();
  stacks_.push_back(stack);
  node_to_stack_map_[node] = stack_id;
  return stack_id;
}

uint64_t ProfilerTraceBuilder::GetOrInsertFrameId(
    const v8::CpuProfileNode* node) {
  auto existing_frame_id = node_to_frame_map_.find(node);

  if (existing_frame_id != node_to_frame_map_.end())
    return existing_frame_id->second;

  ProfilerFrame frame;
  std::string function_name(node->GetFunctionNameStr());
  frame.set_name(function_name);
  if (*node->GetScriptResourceNameStr() != '\0') {
    uint64_t resource_id =
        GetOrInsertResourceId(node->GetScriptResourceNameStr());
    frame.set_resource_id(resource_id);
  }
  if (node->GetLineNumber() != v8::CpuProfileNode::kNoLineNumberInfo)
    frame.set_line(node->GetLineNumber());
  if (node->GetColumnNumber() != v8::CpuProfileNode::kNoColumnNumberInfo)
    frame.set_column(node->GetColumnNumber());

  uint64_t frame_id = frames_.size();
  frames_.push_back(frame);
  node_to_frame_map_[node] = frame_id;

  return frame_id;
}

uint64_t ProfilerTraceBuilder::GetOrInsertResourceId(
    const char* resource_name) {
  auto existing_resource_id = resource_indices_.find(resource_name);

  if (existing_resource_id != resource_indices_.end())
    return existing_resource_id->second;

  uint64_t resource_id = resources_.size();
  resources_.push_back(resource_name);

  resource_indices_[resource_name] = resource_id;

  return resource_id;
}

ProfilerTrace ProfilerTraceBuilder::GetTrace() const {
  ProfilerTrace trace;
  trace.set_resources(resources_);
  trace.set_frames(frames_);
  trace.set_stacks(stacks_);
  trace.set_samples(samples_);
  return trace;
}

bool ProfilerTraceBuilder::ShouldIncludeStackFrame(
    const v8::CpuProfileNode* node) {
  DCHECK(node);

  // Omit V8 metadata frames.
  const v8::CpuProfileNode::SourceType source_type = node->GetSourceType();
  if (source_type != v8::CpuProfileNode::kScript &&
      source_type != v8::CpuProfileNode::kBuiltin &&
      source_type != v8::CpuProfileNode::kCallback) {
    return false;
  }

  // Attempt to attribute each stack frame to a script.
  // - For JS functions, this is their own script.
  // - For builtins, this is the first attributable caller script.
  const v8::CpuProfileNode* resource_node = node;
  if (source_type != v8::CpuProfileNode::kScript) {
    while (resource_node &&
           resource_node->GetScriptId() == v8::UnboundScript::kNoScriptId) {
      resource_node = resource_node->GetParent();
    }
  }
  if (!resource_node) return false;

  int script_id = resource_node->GetScriptId();

  // If we already tested whether or not this script was cross-origin, return
  // the cached results.
  auto it = script_same_origin_cache_.find(script_id);
  if (it != script_same_origin_cache_.end()) return it->second;
  // insert in pair script_same_origin_cache_ (script_id, true)
  script_same_origin_cache_[script_id] = true;
  return true;
}
}  // namespace js_profiler
}  // namespace cobalt
