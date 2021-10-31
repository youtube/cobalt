// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/animation_frame_request_callback_list.h"

#include "base/trace_event/trace_event.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/dom/global_stats.h"

namespace cobalt {
namespace dom {

int32 AnimationFrameRequestCallbackList::RequestAnimationFrame(
    const FrameRequestCallbackArg& frame_request_callback) {
  // Wrap the frame request callback so that we can associate it with a
  // "cancelled" flag.
  // Push it into our vector of frame requests and return its 1-based position
  // ((to ensure a > 0 handle value, as required by the specification) in that
  // vector as the handle.
  frame_request_callbacks_.emplace_back(
      new FrameRequestCallbackWithCancelledFlag(owner_,
                                                frame_request_callback));
  debugger_hooks_.AsyncTaskScheduled(
      frame_request_callbacks_.back().get(), "requestAnimationFrame",
      base::DebuggerHooks::AsyncTaskFrequency::kOneshot);
  return static_cast<int32>(frame_request_callbacks_.size());
}

void AnimationFrameRequestCallbackList::CancelAnimationFrame(int32 in_handle) {
  // If the handle is valid, set the "cancelled" flag on the specified
  // frame request callback.
  const size_t handle = static_cast<size_t>(in_handle);
  if (handle > 0 && handle <= frame_request_callbacks_.size()) {
    auto& callback = frame_request_callbacks_.at(handle - 1);
    debugger_hooks_.AsyncTaskCanceled(callback.get());
    callback->cancelled = true;
  }
}

void AnimationFrameRequestCallbackList::RunCallbacks(double animation_time) {
  TRACE_EVENT1("cobalt::dom", "Window::RunAnimationFrameCallbacks()",
               "number_of_callbacks", frame_request_callbacks_.size());

  // The callbacks are now being run. Track it in the global stats.
  GlobalStats::GetInstance()->StartJavaScriptEvent();

  for (InternalList::const_iterator iter = frame_request_callbacks_.begin();
       iter != frame_request_callbacks_.end(); ++iter) {
    if (!(*iter)->cancelled) {
      base::ScopedAsyncTask async_task(debugger_hooks_, iter->get());
      (*iter)->callback.value().Run(animation_time);
    }
  }

  // The callbacks are done running. Stop tracking it in the global stats.
  GlobalStats::GetInstance()->StopJavaScriptEvent();
}

bool AnimationFrameRequestCallbackList::HasPendingCallbacks() const {
  return !frame_request_callbacks_.empty();
}

}  // namespace dom
}  // namespace cobalt
