// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_controller.h"

#include <memory>
#include <ostream>

#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"

namespace dictation {

SessionController::SessionController(SessionControllerDelegate& delegate)
    : delegate_(delegate) {}

SessionController::~SessionController() {
  CHECK(state_ != State::kInactive || !attached_stream_provider_);
  if (state_ != State::kInactive) {
    EndDictationStream();
  }
}

void SessionController::Initialize() {
  ui_ = delegate_->CreateUi(*this);
}

void SessionController::StartDictationStream(Target& target) {
  CHECK_EQ(state_, State::kInactive);

  std::unique_ptr<StreamProvider> stream_provider =
      delegate_->CreateStreamProvider(*this);
  stream_provider->BindToTarget(target);
  attached_stream_provider_ = std::move(stream_provider);

  MoveToState(State::kStreamInitializing);
}

void SessionController::EndDictationStream() {
  CHECK_NE(state_, State::kInactive);
  attached_stream_provider_->Stop();
  attached_stream_provider_.reset();
  MoveToState(State::kInactive);
}

void SessionController::RequestEndSession() {
  // EndSession will destroy `this` which owns other objects that call into here
  // so PostTask to avoid destroying objects in the callstack.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SessionController> this_ptr) {
                       if (!this_ptr) {
                         return;
                       }
                       this_ptr->delegate_->EndSession();
                       CHECK(!this_ptr);
                     },
                     weak_ptr_factory_.GetWeakPtr()));
}

void SessionController::MoveToState(State new_state) {
  using enum State;
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>>
      allowed_transitions(base::StateTransitions<State>(
          {{kInactive, {kStreamInitializing}},
           {kStreamInitializing, {kInactive, kTranscribing}},
           {kTranscribing, {kInactive, kFinalizing}},
           {kFinalizing, {kInactive}}}));
  if (new_state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions, /*old_state=*/state_,
                            /*new_state=*/new_state);
  }
#endif  // DCHECK_IS_ON()
  state_ = new_state;
}

const char* ToString(SessionController::State state) {
  using enum SessionController::State;
  switch (state) {
    case kInactive:
      return "kInactive";
    case kStreamInitializing:
      return "kStreamInitializing";
    case kTranscribing:
      return "kTranscribing";
    case kFinalizing:
      return "kFinalizing";
  }
}

std::ostream& operator<<(std::ostream& out, SessionController::State state) {
  return out << ToString(state);
}

}  // namespace dictation
