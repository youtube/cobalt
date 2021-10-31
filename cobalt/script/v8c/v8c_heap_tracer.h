// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_
#define COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/threading/thread_checker.h"
#include "cobalt/script/v8c/isolate_fellowship.h"
#include "cobalt/script/v8c/scoped_persistent.h"
#include "cobalt/script/wrappable.h"
#include "v8/include/v8-platform.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cHeapTracer final : public v8::EmbedderHeapTracer,
                            public ::cobalt::script::Tracer {
 public:
  explicit V8cHeapTracer(v8::Isolate* isolate) : isolate_(isolate) {}

  // V8 EmbedderHeapTracer API
  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) override;
  void TracePrologue(TraceFlags) override;
  bool AdvanceTracing(double deadline_in_ms) override;
  bool IsTracingDone() override;
  void TraceEpilogue(TraceSummary* trace_summary) override;
  void EnterFinalPause(EmbedderStackState stack_state) override;
  // IsRootForNonTracingGC provides an opportunity for us to get quickly
  // perished reference deleted in scavenger GCs. But that requires the ability
  // to determine whether a v8 object is reference by anything in Cobalt heap.
  // Cobalt does have the reference_map_ that tracks all ScriptValues but Cobalt
  // does not track referencers of all wrappables yet. So we don't have the
  // ability to exploit this feature yet.

  // bool IsRootForNonTracingGC(const v8::TracedGlobal<v8::Value>& handle)
  // override

  // Cobalt Tracer API
  void Trace(Traceable* traceable) override;

  void AddReferencedObject(Wrappable* owner,
                           ScopedPersistent<v8::Value>* value);
  void RemoveReferencedObject(Wrappable* owner,
                              ScopedPersistent<v8::Value>* value);

  void AddRoot(Traceable* traceable);
  void RemoveRoot(Traceable* traceable);

  void AddRoot(v8::TracedGlobal<v8::Value>* traced_global);
  void RemoveRoot(v8::TracedGlobal<v8::Value>* traced_global);

  // Used during shutdown to ask V8cHeapTracer do nothing so that V8 can
  // GC every embedder-created object.
  void DisableForShutdown() {
    disabled_ = true;
  }

 private:
  void MaybeAddToFrontier(Traceable* traceable);

  v8::Isolate* const isolate_;
  v8::Platform* const platform_ =
      IsolateFellowship::GetInstance()->platform.get();

  THREAD_CHECKER(thread_checker_);

  std::vector<Traceable*> frontier_;
  // Traceables from frontier_ are also considered global objects,
  // globals_ are the ones that hold no member references.
  std::unordered_set<Traceable*> visited_;
  std::unordered_multimap<Wrappable*, ScopedPersistent<v8::Value>*>
      reference_map_;

  // TODO: A "counted" multiset approach here would be a bit nicer than
  // std::multiset.
  std::unordered_multiset<Traceable*> roots_;
  std::unordered_multiset<v8::TracedGlobal<v8::Value>*> globals_;

  bool disabled_ = false;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_
