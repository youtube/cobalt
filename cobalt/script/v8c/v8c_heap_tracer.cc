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
    wrapper_private->Mark();

    Wrappable* wrappable = wrapper_private->raw_wrappable();
    if (visited_.insert(wrappable).second) {
      frontier_.push_back(wrappable);
    }

    // We expect this field to always be null, since we only have it as a
    // workaround for V8.  See "wrapper_private.h" for details.
    DCHECK(embedder_field.second == nullptr);
  }
}

bool V8cHeapTracer::AdvanceTracing(double deadline_in_ms,
                                   AdvanceTracingActions actions) {
  while (actions.force_completion ==
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION ||
         platform_->MonotonicallyIncreasingTime() < deadline_in_ms) {
    if (frontier_.empty()) {
      return false;
    }

    Traceable* traceable = frontier_.back();
    frontier_.pop_back();
    traceable->TraceMembers(this);
  }

  return true;
}

void V8cHeapTracer::Trace(Traceable* traceable) {
  if (!traceable) {
    return;
  }

  // Attempt to add |traceable| to |visited_|.  Don't do any additional work
  // if we find that it has already been traced.
  if (!visited_.insert(traceable).second) {
    return;
  }

  // A |Traceable| that isn't a |Wrappable| cannot be traced by V8, so we will
  // add it to |frontier_| and trace it ourselves later.
  if (!traceable->IsWrappable()) {
    frontier_.push_back(traceable);
    return;
  }

  // Now that we know |Traceable| is a |Wrappable|, check and see if it has a
  // wrapper.  If it does, then let V8 know that it's reacheable, and wait for
  // V8 to give it back to us later via |RegisterV8References|.  If not, add
  // it to the frontier to trace ourselves later.
  Wrappable* wrappable = base::polymorphic_downcast<Wrappable*>(traceable);
  WrapperFactory* wrapper_factory =
      V8cGlobalEnvironment::GetFromIsolate(isolate_)->wrapper_factory();
  WrapperPrivate* maybe_wrapper_private =
      wrapper_factory->MaybeGetWrapperPrivate(wrappable);
  if (maybe_wrapper_private) {
    maybe_wrapper_private->Mark();
  } else {
    frontier_.push_back(wrappable);
  }
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt
