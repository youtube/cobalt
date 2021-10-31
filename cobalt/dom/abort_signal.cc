// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/abort_signal.h"

#include "cobalt/dom/event.h"

namespace cobalt {
namespace dom {

// https://dom.spec.whatwg.org/#abortsignal-follow
void AbortSignal::Follow(const scoped_refptr<AbortSignal>& parent_signal) {
  // 1. If followingSignal's aborted flag is set, then return.
  if (aborted_) {
    return;
  }

  if (!parent_signal) {
    return;
  }

  // 2. If parentSignal's aborted flag is set, then signal abort on
  //    followingSignal.
  if (parent_signal->aborted()) {
    SignalAbort();
    return;
  }

  // 3. Otherwise, add the following abort steps to parentSignal:
  //    a. Signal abort on followingSignal.
  parent_signal->following_signals_.emplace_back(this);
}

// https://dom.spec.whatwg.org/#abortsignal-signal-abort
void AbortSignal::SignalAbort() {
  // 1. If signal's aborted flag is set, then return.
  if (aborted_) {
    return;
  }

  // 2. Set signal's aborted flag.
  aborted_ = true;

  // 3. For each algorithm in signal's abort algorithms: run algorithm.
  for (auto& following_signal : following_signals_) {
    following_signal->SignalAbort();
  }

  // 4. Empty signal's abort algorithms.
  following_signals_.clear();

  // 5. Fire an event named abort at signal.
  DispatchEvent(new Event(base::Tokens::abort()));
}

void AbortSignal::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(following_signals_);
}

}  // namespace dom
}  // namespace cobalt
