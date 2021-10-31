// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "base/trace_event/trace_event.h"
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
    DCHECK(embedder_field.second == WrapperPrivate::kInternalFieldIdValue);
  }
}

void V8cHeapTracer::TracePrologue(TraceFlags flags) {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::TracePrologue");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (disabled_) {
    return;
  }
  DCHECK_EQ(frontier_.size(), 0);
  DCHECK_EQ(visited_.size(), 0);
}

bool V8cHeapTracer::AdvanceTracing(double deadline_in_ms) {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::AdvanceTracing");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  double start_time = platform_->MonotonicallyIncreasingTime();
  if (disabled_) {
    return true;
  }

  // This feels a bit weird, but as far as I can tell, we're expected to
  // manually decide to trace the from the global object.
  MaybeAddToFrontier(
      V8cGlobalEnvironment::GetFromIsolate(isolate_)->global_wrappable());

  // Objects that we want to keep alive.
  for (Traceable* traceable : roots_) {
    MaybeAddToFrontier(traceable);
  }
  for (v8::TracedGlobal<v8::Value>* traced_global : globals_) {
    RegisterEmbedderReference(traced_global->As<v8::Data>());
  }

  while (platform_->MonotonicallyIncreasingTime() - start_time <
         deadline_in_ms) {
    if (frontier_.empty()) {
      return false;
    }

    Traceable* traceable = frontier_.back();
    frontier_.pop_back();
    DCHECK(traceable);

    if (traceable->IsWrappable()) {
      Wrappable* wrappable = base::polymorphic_downcast<Wrappable*>(traceable);
      auto pair_range = reference_map_.equal_range(wrappable);
      for (auto it = pair_range.first; it != pair_range.second; ++it) {
        // Tell v8 this object is referenced on Cobalt heap.
        RegisterEmbedderReference(it->second->traced_global().As<v8::Data>());
      }
      WrapperFactory* wrapper_factory =
          V8cGlobalEnvironment::GetFromIsolate(isolate_)->wrapper_factory();
      WrapperPrivate* maybe_wrapper_private =
          wrapper_factory->MaybeGetWrapperPrivate(
              static_cast<Wrappable*>(traceable));
      if (maybe_wrapper_private) {
        RegisterEmbedderReference(maybe_wrapper_private->traced_global().As<v8::Data>());
      }
    }

    traceable->TraceMembers(this);
  }

  return true;
}

bool V8cHeapTracer::IsTracingDone() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (disabled_) {
    return true;
  }
  return frontier_.empty();
}

void V8cHeapTracer::TraceEpilogue(TraceSummary* trace_summary) {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::TraceEpilogue");

  if (disabled_) {
    return;
  }
  DCHECK(frontier_.empty());
  visited_.clear();
}

void V8cHeapTracer::EnterFinalPause(EmbedderStackState stack_state) {
  TRACE_EVENT0("cobalt::script", "V8cHeapTracer::EnterFinalPause");
}

void V8cHeapTracer::Trace(Traceable* traceable) {
  DCHECK(!disabled_);
  MaybeAddToFrontier(traceable);
}

void V8cHeapTracer::AddReferencedObject(Wrappable* owner,
                                        ScopedPersistent<v8::Value>* value) {
  DCHECK(owner);
  DCHECK(value);
  auto it = reference_map_.insert({owner, value});
}

void V8cHeapTracer::RemoveReferencedObject(Wrappable* owner,
                                           ScopedPersistent<v8::Value>* value) {
  DCHECK(owner);
  DCHECK(value);
  auto pair_range = reference_map_.equal_range(owner);
  auto it = std::find_if(
      pair_range.first, pair_range.second,
      [&](decltype(*pair_range.first) it) { return it.second == value; });
  DCHECK(it != pair_range.second);
  reference_map_.erase(it);
}

void V8cHeapTracer::AddRoot(Traceable* traceable) {
  DCHECK(traceable);
  roots_.insert(traceable);
}

void V8cHeapTracer::RemoveRoot(Traceable* traceable) {
  DCHECK(traceable);
  auto it = roots_.find(traceable);
  DCHECK(it != roots_.end());
  roots_.erase(it);
}

void V8cHeapTracer::AddRoot(v8::TracedGlobal<v8::Value>* traced_global) {
  DCHECK(traced_global);
  globals_.insert(traced_global);
}

void V8cHeapTracer::RemoveRoot(v8::TracedGlobal<v8::Value>* traced_global) {
  DCHECK(traced_global);
  auto it = globals_.find(traced_global);
  DCHECK(it != globals_.end());
  globals_.erase(it);
}

void V8cHeapTracer::MaybeAddToFrontier(Traceable* traceable) {
  if (!traceable) {
    return;
  }
  if (!visited_.insert(traceable).second) {
    return;
  }
  frontier_.push_back(traceable);
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
