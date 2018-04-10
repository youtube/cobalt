// Copyright 2017 Google Inc. All Rights Reserved.
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

  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) override;
  void TracePrologue() override;
  bool AdvanceTracing(double deadline_in_ms,
                      AdvanceTracingActions actions) override;
  void TraceEpilogue() override;
  void EnterFinalPause() override;
  void AbortTracing() override;
  size_t NumberOfWrappersToTrace() override;

  void Trace(Traceable* traceable) override;

  void AddReferencedObject(Wrappable* owner,
                           ScopedPersistent<v8::Value>* value);
  void RemoveReferencedObject(Wrappable* owner,
                              ScopedPersistent<v8::Value>* value);

  void AddRoot(Traceable* traceable);
  void RemoveRoot(Traceable* traceable);

 private:
  void MaybeAddToFrontier(Traceable* traceable);

  v8::Isolate* const isolate_;
  v8::Platform* const platform_ = IsolateFellowship::GetInstance()->platform;

  std::vector<Traceable*> frontier_;
  std::unordered_set<Traceable*> visited_;
  std::unordered_multimap<Wrappable*, ScopedPersistent<v8::Value>*>
      reference_map_;

  // TODO: A "counted" multiset approach here would be a bit nicer than
  // std::multiset.
  std::unordered_multiset<Traceable*> roots_;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_
