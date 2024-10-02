// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_prediction_waiter.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"

namespace password_manager {

PasswordFormPredictionWaiter::PasswordFormPredictionWaiter(Client* client)
    : client_(client) {}

PasswordFormPredictionWaiter::~PasswordFormPredictionWaiter() = default;

void PasswordFormPredictionWaiter::StartTimer() {
  // Unretained |this| is safe because the timer will be destroyed on
  // destruction of this object.
  timer_.Start(FROM_HERE, kMaxFillingDelayForAsyncPredictions, this,
               &PasswordFormPredictionWaiter::OnTimeout);
}

base::OnceClosure PasswordFormPredictionWaiter::CreateClosure() {
  outstanding_closures_++;
  return base::BindOnce(&PasswordFormPredictionWaiter::OnClosureComplete,
                        weak_ptr_factory_.GetWeakPtr());
}

void PasswordFormPredictionWaiter::OnTimeout() {
  // The barrier closure remains active. OnWaitCompleted() will still be called
  // if the callbacks occur ever the timeout.
  client_->OnTimeout();
}

void PasswordFormPredictionWaiter::OnClosureComplete() {
  DCHECK(outstanding_closures_ > 0);
  outstanding_closures_--;

  if (outstanding_closures_ == 0) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    timer_.Stop();
    client_->OnWaitCompleted();
    return;
  }

  // If the timer has already expired, this should notify the client even when
  // there are other outstanding closures. This is the reason we can't use
  // a BarrierClosure here, even though this code mostly replicates how one
  // works.
  if (!timer_.IsRunning()) {
    client_->OnWaitCompleted();
  }
}

}  // namespace password_manager
