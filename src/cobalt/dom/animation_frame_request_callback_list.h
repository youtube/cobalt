/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DOM_ANIMATION_FRAME_REQUEST_CALLBACK_LIST_H_
#define COBALT_DOM_ANIMATION_FRAME_REQUEST_CALLBACK_LIST_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_object.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// Manages a list of frame request callbacks as well as their "cancelled" state.
// Associates a handle to each frame frequest that is unique to this list.
//   https://www.w3.org/TR/animation-timing/#dfn-animation-frame-request-callback-list
class AnimationFrameRequestCallbackList {
 public:
  typedef script::CallbackFunction<void(double)> FrameRequestCallback;
  typedef script::ScriptObject<FrameRequestCallback> FrameRequestCallbackArg;

  explicit AnimationFrameRequestCallbackList(script::Wrappable* const owner)
      : owner_(owner) {}

  int32 RequestAnimationFrame(
      const FrameRequestCallbackArg& frame_request_callback);

  void CancelAnimationFrame(int32 handle);

  // Call each animation frame callback in the order they were requested.
  void RunCallbacks(double animation_time);

 private:
  // We define a wrapper structure for the frame request so that we can
  // associate a "cancelled" flag with each callback.
  struct FrameRequestCallbackWithCancelledFlag {
    FrameRequestCallbackWithCancelledFlag(script::Wrappable* const owner,
                                          const FrameRequestCallbackArg& cb)
        : callback(owner, cb), cancelled(false) {}

    FrameRequestCallbackArg::Reference callback;
    bool cancelled;
  };
  typedef ScopedVector<FrameRequestCallbackWithCancelledFlag> InternalList;

  script::Wrappable* const owner_;
  // Our list of frame request callbacks.
  InternalList frame_request_callbacks_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_ANIMATION_FRAME_REQUEST_CALLBACK_LIST_H_
