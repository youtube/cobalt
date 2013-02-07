// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/scoped_loop_observer.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

namespace media {

ScopedLoopObserver::ScopedLoopObserver(
    const scoped_refptr<base::MessageLoopProxy>& loop)
    : loop_(loop) {
  ObserveLoopDestruction(true, NULL);
}

ScopedLoopObserver::~ScopedLoopObserver() {
  ObserveLoopDestruction(false, NULL);
}

void ScopedLoopObserver::ObserveLoopDestruction(bool enable,
                                                base::WaitableEvent* done) {
  // Note: |done| may be NULL.
  if (loop_->BelongsToCurrentThread()) {
    MessageLoop* loop = MessageLoop::current();
    if (enable) {
      loop->AddDestructionObserver(this);
    } else {
      loop->RemoveDestructionObserver(this);
    }
  } else {
    base::WaitableEvent event(false, false);
    if (loop_->PostTask(FROM_HERE,
            base::Bind(&ScopedLoopObserver::ObserveLoopDestruction,
                       base::Unretained(this), enable, &event))) {
      event.Wait();
    } else {
      // The message loop's thread has already terminated, so no need to wait.
    }
  }

  if (done)
    done->Signal();
}

}  // namespace media.
