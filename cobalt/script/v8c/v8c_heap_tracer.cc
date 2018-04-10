// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/script/v8c/v8c_heap_tracer.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cobalt/script/v8c/v8c_engine.h"
#include "cobalt/script/v8c/v8c_global_environment.h"
#include "cobalt/script/v8c/wrapper_private.h"

namespace cobalt {
namespace script {
namespace v8c {

void V8cHeapTracer::RegisterV8References(
    const std::vector<std::pair<void*, void*>>& embedder_fields) {
  for (const auto& embedder_field : embedder_fields) {
    WrapperPrivate* wrapper_private =
        static_cast<WrapperPrivate*>(embedder_field.first);
    Wrappable* wrappable = wrapper_private->raw_wrappable();
    MaybeAddToFrontier(wrappable);

    // We expect this field to always be null, since we only have it as a
    // workaround for V8.  See "wrapper_private.h" for details.
    DCHECK(embedder_field.second == nullptr);
  }
}

void V8cHeapTracer::TracePrologue() {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::TracePrologue");

  DCHECK_EQ(frontier_.size(), 0);
  DCHECK_EQ(visited_.size(), 0);

  // This feels a bit weird, but as far as I can tell, we're expected to
  // manually decide to trace the from the global object.
  MaybeAddToFrontier(
      V8cGlobalEnvironment::GetFromIsolate(isolate_)->global_wrappable());

  for (Traceable* traceable : roots_) {
    MaybeAddToFrontier(traceable);
  }
}

bool V8cHeapTracer::AdvanceTracing(double deadline_in_ms,
                                   AdvanceTracingActions actions) {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::AdvanceTracing");

  while (actions.force_completion ==
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION ||
         platform_->MonotonicallyIncreasingTime() < deadline_in_ms) {
    if (frontier_.empty()) {
      return false;
    }

    Traceable* traceable = frontier_.back();
    frontier_.pop_back();

    if (traceable->IsWrappable()) {
      Wrappable* wrappable = base::polymorphic_downcast<Wrappable*>(traceable);
      auto pair_range = reference_map_.equal_range(wrappable);
      for (auto it = pair_range.first; it != pair_range.second; ++it) {
        it->second->Get().RegisterExternalReference(isolate_);
      }
      WrapperFactory* wrapper_factory =
          V8cGlobalEnvironment::GetFromIsolate(isolate_)->wrapper_factory();
      WrapperPrivate* maybe_wrapper_private =
          wrapper_factory->MaybeGetWrapperPrivate(
              static_cast<Wrappable*>(traceable));
      if (maybe_wrapper_private) {
        maybe_wrapper_private->Mark();
      }
    }

    traceable->TraceMembers(this);
  }

  return true;
}

void V8cHeapTracer::TraceEpilogue() {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::TraceEpilogue");

  DCHECK(frontier_.empty());
  visited_.clear();
}

void V8cHeapTracer::EnterFinalPause() {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::EnterFinalPause");
}

void V8cHeapTracer::AbortTracing() {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::AbortTracing");

  LOG(WARNING) << "Tracing aborted.";
  frontier_.clear();
  visited_.clear();
}

size_t V8cHeapTracer::NumberOfWrappersToTrace() { return frontier_.size(); }

void V8cHeapTracer::Trace(Traceable* traceable) {
  if (!traceable) {
    return;
  }
  MaybeAddToFrontier(traceable);
}

void V8cHeapTracer::AddReferencedObject(Wrappable* owner,
                                        ScopedPersistent<v8::Value>* value) {
  auto it = reference_map_.insert({owner, value});
}

void V8cHeapTracer::RemoveReferencedObject(Wrappable* owner,
                                           ScopedPersistent<v8::Value>* value) {
  auto pair_range = reference_map_.equal_range(owner);
  auto it = std::find_if(
      pair_range.first, pair_range.second,
      [&](decltype(*pair_range.first) it) { return it.second == value; });
  DCHECK(it != pair_range.second);
  reference_map_.erase(it);
}

void V8cHeapTracer::AddRoot(Traceable* traceable) { roots_.insert(traceable); }

void V8cHeapTracer::RemoveRoot(Traceable* traceable) {
  roots_.erase(traceable);
}

void V8cHeapTracer::MaybeAddToFrontier(Traceable* traceable) {
  if (!visited_.insert(traceable).second) {
    return;
  }
  frontier_.push_back(traceable);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
