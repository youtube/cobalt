// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_CALLBACK_HOLDER_H_
#define COBALT_MEDIA_BASE_CALLBACK_HOLDER_H_

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "cobalt/media/base/bind_to_current_loop.h"

namespace media {

// A helper class that can hold a callback from being fired.
template <typename CB>
class CallbackHolder {
 public:
  CallbackHolder() : hold_(false) {}

  ~CallbackHolder() {
    // Make sure all callbacks are satisfied!
    DCHECK(!hold_);
    DCHECK(original_cb_.is_null());
    DCHECK(held_cb_.is_null());
  }

  // Sets the callback to be potentially held.
  void SetCallback(const CB& cb) {
    DCHECK(original_cb_.is_null());
    DCHECK(held_cb_.is_null());
    original_cb_ = cb;
  }

  bool IsNull() const { return original_cb_.is_null() && held_cb_.is_null(); }

  // Holds the callback when Run() is called.
  void HoldCallback() { hold_ = true; }

  // Runs or holds the callback as specified by |hold_|.
  // This method has overloaded versions to support different types of CB.
  void RunOrHold() {
    DCHECK(held_cb_.is_null());
    if (hold_)
      held_cb_ = base::ResetAndReturn(&original_cb_);
    else
      base::ResetAndReturn(&original_cb_).Run();
  }

  template <typename A1>
  void RunOrHold(A1 a1) {
    DCHECK(held_cb_.is_null());
    if (hold_) {
      held_cb_ = base::Bind(base::ResetAndReturn(&original_cb_),
                            internal::TrampolineForward(a1));
    } else {
      base::ResetAndReturn(&original_cb_).Run(a1);
    }
  }

  template <typename A1, typename A2>
  void RunOrHold(A1 a1, A2 a2) {
    DCHECK(held_cb_.is_null());
    if (hold_) {
      held_cb_ = base::Bind(base::ResetAndReturn(&original_cb_),
                            internal::TrampolineForward(a1),
                            internal::TrampolineForward(a2));
    } else {
      base::ResetAndReturn(&original_cb_).Run(a1, a2);
    }
  }

  // Releases and runs the held callback.
  void RunHeldCallback() {
    DCHECK(hold_);
    DCHECK(!held_cb_.is_null());
    hold_ = false;
    base::ResetAndReturn(&held_cb_).Run();
  }

 private:
  bool hold_;
  CB original_cb_;
  base::Closure held_cb_;
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_CALLBACK_HOLDER_H_
